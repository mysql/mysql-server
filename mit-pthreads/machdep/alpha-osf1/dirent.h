struct dirent {
  ino_t d_ino;
  ushort_t d_reclen, d_namlen;
  char d_name[256];
};
#define d_fileno d_ino
#define MAXNAMLEN 256
