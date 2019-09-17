#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

void print_dir_entries(const char *path, char *buf, int fd, struct stat *st);

char *
fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--);
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void
ls(char *path) {
  char buf[512];
  int fd;
  struct stat st;

  if(*path == '-' && *(path + 1) == 's')
    return;

  if ((fd = open(path, 0)) < 0) {
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
      printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf(1, "ls: path too long\n");
        break;
      }
      print_dir_entries(path, buf, fd, &st);
      break;

    case T_DEV:
      if (st.dev_t == DEV_DIR) {
        print_dir_entries(path, buf, fd, &st);
      } else {
        printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
      }

      break;
  }
  close(fd);
}

void print_dir_entries(const char *path, char *buf, int fd, struct stat *st) {
  struct dirent d;
  struct dirent *de = &d;
  char *p;

  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';

  while (read(fd, de, sizeof((*de))) == sizeof((*de))) {
    if ((*de).inum == 0)
      continue;

    memmove(p, (*de).name, DIRSIZ);
    p[DIRSIZ] = 0;

    if (stat(buf, st) < 0) {
      printf(1, "ls: cannot stat %s\n", buf);
      continue;
    }

    printf(1, "%s %d %d %d\n", fmtname(buf), (*st).type, (*st).ino, (*st).size);
  }
}

int
main(int argc, char *argv[]) {
  int i;

  if (argc < 2) {
    ls(".");
    exit();
  }
  for (i = 1; i < argc; i++)
    ls(argv[i]);
  exit();
}
