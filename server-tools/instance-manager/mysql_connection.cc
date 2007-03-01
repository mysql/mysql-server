/* Copyright (C) 2004-2006 MySQL AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "mysql_connection.h"

#include <m_string.h>
#include <m_string.h>
#include <my_global.h>
#include <mysql.h>
#include <my_sys.h>
#include <violite.h>

#include "command.h"
#include "log.h"
#include "messages.h"
#include "mysqld_error.h"
#include "mysql_manager_error.h"
#include "parse.h"
#include "priv.h"
#include "protocol.h"
#include "thread_registry.h"
#include "user_map.h"


Mysql_connection::Mysql_connection(Thread_registry *thread_registry_arg,
                                   User_map *user_map_arg,
                                   struct st_vio *vio_arg, ulong
                                   connection_id_arg)
  :vio(vio_arg),
  connection_id(connection_id_arg),
  thread_registry(thread_registry_arg),
  user_map(user_map_arg)
{
}


/*
  NET subsystem requieres its user to provide my_net_local_init extern
  C function (exactly as declared below). my_net_local_init is called by
  my_net_init and is supposed to set NET controlling variables.
  See also priv.h for variables description.
*/

C_MODE_START

void my_net_local_init(NET *net)
{
  net->max_packet= net_buffer_length;
  net->read_timeout= net_read_timeout;
  net->write_timeout= net_write_timeout;
  net->retry_count= net_retry_count;
  net->max_packet_size= max_allowed_packet;
}

C_MODE_END


/*
  Every resource, which we can fail to acquire, is allocated in init().
  This function is complementary to cleanup().
*/

bool Mysql_connection::init()
{
  /* Allocate buffers for network I/O */
  if (my_net_init(&net, vio))
    return TRUE;

  net.return_status= &status;

  /* Initialize random number generator */
  {
    ulong seed1= (ulong) &rand_st + rand();
    ulong seed2= (ulong) rand() + (ulong) time(0);
    randominit(&rand_st, seed1, seed2);
  }

  /* Fill scramble - server's random message used for handshake */
  create_random_string(scramble, SCRAMBLE_LENGTH, &rand_st);

  /* We don't support transactions, every query is atomic */
  status= SERVER_STATUS_AUTOCOMMIT;

  thread_registry->register_thread(&thread_info);

  return FALSE;
}


void Mysql_connection::cleanup()
{
  net_end(&net);
  thread_registry->unregister_thread(&thread_info);
}


Mysql_connection::~Mysql_connection()
{
  /* vio_delete closes the socket if necessary */
  vio_delete(vio);
}


void Mysql_connection::main()
{
  log_info("Connection %lu: accepted.", (unsigned long) connection_id);

  if (check_connection())
  {
    log_info("Connection %lu: failed to authorize the user.",
            (unsigned long) connection_id);

    return;
  }

  log_info("Connection %lu: the user was authorized successfully.",
           (unsigned long) connection_id);

  vio_keepalive(vio, TRUE);

  while (!net.error && net.vio && !thread_registry->is_shutdown())
  {
    if (do_command())
      break;
  }
}


int Mysql_connection::check_connection()
{
  ulong pkt_len=0;                              // to hold client reply length

  /* buffer for the first packet */             /* packet contains: */
  char buff[MAX_VERSION_LENGTH + 1 +            // server version, 0-ended
            4 +                                 // connection id
            SCRAMBLE_LENGTH + 2 +               // scramble (in 2 pieces)
            18];                                // server variables: flags,
                                                // charset number, status,
  char *pos= buff;
  ulong server_flags;

  memcpy(pos, mysqlmanager_version.str, mysqlmanager_version.length + 1);
  pos+= mysqlmanager_version.length + 1;

  int4store((uchar*) pos, connection_id);
  pos+= 4;

  /*
    Old clients does not understand long scrambles, but can ignore packet
    tail: that's why first part of the scramble is placed here, and second
    part at the end of packet (even though we don't support old clients,
    we must follow standard packet format.)
  */
  memcpy(pos, scramble, SCRAMBLE_LENGTH_323);
  pos+= SCRAMBLE_LENGTH_323;
  *pos++= '\0';

  server_flags= CLIENT_LONG_FLAG | CLIENT_PROTOCOL_41 |
                CLIENT_SECURE_CONNECTION;

  /*
    18-bytes long section for various flags/variables

    Every flag occupies a bit in first half of ulong; int2store will
    gracefully pick up all flags.
  */
  int2store(pos, server_flags);
  pos+= 2;
  *pos++= (char) default_charset_info->number;  // global mysys variable
  int2store(pos, status);                       // connection status
  pos+= 2;
  bzero(pos, 13);                               // not used now
  pos+= 13;

  /* second part of the scramble, null-terminated */
  memcpy(pos, scramble + SCRAMBLE_LENGTH_323,
         SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1);
  pos+= SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1;

  /* write connection message and read reply */
  enum { MIN_HANDSHAKE_SIZE= 2 };
  if (net_write_command(&net, protocol_version, "", 0, buff, pos - buff) ||
     (pkt_len= my_net_read(&net)) == packet_error ||
      pkt_len < MIN_HANDSHAKE_SIZE)
  {
    net_send_error(&net, ER_HANDSHAKE_ERROR);
    return 1;
  }

  client_capabilities= uint2korr(net.read_pos);
  if (!(client_capabilities & CLIENT_PROTOCOL_41))
  {
    net_send_error_323(&net, ER_NOT_SUPPORTED_AUTH_MODE);
    return 1;
  }
  client_capabilities|= ((ulong) uint2korr(net.read_pos + 2)) << 16;

  pos= (char*) net.read_pos + 32;

  /* At least one byte for username and one byte for password */
  if (pos >= (char*) net.read_pos + pkt_len + 2)
  {
    /*TODO add user and password handling in error messages*/
    net_send_error(&net, ER_HANDSHAKE_ERROR);
    return 1;
  }

  const char *user= pos;
  const char *password= strend(user)+1;
  ulong password_len= *password++;
  LEX_STRING user_name= { (char *) user, password - user - 2 };

  if (password_len != SCRAMBLE_LENGTH)
  {
    net_send_error(&net, ER_ACCESS_DENIED_ERROR);
    return 1;
  }
  if (user_map->authenticate(&user_name, password, scramble))
  {
    net_send_error(&net, ER_ACCESS_DENIED_ERROR);
    return 1;
  }
  net_send_ok(&net, connection_id, NULL);
  return 0;
}


int Mysql_connection::do_command()
{
  char *packet;
  ulong packet_length;

  /* We start to count packets from 0 for each new command */
  net.pkt_nr= 0;

  if ((packet_length=my_net_read(&net)) == packet_error)
  {
    /* Check if we can continue without closing the connection */
    if (net.error != 3) // what is 3 - find out
      return 1;
    if (thread_registry->is_shutdown())
      return 1;
    net_send_error(&net, net.last_errno);
    net.error= 0;
    return 0;
  }
  else
  {
    if (thread_registry->is_shutdown())
      return 1;
    packet= (char*) net.read_pos;
    enum enum_server_command command= (enum enum_server_command)
                                      (uchar) *packet;
    log_info("Connection %lu: received packet (length: %lu; command: %d).",
             (unsigned long) connection_id,
             (unsigned long) packet_length,
             (int) command);

    return dispatch_command(command, packet + 1);
  }
}

int Mysql_connection::dispatch_command(enum enum_server_command command,
                                       const char *packet)
{
  switch (command) {
  case COM_QUIT:                                // client exit
    log_info("Connection %lu: received QUIT command.",
             (unsigned long) connection_id);
    return 1;

  case COM_PING:
    log_info("Connection %lu: received PING command.",
             (unsigned long) connection_id);
    net_send_ok(&net, connection_id, NULL);
    return 0;

  case COM_QUERY:
  {
    log_info("Connection %lu: received QUERY command: '%s'.",
	     (unsigned long) connection_id,
             (const char *) packet);

    if (Command *com= parse_command(packet))
    {
      int res= 0;

      log_info("Connection %lu: query parsed successfully.",
               (unsigned long) connection_id);

      res= com->execute(&net, connection_id);
      delete com;
      if (!res)
      {
        log_info("Connection %lu: query executed successfully",
                 (unsigned long) connection_id);
      }
      else
      {
        log_info("Connection %lu: can not execute query (error: %d).",
                 (unsigned long) connection_id,
                 (int) res);

        net_send_error(&net, res);
      }
    }
    else
    {
      log_error("Connection %lu: can not parse query: out ot resources.",
                (unsigned long) connection_id);

      net_send_error(&net,ER_OUT_OF_RESOURCES);
    }

    return 0;
  }

  default:
    log_info("Connection %lu: received unsupported command (%d).",
              (unsigned long) connection_id,
              (int) command);

    net_send_error(&net, ER_UNKNOWN_COM_ERROR);
    return 0;
  }

  return 0; /* Just to make compiler happy. */
}


void Mysql_connection::run()
{
  if (init())
    log_error("Connection %lu: can not init handler.",
              (unsigned long) connection_id);
  else
  {
    main();
    cleanup();
  }

  delete this;
}

/*
  vim: fdm=marker
*/
