/* N.B.:  The Alpha, under OSF/1, does *not* use size_t for the length,
   or for the returned values from readv and writev.  */

struct iovec {
  void *iov_base;
  int   iov_len;
};

/* I'm assuming the iovec structures are const.  I haven't verified
   it.  */
extern ssize_t readv (int, const struct iovec *, int);
extern ssize_t writev (int, const struct iovec *, int);
