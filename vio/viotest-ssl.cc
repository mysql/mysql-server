#include	"all.h"

#include	<sys/types.h>
#include	<sys/socket.h>
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
	printf("viossltest: testing SSL virtual IO. Usage:\n");
	printf("viossltest server-key server-cert client-key client-cert [CAfile] [CApath]\n");
}

int
main(	int	argc,
	char**	argv)
{
	char*	server_key = 0;
	char*	server_cert = 0;
	char*	client_key = 0;
	char*	client_cert = 0;
	char*	ca_file = 0;
	char*	ca_path = 0;
	int	sv[2];

	if (argc<5)
	{
		print_usage();
		return 1;
	}

	if (socketpair(PF_UNIX, SOCK_STREAM, IPPROTO_IP, sv)==-1)
		fatal_error("socketpair");

	server_key = argv[1];
	server_cert = argv[2];
	client_key = argv[3];
	client_cert = argv[4];
	if (argc>5)
		ca_file = argv[5];
	if (argc>6)
		ca_path = argv[6];
	printf("Server key/cert : %s/%s\n", server_key, server_cert);
	printf("Client key/cert : %s/%s\n", client_key, client_cert);
	if (ca_file!=0)
		printf("CAfile          : %s\n", ca_file);
	if (ca_path!=0)
		printf("CApath          : %s\n", ca_path);

	VIO_NS::VioSSLAcceptorFd*	ssl_acceptor = new VIO_NS::VioSSLAcceptorFd(server_key, server_cert, ca_file, ca_path);
	VIO_NS::VioSSLConnectorFd*	ssl_connector = new VIO_NS::VioSSLConnectorFd(client_key, client_cert, ca_file, ca_path);

	printf("Socketpair: %d , %d\n", sv[0], sv[1]);

	VIO_NS::VioSSL*	client_vio = ssl_connector->connect(sv[0]);
	VIO_NS::VioSSL* server_vio = ssl_acceptor->accept(sv[1]);


	int child_pid = fork();
	if (child_pid==-1) {
		delete ssl_acceptor;
		delete ssl_connector;
		fatal_error("fork");
	}
	if (child_pid==0) {
		//child, therefore, client
		char	xbuf[100];
		int	r = client_vio->read(xbuf, sizeof(xbuf));
		if (r<=0) {
			delete ssl_acceptor;
			delete ssl_connector;
			fatal_error("client:SSL_read");
		}
        printf("*** client cipher %s\n",client_vio->cipher_description());
		xbuf[r] = 0;
		printf("client:got %s\n", xbuf);
		delete client_vio;
		delete ssl_acceptor;
		delete ssl_connector;
		sleep(1);
	} else {
		const char*	s = "Huhuhuh";
		int		r = server_vio->write((void *)s, strlen(s));
		if (r<=0) {
			delete ssl_acceptor;
			delete ssl_connector;
			fatal_error("server:SSL_write");
		}
        printf("*** server cipher %s\n",server_vio->cipher_description());
		delete server_vio;
		delete ssl_acceptor;
		delete ssl_connector;
		sleep(1);
	}
}
