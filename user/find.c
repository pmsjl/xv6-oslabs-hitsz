#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *name) {
  struct dirent de;
  struct stat st;
  int fd;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find cannot open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }
  char *p = path + strlen(path);
  while (p > path && *(p - 1) != '/') {
    p--;
  }
  if (strcmp(p, name) == 0) {
    printf("%s\n", path);
  }

  if (st.type == T_FILE) {
    close(fd);
    return;
  }

  if (st.type == T_DIR) {
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) {
        continue;
      }
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
        continue;
      }
      char buff[512];
      char *p = buff;
      strcpy(buff, path);
      p += strlen(path);
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buff, name);
    }
    close(fd);
    return;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("find needs two arguments!\n");
    exit(1);
  }
  char *path = argv[1];
  char *name = argv[2];
  find(path, name);
  exit(0);
}
