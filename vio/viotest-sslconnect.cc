
/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

#include	"all.h"

#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<stdio.h>
#include	<unistd.h>


void
fatal_error(	const char*	r)
{
	perror(r);
	exit(0);
}

void
print_usage()
{
	printf("viotest-sslconnect: testing SSL virtual IO. Usage:\n");
	printf("viotest-sslconnect key cert\n");
}

int
main(	int	argc,
	char**	argv)
{
	char*	key = 0;
	char*	cert = 0;

	if (argc<3)
	{
		print_usage();
		return 1;
	}

	char		ip[4] = {127, 0, 0, 1};
	unsigned long	addr = (unsigned long)
			((unsigned long)ip[0]<<24L)|
			((unsigned long)ip[1]<<16L)|
			((unsigned long)ip[2]<< 8L)|
			((unsigned long)ip[3]);
	int	fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd<0)
		fatal_error("socket");
	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_port=htons(4433);
	sa.sin_addr.s_addr=htonl(addr);
	int	sa_size = sizeof sa;
	if (connect(fd, reinterpret_cast<const sockaddr*>(&sa), sa_size)==-1)
		fatal_error("connect");
	key = argv[1];
	cert = argv[2];
	printf("Key  : %s\n", key);
	printf("Cert : %s\n", cert);

	VIO_NS::VioSSLConnectorFd*	ssl_connector = new VIO_NS::VioSSLConnectorFd(cert, key,0,0);

	VIO_NS::VioSSL*	vio = ssl_connector->connect(fd);

	char	xbuf[100];
	int	r = vio->read(xbuf, sizeof(xbuf));
	if (r<=0) {
		delete ssl_connector;
		delete vio;
		fatal_error("client:SSL_read");
	}
	xbuf[r] = 0;
	printf("client:got %s\n", xbuf);
	delete vio;
	delete ssl_connector;
	return 0;
}
