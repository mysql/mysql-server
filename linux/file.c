#include <unistd.h>

static ssize_t (*t_pwrite)(int, const void *, size_t, off_t) = 0;
static ssize_t (*t_write)(int, const void *, size_t) = 0;

int toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

int toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    return 0;
}


ssize_t
toku_os_pwrite (int fd, const void *buf, size_t len, off_t off)
{
    if (t_pwrite) {
	return t_pwrite(fd, buf, len, off);
    } else {
	return pwrite(fd, buf, len, off);
    }
}
