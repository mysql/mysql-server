#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#if !defined(_POSIX_SOURCE) 
#define MAXNAMLEN	255			/* maximum filename length */
#define DIRBUF		4096		/* buffer size for fs-indep. dirs */
#endif /* !defined(_POSIX_SOURCE) */ 

#include <sys/types.h>

struct dirent {							/* data from readdir() */
	ino_t			d_ino;				/* inode number of entry */
	off_t			d_off;				/* offset of disk direntory entry */
	unsigned short	d_reclen;			/* length of this record */
	char			d_name[MAXNAMLEN+1];/* name of file */
};

#define d_namlen	d_reclen
#define d_fileno	d_ino  

#endif /* _SYS_DIRENT_H */
