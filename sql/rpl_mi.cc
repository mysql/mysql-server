/* Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_global.h> // For HAVE_REPLICATION
#include "sql_priv.h"
#include <my_dir.h>
#include "unireg.h"                             // REQUIRED by other includes
#include "rpl_mi.h"
#include "rpl_slave.h"                          // SLAVE_MAX_HEARTBEAT_PERIOD

using std::min;
using std::max;

#ifdef HAVE_REPLICATION

enum {
  LINES_IN_MASTER_INFO_WITH_SSL= 14,

  /* 5.1.16 added value of master_ssl_verify_server_cert */
  LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT= 15,

  /* 5.5 added value of master_heartbeat_period */
  LINE_FOR_MASTER_HEARTBEAT_PERIOD= 16,

  /* MySQL Cluster 6.3 added master_bind */
  LINE_FOR_MASTER_BIND = 17,

  /* 6.0 added value of master_ignore_server_id */
  LINE_FOR_REPLICATE_IGNORE_SERVER_IDS= 18,

  /* 6.0 added value of master_uuid */
  LINE_FOR_MASTER_UUID= 19,

  /* line for master_retry_count */
  LINE_FOR_MASTER_RETRY_COUNT= 20,

  /* line for ssl_crl */
  LINE_FOR_SSL_CRL= 21,

  /* line for ssl_crl */
  LINE_FOR_SSL_CRLPATH= 22,

  /* line for auto_position */
  LINE_FOR_AUTO_POSITION= 23,

  /* Number of lines currently used when saving master info file */
  LINES_IN_MASTER_INFO= LINE_FOR_AUTO_POSITION
};

/*
  Please every time you add a new field to the mater info, update
  what follows. For now, this is just used to get the number of
  fields.
*/
const char *info_mi_fields []=
{ 
  "number_of_lines",
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
  "bind", 
  "ignore_server_ids",
  "uuid",
  "retry_count",
  "ssl_crl",
  "ssl_crlpath",
  "auto_position"
};

Master_info::Master_info(
#ifdef HAVE_PSI_INTERFACE
                         PSI_mutex_key *param_key_info_run_lock,
                         PSI_mutex_key *param_key_info_data_lock,
                         PSI_mutex_key *param_key_info_sleep_lock,
                         PSI_mutex_key *param_key_info_data_cond,
                         PSI_mutex_key *param_key_info_start_cond,
                         PSI_mutex_key *param_key_info_stop_cond,
                         PSI_mutex_key *param_key_info_sleep_cond,
#endif
                         uint param_id
                        )
   :Rpl_info("I/O"
#ifdef HAVE_PSI_INTERFACE
             ,param_key_info_run_lock, param_key_info_data_lock,
             param_key_info_sleep_lock,
             param_key_info_data_cond, param_key_info_start_cond,
             param_key_info_stop_cond, param_key_info_sleep_cond
#endif
             ,param_id
            ),
   start_user_configured(false),
   ssl(0), ssl_verify_server_cert(0),
   port(MYSQL_PORT), connect_retry(DEFAULT_CONNECT_RETRY),
   clock_diff_with_master(0), heartbeat_period(0),
   received_heartbeats(0), last_heartbeat(0), master_id(0),
   checksum_alg_before_fd(BINLOG_CHECKSUM_ALG_UNDEF),
   retry_count(master_retry_count), master_gtid_mode(0),
   mi_description_event(NULL),
   auto_position(false)
{
  host[0] = 0; user[0] = 0; bind_addr[0] = 0;
  password[0]= 0; start_password[0]= 0;
  ssl_ca[0]= 0; ssl_capath[0]= 0; ssl_cert[0]= 0;
  ssl_cipher[0]= 0; ssl_key[0]= 0;
  ssl_crl[0]= 0; ssl_crlpath[0]= 0;
  master_uuid[0]= 0;
  start_plugin_auth[0]= 0; start_plugin_dir[0]= 0;
  start_user[0]= 0;
  ignore_server_ids= new Server_ids(sizeof(::server_id));
}

Master_info::~Master_info()
{
  delete ignore_server_ids;
  delete mi_description_event;
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
  if (likely(ignore_server_ids->dynamic_ids.elements == 1))
    return (* (ulong*)
      dynamic_array_ptr(&(ignore_server_ids->dynamic_ids), 0)) == s_id;
  else      
    return bsearch((const ulong *) &s_id,
                   ignore_server_ids->dynamic_ids.buffer,
                   ignore_server_ids->dynamic_ids.elements, sizeof(ulong),
                   (int (*) (const void*, const void*)) change_master_server_id_cmp)
      != NULL;
}

void Master_info::clear_in_memory_info(bool all)
{
  init_master_log_pos();
  if (all)
  {
    port= MYSQL_PORT;
    host[0] = 0; user[0] = 0; password[0] = 0;
  }
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
  heartbeat_period= min<float>(SLAVE_MAX_HEARTBEAT_PERIOD,
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

/**
  Store the file and position where the slave's SQL thread are in the
   relay log.

  - This function should be called either from the slave SQL thread,
    or when the slave thread is not running.  (It reads the
    group_{relay|master}_log_{pos|name} and delay fields in the rli
    object.  These may only be modified by the slave SQL thread or by
    a client thread when the slave SQL thread is not running.)

  - If there is an active transaction, then we do not update the
    position in the relay log.  This is to ensure that we re-execute
    statements if we die in the middle of an transaction that was
    rolled back.

  - As a transaction never spans binary logs, we don't have to handle
    the case where we do a relay-log-rotation in the middle of the
    transaction.  If transactions could span several binlogs, we would
    have to ensure that we do not delete the relay log file where the
    transaction started before switching to a new relay log file.

  - Error can happen if writing to file fails or if flushing the file
    fails.

  @param rli The object representing the Relay_log_info.

  @todo Change the log file information to a binary format to avoid
  calling longlong2str.
*/
int Master_info::flush_info(bool force)
{
  DBUG_ENTER("Master_info::flush_info");
  DBUG_PRINT("enter",("master_pos: %lu", (ulong) master_log_pos));

  if (!inited)
    DBUG_RETURN(0);

  /*
    We update the sync_period at this point because only here we
    now that we are handling a master info. This needs to be
    update every time we call flush because the option maybe
    dinamically set.
  */
  handler->set_sync_period(sync_masterinfo_period);

  if (write_info(handler))
    goto err;

  if (handler->flush_info(force))
    goto err;

  DBUG_RETURN(0);

err:
  sql_print_error("Error writing master configuration.");
  DBUG_RETURN(1);
}

void Master_info::set_relay_log_info(Relay_log_info* info)
{
  rli= info;
}


/**
  Creates or reads information from the repository, initializing the
  Master_info.
*/
int Master_info::mi_init_info()
{
  DBUG_ENTER("Master_info::mi_init_info");
  enum_return_check check_return= ERROR_CHECKING_REPOSITORY;

  if (inited)
    DBUG_RETURN(0);

  mysql= 0; file_id= 1;
  if ((check_return= check_info()) == ERROR_CHECKING_REPOSITORY)
    goto err;
  
  if (handler->init_info())
    goto err;

  if (check_return == REPOSITORY_DOES_NOT_EXIST)
  {
    init_master_log_pos();
  }
  else 
  {
    if (read_info(handler))
      goto err;
  }

  inited= 1;
  if (flush_info(TRUE))
    goto err;

  DBUG_RETURN(0);

err:
  handler->end_info();
  inited= 0;
  sql_print_error("Error reading master configuration.");
  DBUG_RETURN(1);
}

size_t Master_info::get_number_info_mi_fields()
{
  return sizeof(info_mi_fields)/sizeof(info_mi_fields[0]); 
}

bool Master_info::read_info(Rpl_info_handler *from)
{
  int lines= 0;
  char *first_non_digit= NULL;
  ulong temp_master_log_pos= 0;
  int temp_ssl= 0;
  int temp_ssl_verify_server_cert= 0;
  int temp_auto_position= 0;

  DBUG_ENTER("Master_info::read_info");

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

  if (from->prepare_info_for_read() || 
      from->get_info(master_log_name, (size_t) sizeof(master_log_name),
                     (char *) ""))
    DBUG_RETURN(true);

  lines= strtoul(master_log_name, &first_non_digit, 10);

  if (master_log_name[0]!='\0' &&
      *first_non_digit=='\0' && lines >= LINES_IN_MASTER_INFO_WITH_SSL)
  {
    /* Seems to be new format => read master log name */
    if (from->get_info(master_log_name, (size_t) sizeof(master_log_name),
                       (char *) ""))
      DBUG_RETURN(true);
  }
  else 
    lines= 7;

  if (from->get_info(&temp_master_log_pos,
                     (ulong) BIN_LOG_HEADER_SIZE) ||
      from->get_info(host, (size_t) sizeof(host), (char *) 0) ||
      from->get_info(user, (size_t) sizeof(user), (char *) "test") ||
      from->get_info(password, (size_t) sizeof(password), (char *) 0) ||
      from->get_info((int *) &port, (int) MYSQL_PORT) ||
      from->get_info((int *) &connect_retry,
                        (int) DEFAULT_CONNECT_RETRY))
      DBUG_RETURN(true);

  /*
    If file has ssl part use it even if we have server without
    SSL support. But these options will be ignored later when
    slave will try connect to master, so in this case warning
    is printed.
  */
  if (lines >= LINES_IN_MASTER_INFO_WITH_SSL)
  {
    if (from->get_info(&temp_ssl, 0) ||
        from->get_info(ssl_ca, (size_t) sizeof(ssl_ca), (char *) 0) ||
        from->get_info(ssl_capath, (size_t) sizeof(ssl_capath), (char *) 0) ||
        from->get_info(ssl_cert, (size_t) sizeof(ssl_cert), (char *) 0) ||
        from->get_info(ssl_cipher, (size_t) sizeof(ssl_cipher), (char *) 0) ||
        from->get_info(ssl_key, (size_t) sizeof(ssl_key), (char *) 0))
      DBUG_RETURN(true);
  }

  /*
    Starting from 5.1.16 ssl_verify_server_cert might be
    in the file
  */
  if (lines >= LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT)
  { 
    if (from->get_info(&temp_ssl_verify_server_cert, (int) 0))
      DBUG_RETURN(true);
  }

  /*
    Starting from 5.5 master_heartbeat_period might be
    in the file
  */
  if (lines >= LINE_FOR_MASTER_HEARTBEAT_PERIOD)
  {
    if (from->get_info(&heartbeat_period, (float) 0.0))
      DBUG_RETURN(true);
  }

  /*
    Starting from 5.5 master_bind might be in the file
  */
  if (lines >= LINE_FOR_MASTER_BIND)
  {
    if (from->get_info(bind_addr, (size_t) sizeof(bind_addr), (char *) ""))
      DBUG_RETURN(true);
  }

  /*
    Starting from 5.5 list of server_id of ignorable servers might be
    in the file
  */
  if (lines >= LINE_FOR_REPLICATE_IGNORE_SERVER_IDS)
  {
     if (from->get_info(ignore_server_ids, (Dynamic_ids *) NULL))
      DBUG_RETURN(true);
  }

  /* Starting from 5.5 the master_uuid may be in the repository. */
  if (lines >= LINE_FOR_MASTER_UUID)
  {
    if (from->get_info(master_uuid, (size_t) sizeof(master_uuid),
                       (char *) 0))
      DBUG_RETURN(true);
  }

  /* Starting from 5.5 the master_retry_count may be in the repository. */
  retry_count= master_retry_count;
  if (lines >= LINE_FOR_MASTER_RETRY_COUNT)
  {
    if (from->get_info(&retry_count, master_retry_count))
      DBUG_RETURN(true);
  }

  if (lines >= LINE_FOR_SSL_CRLPATH)
  {
    if (from->get_info(ssl_crl, sizeof(ssl_crl), (char *) 0) ||
        from->get_info(ssl_crlpath, sizeof(ssl_crlpath), (char *) 0))
      DBUG_RETURN(true);
  }

  if (lines >= LINE_FOR_AUTO_POSITION)
  {
    if (from->get_info(&temp_auto_position, (int) 0))
      DBUG_RETURN(true);
  }

  ssl= (my_bool) MY_TEST(temp_ssl);
  ssl_verify_server_cert= (my_bool) MY_TEST(temp_ssl_verify_server_cert);
  master_log_pos= (my_off_t) temp_master_log_pos;
  auto_position= MY_TEST(temp_auto_position);

  if (auto_position != 0 && gtid_mode != 3)
  {
    auto_position = 0;
    sql_print_warning("MASTER_AUTO_POSITION in the master info file was 1 but "
                      "server is started with @@GLOBAL.GTID_MODE = OFF. Forcing "
                      "MASTER_AUTO_POSITION to 0.");
  }

#ifndef HAVE_OPENSSL
  if (ssl)
    sql_print_warning("SSL information in the master info file "
                      "are ignored because this MySQL slave was "
                      "compiled without SSL support.");
#endif /* HAVE_OPENSSL */

  DBUG_RETURN(false);
}

bool Master_info::write_info(Rpl_info_handler *to)
{
  DBUG_ENTER("Master_info::write_info");

  /*
     In certain cases this code may create master.info files that seems
     corrupted, because of extra lines filled with garbage in the end
     file (this happens if new contents take less space than previous
     contents of file). But because of number of lines in the first line
     of file we don't care about this garbage.
  */
  if (to->prepare_info_for_write() ||
      to->set_info((int) LINES_IN_MASTER_INFO) ||
      to->set_info(master_log_name) ||
      to->set_info((ulong) master_log_pos) ||
      to->set_info(host) ||
      to->set_info(user) ||
      to->set_info(password) ||
      to->set_info((int) port) ||
      to->set_info((int) connect_retry) ||
      to->set_info((int) ssl) ||
      to->set_info(ssl_ca) ||
      to->set_info(ssl_capath) ||
      to->set_info(ssl_cert) ||
      to->set_info(ssl_cipher) ||
      to->set_info(ssl_key) ||
      to->set_info((int) ssl_verify_server_cert) ||
      to->set_info(heartbeat_period) ||
      to->set_info(bind_addr) ||
      to->set_info(ignore_server_ids) ||
      to->set_info(master_uuid) ||
      to->set_info(retry_count) ||
      to->set_info(ssl_crl) ||
      to->set_info(ssl_crlpath) ||
      to->set_info((int) auto_position))
    DBUG_RETURN(TRUE);

  DBUG_RETURN(FALSE);
}

bool Master_info::set_password(const char* password_arg,
                               int password_arg_size __attribute__((unused)))
{
  bool ret= true;
  DBUG_ENTER("Master_info::set_password");

  if (password_arg && start_user_configured)
  {
    strmake(start_password, password_arg, sizeof(start_password) - 1);
    ret= false;
  }
  else if (password_arg)
  {
    strmake(password, password_arg, sizeof(password) - 1);
    ret= false;
  }
  DBUG_RETURN(ret);
}

bool Master_info::get_password(char *password_arg, int *password_arg_size)
{
  bool ret= true;
  DBUG_ENTER("Master_info::get_password");

  if (password_arg && start_user_configured)
  {
    *password_arg_size= strlen(start_password);
    strmake(password_arg, start_password, sizeof(start_password) - 1);
    ret= false;
  }
  else if (password_arg)
  {
    *password_arg_size= strlen(password);
    strmake(password_arg, password, sizeof(password) - 1);
    ret= false;
  }
  DBUG_RETURN(ret);
}

void Master_info::reset_start_info()
{
  DBUG_ENTER("Master_info::reset_start_info");
  start_plugin_auth[0]= 0;
  start_plugin_dir[0]= 0;
  start_user_configured= false;
  start_user[0]= 0;
  start_password[0]= 0;
  DBUG_VOID_RETURN;
}
#endif /* HAVE_REPLICATION */
