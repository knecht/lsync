#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sysexits.h>

char* appendPath(const char* p1, const char* p2) {
  int p1len = strlen(p1);
  bool p1_ends_with_slash = p1[p1len-1] == '/';
  char* name = malloc(p1len + (p1_ends_with_slash ? 1 : 2) + strlen(p2));
  if (p1_ends_with_slash) {
    sprintf(name, "%s%s", p1, p2);
  } else {
    sprintf(name, "%s/%s", p1, p2);
  }
  return name;
}

#define fatal(fmt, ...) {fprintf(stderr, "\x1b[01;31m" fmt "\x1b[00m\n", __VA_ARGS__); exit(1);}

int mkdirs(const char* path, mode_t mode) {
  int len = strlen(path);
  char* parts = malloc(len+2);
  struct stat ds;
  int ret;
  for (int i=0;i<len+1;i++) {
    if (path[i] == 0 || path[i] == '/') {
      parts[i] = '/';
      parts[i+1] = 0;
      if (i > 0) {
        ret = lstat(parts, &ds);
        bool exists = ret == 0;
        if (!exists) {
          bool is_last_part = i >= len - 1;
          ret = mkdir(parts, is_last_part ? mode : 0700);
          if (ret != 0) {
            fatal("mkdir %s failed: %s", parts, strerror(errno));
          }
        }
      }
    } else {
      parts[i] = path[i];
    }
  }
  return ret;
}

void lsync(const char* srcPath, const char* destPath, unsigned char d_type) {
  assert(d_type != DT_UNKNOWN);
  struct stat ds;
  int ret = lstat(destPath, &ds);
  bool dest_exists = ret == 0;
  if (d_type == DT_DIR) {
    if (dest_exists) {
      assert(S_ISDIR(ds.st_mode));
    } else {
      struct stat ss;
      ret = lstat(srcPath, &ss);
      if (ret != 0) {
        perror(srcPath);
        exit(EX_NOINPUT);
      }
      ret = mkdir(destPath, ss.st_mode);
      if (ret == -1 && errno == ENOENT) {
        ret = mkdirs(destPath, ss.st_mode);
      }
      if (ret != 0) {
        fatal("mkdir %s failed: %s", destPath, strerror(errno));
      }
    }
    DIR* srcDir = opendir(srcPath);
    if (!srcDir) {
      perror(srcPath);
      exit(1);
    }
    assert(srcDir);
    struct dirent* srcDent;
    while (srcDent = readdir(srcDir)) {
      if (srcDent->d_name[0] == '.') {
        char c2 = srcDent->d_name[1];
        if (c2 == '.' || c2 == 0) continue;
      }
      char* subSrcPath = appendPath(srcPath, srcDent->d_name);

      char* subDestPath = appendPath(destPath, srcDent->d_name);
      lsync(subSrcPath, subDestPath, srcDent->d_type);
      free(subSrcPath);
      free(subDestPath);
    }
  } else {
    assert(d_type == DT_REG);
    if (dest_exists) {
      assert(S_ISREG(ds.st_mode));
    } else {
      ret = link(srcPath, destPath);
      if (ret != 0) {
        char msg[PATH_MAX*2+5];
        sprintf(msg, "%s -> %s", srcPath, destPath);
        perror(msg);
        abort();
      }
      assert(ret == 0);
    }
  }
}

int main(int argc, char** argv) {
  assert(argc == 3);
  lsync(argv[1], argv[2], DT_DIR);
}
