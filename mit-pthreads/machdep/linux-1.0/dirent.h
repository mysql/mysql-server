
#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <sys/types.h>
#include <linux/limits.h>

struct dirent {
	long			d_ino;
	off_t			d_off;
	unsigned short	d_reclen;
	char			d_name[NAME_MAX+1];
};

#ifndef	d_fileno
#define d_fileno	d_ino
#endif

#ifndef	d_namlen
#define	d_namlen	d_reclen
#endif

#ifndef MAXNAMLEN
#define MAXNAMLEN	NAME_MAX
#endif

#endif
