#include <global.h>
#ifdef HAVE_OPENSSL
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "errmsg.h"
#include <my_dir.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__               // Skip warnings in getopt.h
#endif
#include <getopt.h>
#include <signal.h>
#include <violite.h>

const char *VER="0.1";


#ifndef DBUG_OFF
const char *default_dbug_option="d:t:O,-";
#endif

static void
fatal_error(	const char*	r)
{
	perror(r);
	exit(0);
}

typedef struct {
	int	sd;
	struct	st_VioSSLAcceptorFd*	ssl_acceptor;
} TH_ARGS;

static void
do_ssl_stuff(	TH_ARGS*	args)
{
	const char*	s = "Huhuhuhuuu";
	Vio*		server_vio;
	int		err;
	DBUG_ENTER("do_ssl_stuff");

	server_vio = vio_new(args->sd, VIO_TYPE_TCPIP, TRUE);

	/* ----------------------------------------------- */
	/* TCP connection is ready. Do server side SSL. */

	err = write(server_vio->sd,(gptr)s, strlen(s));
	sslaccept(args->ssl_acceptor,server_vio);
	err = server_vio->write(server_vio,(gptr)s, strlen(s));
	DBUG_VOID_RETURN;
}

static void*
client_thread(	void*	arg)
{
	my_thread_init();
	do_ssl_stuff((TH_ARGS*)arg);
}

int
main(	int	argc __attribute__((unused)),
	char**	argv)
{
	char	server_key[] = "../SSL/server-key.pem",
		server_cert[] = "../SSL/server-cert.pem";
	char	ca_file[] = "../SSL/cacert.pem",
		*ca_path = 0;
	struct	st_VioSSLAcceptorFd*	ssl_acceptor;
	pthread_t	th;
	TH_ARGS		th_args;


	struct sockaddr_in sa_serv;
	struct sockaddr_in sa_cli;
	int listen_sd;
	int err;
	size_t client_len;
	int	reuseaddr = 1; /* better testing, uh? */
	
	MY_INIT(argv[0]);
        DBUG_PROCESS(argv[0]);
        DBUG_PUSH(default_dbug_option);

	printf("Server key/cert : %s/%s\n", server_key, server_cert);
	if (ca_file!=0)

		printf("CAfile          : %s\n", ca_file);
	if (ca_path!=0)
		printf("CApath          : %s\n", ca_path);

        th_args.ssl_acceptor = ssl_acceptor = new_VioSSLAcceptorFd(server_key, server_cert, ca_file, ca_path);

	/* ----------------------------------------------- */
	/* Prepare TCP socket for receiving connections */

	listen_sd = socket (AF_INET, SOCK_STREAM, 0);
	setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(&reuseaddr));
  
	memset (&sa_serv, '\0', sizeof(sa_serv));
	sa_serv.sin_family      = AF_INET;
	sa_serv.sin_addr.s_addr = INADDR_ANY;
	sa_serv.sin_port        = htons (1111);          /* Server Port number */
  
	err = bind(listen_sd, (struct sockaddr*) &sa_serv,
	     sizeof (sa_serv));                  
	     
	/* Receive a TCP connection. */
	     
	err = listen (listen_sd, 5); 
	client_len = sizeof(sa_cli);
	th_args.sd = accept (listen_sd, (struct sockaddr*) &sa_cli, &client_len);
	close (listen_sd);

	printf ("Connection from %lx, port %x\n",
		  (long)sa_cli.sin_addr.s_addr, sa_cli.sin_port);
  
	/* ----------------------------------------------- */
	/* TCP connection is ready. Do server side SSL. */

	err = pthread_create(&th, NULL, client_thread, (void*)&th_args);
	DBUG_PRINT("info", ("pthread_create: %d", err));
	pthread_join(th, NULL);

#if 0
	if (err<=0) {
		my_free((gptr)ssl_acceptor,MYF(0));
		fatal_error("server:SSL_write");
	}
#endif /* 0 */

	my_free((gptr)ssl_acceptor,MYF(0));
	return 0;
}
#else /* HAVE_OPENSSL */

int main() {
return 0;
}
#endif /* HAVE_OPENSSL */

