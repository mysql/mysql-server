/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <mysql_priv.h>
#include <my_dir.h>
#include "rpl_mi.h"

#ifdef HAVE_REPLICATION

enum {
  LINES_IN_MASTER_INFO_WITH_SSL= 14,

  /* 5.1.16 added value of master_ssl_verify_server_cert */
  LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT= 14,

  /* 5.5 added value of master_heartbeat_period */
  LINE_FOR_MASTER_HEARTBEAT_PERIOD= 16,

  /* 5.5 added value of master_ignore_server_id */
  LINE_FOR_REPLICATE_IGNORE_SERVER_IDS= 17,

  /* line for master_retry_count */
  LINE_FOR_MASTER_RETRY_COUNT= 18,

  /* Number of lines currently used when saving master info file */
  LINES_IN_MASTER_INFO= LINE_FOR_MASTER_RETRY_COUNT
};

/*
  Please every time you add a new field to the mater info, update
  what follows. For now, this is just used to get the number of
  fields.
*/
const char *info_mi_fields []=
{ 
  "lines",
  "master_log_name",
  "master_log_pos",
  "host",
  "user",
  "password",
  "port",
  "connect_retry",
  "ssl",
  "ssl_ca",
  "ssl_capath",
  "ssl_cert",
  "ssl_cipher",
  "ssl_key",
  "ssl_verify_server_cert",
  "heartbeat_period",
  "ignore_server_ids",
  "retry_count"
};

Master_info::Master_info()
   :Rpl_info("I/O"),
   ssl(0), ssl_verify_server_cert(0),
   port(MYSQL_PORT), connect_retry(DEFAULT_CONNECT_RETRY),
   heartbeat_period(min(SLAVE_MAX_HEARTBEAT_PERIOD, (slave_net_timeout/2.0))),
   received_heartbeats(0), master_id(0), retry_count(master_retry_count)
{
  host[0] = 0; user[0] = 0; password[0] = 0;
  ssl_ca[0]= 0; ssl_capath[0]= 0; ssl_cert[0]= 0;
  ssl_cipher[0]= 0; ssl_key[0]= 0;
  ignore_server_ids= new Server_ids();
}

Master_info::~Master_info()
{
  delete ignore_server_ids;
}

/**
   A comparison function to be supplied as argument to @c sort_dynamic()
   and @c bsearch()

   @return -1 if first argument is less, 0 if it equal to, 1 if it is greater
   than the second
*/
int change_master_server_id_cmp(ulong *id1, ulong *id2)
{
  return *id1 < *id2? -1 : (*id1 > *id2? 1 : 0);
}

/**
   Reports if the s_id server has been configured to ignore events 
   it generates with

      CHANGE MASTER IGNORE_SERVER_IDS= ( list of server ids )

   Method is called from the io thread event receiver filtering.

   @param      s_id    the master server identifier

   @retval   TRUE    if s_id is in the list of ignored master  servers,
   @retval   FALSE   otherwise.
 */
bool Master_info::shall_ignore_server_id(ulong s_id)
{
  if (likely(ignore_server_ids->server_ids.elements == 1))
    return (* (ulong*)
      dynamic_array_ptr(&(ignore_server_ids->server_ids), 0)) == s_id;
  else      
    return bsearch((const ulong *) &s_id,
                   ignore_server_ids->server_ids.buffer,
                   ignore_server_ids->server_ids.elements, sizeof(ulong),
                   (int (*) (const void*, const void*)) change_master_server_id_cmp)
      != NULL;
}

void Master_info::init_master_log_pos()
{
  DBUG_ENTER("Master_info::init_master_log_pos");

  master_log_name[0]= 0;
  master_log_pos= BIN_LOG_HEADER_SIZE;             // skip magic number

  /* Intentionally init ssl_verify_server_cert to 0, no option available  */
  ssl_verify_server_cert= 0;
  /* 
    always request heartbeat unless master_heartbeat_period is set
    explicitly zero.  Here is the default value for heartbeat period
    if CHANGE MASTER did not specify it.  (no data loss in conversion
    as hb period has a max)
  */
  heartbeat_period= (float) min(SLAVE_MAX_HEARTBEAT_PERIOD,
                                (slave_net_timeout/2.0));
  DBUG_ASSERT(heartbeat_period > (float) 0.001
              || heartbeat_period == 0);

  DBUG_VOID_RETURN;
}

void Master_info::end_info()
{
  DBUG_ENTER("Master_info::end_info");

  if (!inited)
    DBUG_VOID_RETURN;

  handler->end_info();

  inited = 0;

  DBUG_VOID_RETURN;
}

int Master_info::flush_info(bool force)
{
  DBUG_ENTER("Master_info::flush_info");
  DBUG_PRINT("enter",("master_pos: %ld", (long) master_log_pos));

  /*
    We update the sync_period at this point because only here we
    now that we are handling a master info. This needs to be
    update every time we call flush because the option maybe
    dinamically set.
  */
  handler->set_sync_period(sync_masterinfo_period);

  /*
     In certain cases this code may create master.info files that seems
     corrupted, because of extra lines filled with garbage in the end
     file (this happens if new contents take less space than previous
     contents of file). But because of number of lines in the first line
     of file we don't care about this garbage.
  */
  if (handler->prepare_info_for_write() ||
      handler->set_info((int) LINES_IN_MASTER_INFO) ||
      handler->set_info(master_log_name) ||
      handler->set_info(master_log_pos) ||
      handler->set_info(host) ||
      handler->set_info(user) ||
      handler->set_info(password) ||
      handler->set_info((int) port) ||
      handler->set_info((int) connect_retry) ||
      handler->set_info((int) ssl) ||
      handler->set_info(ssl_ca) ||
      handler->set_info(ssl_capath) ||
      handler->set_info(ssl_cert) ||
      handler->set_info(ssl_cipher) ||
      handler->set_info(ssl_key) ||
      handler->set_info(ssl_verify_server_cert) ||
      handler->set_info(heartbeat_period) ||
      handler->set_info(ignore_server_ids) ||
      handler->set_info(retry_count))
    goto err;

  if (handler->flush_info(force))
    goto err;

  DBUG_RETURN(0);

err:
  sql_print_error("Error writing master configuration");
  DBUG_RETURN(1);
}

void Master_info::set_relay_log_info(Relay_log_info* info)
{
  rli= info;
}

int Master_info::init_info()
{
  int lines;
  char *first_non_digit;

  DBUG_ENTER("Master_info::init_info");

  if (inited)
    DBUG_RETURN(0);

  /*
    The init_info() is used to either create or read information
    from the repository, in order to initialize the Master_info.
  */
  mysql= 0; file_id= 1;
  int necessary_to_configure= check_info();
  
  if (handler->init_info())
    goto err;

  if (necessary_to_configure)
  {
    init_master_log_pos();
    goto end;
  }

  /*
     Starting from 4.1.x master.info has new format. Now its
     first line contains number of lines in file. By reading this
     number we will be always distinguish to which version our
     master.info corresponds to. We can't simply count lines in
     file since versions before 4.1.x could generate files with more
     lines than needed.
     If first line doesn't contain a number or contain number less than
     LINES_IN_MASTER_INFO_WITH_SSL then such file is treated like file
     from pre 4.1.1 version.
     There is no ambiguity when reading an old master.info, as before
     4.1.1, the first line contained the binlog's name, which is either
     empty or has an extension (contains a '.'), so can't be confused
     with an integer.

     So we're just reading first line and trying to figure which version
     is this.
  */

  if (handler->prepare_info_for_read() || 
      handler->get_info(master_log_name, sizeof(master_log_name), ""))
    goto err;

  lines= strtoul(master_log_name, &first_non_digit, 10);

  if (master_log_name[0]!='\0' &&
      *first_non_digit=='\0' && lines >= LINES_IN_MASTER_INFO_WITH_SSL)
  {
    /* Seems to be new format => read master log name */
    if (handler->get_info(master_log_name,  sizeof(master_log_name), ""))
      goto err;
  }
  else 
    lines= 7;

  if (handler->get_info(&master_log_pos,
                        (my_off_t) BIN_LOG_HEADER_SIZE) ||
      handler->get_info(host, sizeof(host), 0) ||
      handler->get_info(user, sizeof(user), "test") ||
      handler->get_info(password, sizeof(password), 0) ||
      handler->get_info((int *) &port, (int) MYSQL_PORT) ||
      handler->get_info((int *) &connect_retry,
                        (int) DEFAULT_CONNECT_RETRY))
    goto err;

  /*
    If file has ssl part use it even if we have server without
    SSL support. But these options will be ignored later when
    slave will try connect to master, so in this case warning
    is printed.
  */
  if (lines >= LINES_IN_MASTER_INFO_WITH_SSL)
  {
    if (handler->get_info((int *) &ssl, 0) ||
        handler->get_info(ssl_ca, sizeof(ssl_ca), 0) ||
        handler->get_info(ssl_capath, sizeof(ssl_capath), 0) ||
        handler->get_info(ssl_cert, sizeof(ssl_cert), 0) ||
        handler->get_info(ssl_cipher, sizeof(ssl_cipher), 0) ||
        handler->get_info(ssl_key, sizeof(ssl_key), 0))
      goto err;
  }

  /*
    Starting from 5.1.16 ssl_verify_server_cert might be
    in the file
  */
  if (lines >= LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT)
  { 
    if (handler->get_info((int *) &ssl_verify_server_cert, 0))
      goto err;
  }
  /*
    Starting from 5.5 master_heartbeat_period might be
    in the file
  */
  if (lines >= LINE_FOR_MASTER_HEARTBEAT_PERIOD)
  {
    if (handler->get_info(&heartbeat_period, (float) 0.0))
      goto err;
  }

  /*
    Starting from 5.5 list of server_id of ignorable servers might be
    in the file
  */
  if (lines >= LINE_FOR_REPLICATE_IGNORE_SERVER_IDS)
  {
     if (handler->get_info(ignore_server_ids, NULL))
       goto err;
  }

  /* Starting from 5.5 the master_retry_count may be in the repository. */
  retry_count= master_retry_count;
  if (lines >= LINE_FOR_MASTER_RETRY_COUNT)
  {
    if (handler->get_info(&retry_count, master_retry_count))
      goto err;
  }


#ifndef HAVE_OPENSSL
  if (ssl)
    sql_print_warning("SSL information in the master info file "
                      "are ignored because this MySQL slave was "
                      "compiled without SSL support.");
#endif /* HAVE_OPENSSL */

end:
  if (flush_info(TRUE))
    goto err;

  inited= 1;
  DBUG_RETURN(0);

err:
  sql_print_error("Error reading master configuration");
  DBUG_RETURN(1);
}

size_t Master_info::get_number_info_mi_fields()
{
  return sizeof(info_mi_fields)/sizeof(info_mi_fields[0]); 
}
#endif /* HAVE_REPLICATION */
