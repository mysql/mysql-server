/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

#include	"all.h"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdio.h>

#include	<string.h>

VIO_RCSID(vio, Vio, "$Id$")

VIO_NS_USING;

int
main(	int	argc,
	char**	argv)
{
	VioFd*	fs = 0;
	VioSocket*	ss = 0;
	int		fd = -1;
	char*		hh = "hshshsh\n";

	DBUG_ENTER("main");
	DBUG_PROCESS(argv[0]);
	DBUG_PUSH("d:t");

	fd = open("/dev/tty", O_WRONLY);
	if (fd<0)
	{
		perror("open");
		return 1;
	}
	fs = new VioFd(fd);
	ss = new VioSocket(fd);
	if (fs->write(hh,strlen(hh)) < 0)
		perror("write");
	ss->write(hh,strlen(hh));
	printf("peer_name:%s\n", ss->peer_name());
	printf("cipher_description:%s\n", ss->cipher_description());
	delete fs;
	delete ss;

	DBUG_RETURN(0);
}

