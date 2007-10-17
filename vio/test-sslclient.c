/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#ifdef HAVE_OPENSSL
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "errmsg.h"
#include <my_dir.h>
#include <my_getopt.h>
#include <signal.h>
#include <violite.h>

const char *VER="0.2";


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
main(	int	argc __attribute__((unused)),
	char**	argv)
{
	char	client_key[] = "../SSL/client-key.pem",	client_cert[] = "../SSL/client-cert.pem";
	char	ca_file[] = "../SSL/cacert.pem",	*ca_path = 0, *cipher=0;
	struct st_VioSSLFd* ssl_connector= 0;
	struct sockaddr_in sa;
	Vio* client_vio=0;
	int err;
	char	xbuf[100]="Ohohhhhoh1234";
	MY_INIT(argv[0]);
        DBUG_PROCESS(argv[0]);
        DBUG_PUSH(default_dbug_option);

	printf("Client key/cert : %s/%s\n", client_key, client_cert);
	if (ca_file!=0)
		printf("CAfile          : %s\n", ca_file);
	if (ca_path!=0)
		printf("CApath          : %s\n", ca_path);

	ssl_connector = new_VioSSLConnectorFd(client_key, client_cert, ca_file, ca_path, cipher);
	if(!ssl_connector) {
                 fatal_error("client:new_VioSSLConnectorFd failed");
	}

	/* ----------------------------------------------- */
	/* Create a socket and connect to server using normal socket calls. */

	client_vio = vio_new(socket (AF_INET, SOCK_STREAM, 0), VIO_TYPE_TCPIP, TRUE);

	memset (&sa, '\0', sizeof(sa));
	sa.sin_family      = AF_INET;
	sa.sin_addr.s_addr = inet_addr ("127.0.0.1");   /* Server IP */
	sa.sin_port        = htons     (1111);          /* Server Port number */

	err = connect(client_vio->sd, (struct sockaddr*) &sa,
		sizeof(sa));

	/* ----------------------------------------------- */
	/* Now we have TCP conncetion. Start SSL negotiation. */
	read(client_vio->sd,xbuf, sizeof(xbuf));
        sslconnect(ssl_connector,client_vio,60L);
	err = vio_read(client_vio,xbuf, sizeof(xbuf));
	if (err<=0) {
		my_free((uchar*)ssl_connector,MYF(0));
		fatal_error("client:SSL_read");
	}
	xbuf[err] = 0;
	printf("client:got %s\n", xbuf);
	my_free((uchar*)client_vio,MYF(0));
	my_free((uchar*)ssl_connector,MYF(0));
	return 0;
}
#else /* HAVE_OPENSSL */

int main() {
return 0;
}
#endif /* HAVE_OPENSSL */
