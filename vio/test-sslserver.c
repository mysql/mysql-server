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

void
fatal_error(	const char*	r)
{
	perror(r);
	exit(0);
}

int
main(	int	argc,
	char**	argv)
{
	char*	server_key = 0,	*server_cert = 0;
	char*	ca_file = 0,	*ca_path = 0;
	struct st_VioSSLAcceptorFd* ssl_acceptor=0;
	const char*	s = "Huhuhuhuuu";


	struct sockaddr_in sa_serv;
	struct sockaddr_in sa_cli;
	int listen_sd;

	size_t client_len;
	int err;

	
	Vio* client_vio=0, *server_vio=0;
	MY_INIT(argv[0]);
        DBUG_PROCESS(argv[0]);
        DBUG_PUSH(default_dbug_option);

	server_key = "../SSL/server-key.pem";
	server_cert = "../SSL/server-cert.pem";
	ca_file = "../SSL/cacert.pem";

	printf("Server key/cert : %s/%s\n", server_key, server_cert);
	if (ca_file!=0)

		printf("CAfile          : %s\n", ca_file);
	if (ca_path!=0)
		printf("CApath          : %s\n", ca_path);


        ssl_acceptor = new_VioSSLAcceptorFd(server_key, server_cert, ca_file, ca_path);

	server_vio = (struct st_vio*)my_malloc(sizeof(struct st_vio),MYF(0));



  /* ----------------------------------------------- */
  /* Prepare TCP socket for receiving connections */

  listen_sd = socket (AF_INET, SOCK_STREAM, 0);   
  
  memset (&sa_serv, '\0', sizeof(sa_serv));
  sa_serv.sin_family      = AF_INET;
  sa_serv.sin_addr.s_addr = INADDR_ANY;
  sa_serv.sin_port        = htons (1111);          /* Server Port number */
  
  err = bind(listen_sd, (struct sockaddr*) &sa_serv,
	     sizeof (sa_serv));                  
	     
  /* Receive a TCP connection. */
	     
  err = listen (listen_sd, 5); 
  
  client_len = sizeof(sa_cli);
  server_vio->sd = accept (listen_sd, (struct sockaddr*) &sa_cli, &client_len);
  close (listen_sd);

  printf ("Connection from %lx, port %x\n",
	  sa_cli.sin_addr.s_addr, sa_cli.sin_port);
  
  /* ----------------------------------------------- */
  /* TCP connection is ready. Do server side SSL. */

 sslaccept(ssl_acceptor,server_vio);

{
err = server_vio->write(server_vio,(gptr)s, strlen(s));
if (err<=0) {
	my_free((gptr)ssl_acceptor,MYF(0));
	fatal_error("server:SSL_write");
}
}
my_free((gptr)server_vio,MYF(0));
my_free((gptr)ssl_acceptor,MYF(0));
}
#else /* HAVE_OPENSSL */

int main() {
return 0;
}
#endif /* HAVE_OPENSSL */

