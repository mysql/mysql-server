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
	char*	client_key = 0,	*client_cert = 0;
	char*	ca_file = 0,	*ca_path = 0;
	struct st_VioSSLConnectorFd* ssl_connector=0; 
	Vio* client_vio=0;
	MY_INIT(argv[0]);
        DBUG_PROCESS(argv[0]);
        DBUG_PUSH(default_dbug_option);

	client_key = "../SSL/client-key.pem";
	client_cert = "../SSL/client-cert.pem";
	ca_file = "../SSL/cacert.pem";
	printf("Client key/cert : %s/%s\n", client_key, client_cert);
	if (ca_file!=0)
		printf("CAfile          : %s\n", ca_file);
	if (ca_path!=0)
		printf("CApath          : %s\n", ca_path);


	ssl_connector = new_VioSSLConnectorFd(client_key, client_cert, ca_file, ca_path);

	client_vio = (struct st_vio*)my_malloc(sizeof(struct st_vio),MYF(0));
	client_vio->vioblocking(client_vio,0);
        sslconnect(ssl_connector,client_vio);

	{
		char	xbuf[100];
		int	r = client_vio->read(client_vio,xbuf, sizeof(xbuf));
		if (r<=0) {
	      		my_free((gptr)ssl_connector,MYF(0));
			fatal_error("client:SSL_read");
		}
		xbuf[r] = 0;
		printf("client:got %s\n", xbuf);
		my_free((gptr)client_vio,MYF(0));
		my_free((gptr)ssl_connector,MYF(0));
	}
	return 0;
}
#else /* HAVE_OPENSSL */

int main() {
return 0;
}
#endif /* HAVE_OPENSSL */

