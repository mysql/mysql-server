/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
 mini MySQL client to be included into the server to do server to server
 commincation by Sasha Pachev

 Note: all file-global symbols must begin with mc_ , even the static ones, just
 in case we decide to make them external at some point
*/

#include <my_global.h>

#ifdef HAVE_EXTERNAL_CLIENT

/* my_pthread must be included early to be able to fix things */
#if defined(THREAD)
#include <my_pthread.h>				/* because of signal()	*/
#endif
#include <thr_alarm.h>
#include <mysql_embed.h>
#include <mysql_com.h>
#include <violite.h>
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "mini_client.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"
#include <assert.h>

#if defined( OS2) && defined(MYSQL_SERVER)
#undef  ER
#define ER CER
#endif

extern "C" {					// Because of SCO 3.2V4.2
#include <sys/stat.h>
#include <signal.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif /*!defined(MSDOS) && !defined(__WIN__) */
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif
}

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | \
                             CLIENT_LOCAL_FILES | CLIENT_SECURE_CONNECTION)


#if defined(MSDOS) || defined(__WIN__)
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif

extern ulong slave_net_timeout;
#define _mini_client_c
#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#include "../sql-common/client.c"

#endif /* HAVE_EXTERNAL_CLIENT */


