/* Copyright (C) 2000-2003 MySQL AB

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

#include <my_global.h> // For HAVE_REPLICATION
#include "mysql_priv.h"
#include <my_dir.h>

#include "rpl_mi.h"

#ifdef HAVE_REPLICATION


// Defined in slave.cc
int init_intvar_from_file(int* var, IO_CACHE* f, int default_val);
int init_strvar_from_file(char *var, int max_size, IO_CACHE *f,
			  const char *default_val);
int init_floatvar_from_file(float* var, IO_CACHE* f, float default_val);
int init_dynarray_intvar_from_file(DYNAMIC_ARRAY* arr, IO_CACHE* f);

Master_info::Master_info()
  :Slave_reporting_capability("I/O"),
   ssl(0), ssl_verify_server_cert(0), fd(-1), io_thd(0), inited(0),
   abort_slave(0),slave_running(0), slave_run_id(0),
   heartbeat_period(0), received_heartbeats(0), master_id(0)
{
  host[0] = 0; user[0] = 0; password[0] = 0;
  ssl_ca[0]= 0; ssl_capath[0]= 0; ssl_cert[0]= 0;
  ssl_cipher[0]= 0; ssl_key[0]= 0;

  my_init_dynamic_array(&ignore_server_ids, sizeof(::server_id), 16, 16);
  bzero((char*) &file, sizeof(file));
  pthread_mutex_init(&run_lock, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&data_lock, MY_MUTEX_INIT_FAST);
  pthread_cond_init(&data_cond, NULL);
  pthread_cond_init(&start_cond, NULL);
  pthread_cond_init(&stop_cond, NULL);
}

Master_info::~Master_info()
{
  delete_dynamic(&ignore_server_ids);
  pthread_mutex_destroy(&run_lock);
  pthread_mutex_destroy(&data_lock);
  pthread_cond_destroy(&data_cond);
  pthread_cond_destroy(&start_cond);
  pthread_cond_destroy(&stop_cond);
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
  if (likely(ignore_server_ids.elements == 1))
    return (* (ulong*) dynamic_array_ptr(&ignore_server_ids, 0)) == s_id;
  else      
    return bsearch((const ulong *) &s_id,
                   ignore_server_ids.buffer,
                   ignore_server_ids.elements, sizeof(ulong),
                   (int (*) (const void*, const void*)) change_master_server_id_cmp)
      != NULL;
}

void init_master_info_with_options(Master_info* mi)
{
  DBUG_ENTER("init_master_info_with_options");

  mi->master_log_name[0] = 0;
  mi->master_log_pos = BIN_LOG_HEADER_SIZE;             // skip magic number

  if (master_host)
    strmake(mi->host, master_host, sizeof(mi->host) - 1);
  if (master_user)
    strmake(mi->user, master_user, sizeof(mi->user) - 1);
  if (master_password)
    strmake(mi->password, master_password, MAX_PASSWORD_LENGTH);
  mi->port = master_port;
  mi->connect_retry = master_connect_retry;

  mi->ssl= master_ssl;
  if (master_ssl_ca)
    strmake(mi->ssl_ca, master_ssl_ca, sizeof(mi->ssl_ca)-1);
  if (master_ssl_capath)
    strmake(mi->ssl_capath, master_ssl_capath, sizeof(mi->ssl_capath)-1);
  if (master_ssl_cert)
    strmake(mi->ssl_cert, master_ssl_cert, sizeof(mi->ssl_cert)-1);
  if (master_ssl_cipher)
    strmake(mi->ssl_cipher, master_ssl_cipher, sizeof(mi->ssl_cipher)-1);
  if (master_ssl_key)
    strmake(mi->ssl_key, master_ssl_key, sizeof(mi->ssl_key)-1);
  /* Intentionally init ssl_verify_server_cert to 0, no option available  */
  mi->ssl_verify_server_cert= 0;
  /* 
    always request heartbeat unless master_heartbeat_period is set
    explicitly zero.  Here is the default value for heartbeat period
    if CHANGE MASTER did not specify it.  (no data loss in conversion
    as hb period has a max)
  */
  mi->heartbeat_period= (float) min(SLAVE_MAX_HEARTBEAT_PERIOD,
                                    (slave_net_timeout/2.0));
  DBUG_ASSERT(mi->heartbeat_period > (float) 0.001
              || mi->heartbeat_period == 0);

  DBUG_VOID_RETURN;
}


enum {
  LINES_IN_MASTER_INFO_WITH_SSL= 14,

  /* 5.1.16 added value of master_ssl_verify_server_cert */
  LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT= 15,
  /* 6.0 added value of master_heartbeat_period */
  LINE_FOR_MASTER_HEARTBEAT_PERIOD= 16,
  /* 6.0 added value of master_ignore_server_id */
  LINE_FOR_REPLICATE_IGNORE_SERVER_IDS= 17,
  /* Number of lines currently used when saving master info file */
  LINES_IN_MASTER_INFO= LINE_FOR_REPLICATE_IGNORE_SERVER_IDS
};

int init_master_info(Master_info* mi, const char* master_info_fname,
                     const char* slave_info_fname,
                     bool abort_if_no_master_info_file,
                     int thread_mask)
{
  int fd,error;
  char fname[FN_REFLEN+128];
  DBUG_ENTER("init_master_info");

  if (mi->inited)
  {
    /*
      We have to reset read position of relay-log-bin as we may have
      already been reading from 'hotlog' when the slave was stopped
      last time. If this case pos_in_file would be set and we would
      get a crash when trying to read the signature for the binary
      relay log.

      We only rewind the read position if we are starting the SQL
      thread. The handle_slave_sql thread assumes that the read
      position is at the beginning of the file, and will read the
      "signature" and then fast-forward to the last position read.
    */
    if (thread_mask & SLAVE_SQL)
    {
      my_b_seek(mi->rli.cur_log, (my_off_t) 0);
    }
    DBUG_RETURN(0);
  }

  mi->mysql=0;
  mi->file_id=1;
  fn_format(fname, master_info_fname, mysql_data_home, "", 4+32);

  /*
    We need a mutex while we are changing master info parameters to
    keep other threads from reading bogus info
  */

  pthread_mutex_lock(&mi->data_lock);
  fd = mi->fd;

  /* does master.info exist ? */

  if (access(fname,F_OK))
  {
    if (abort_if_no_master_info_file)
    {
      pthread_mutex_unlock(&mi->data_lock);
      DBUG_RETURN(0);
    }
    /*
      if someone removed the file from underneath our feet, just close
      the old descriptor and re-create the old file
    */
    if (fd >= 0)
      my_close(fd, MYF(MY_WME));
    if ((fd = my_open(fname, O_CREAT|O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
    {
      sql_print_error("Failed to create a new master info file (\
file '%s', errno %d)", fname, my_errno);
      goto err;
    }
    if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,0,
                      MYF(MY_WME)))
    {
      sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
      goto err;
    }

    mi->fd = fd;
    init_master_info_with_options(mi);

  }
  else // file exists
  {
    if (fd >= 0)
      reinit_io_cache(&mi->file, READ_CACHE, 0L,0,0);
    else
    {
      if ((fd = my_open(fname, O_RDWR|O_BINARY, MYF(MY_WME))) < 0 )
      {
        sql_print_error("Failed to open the existing master info file (\
file '%s', errno %d)", fname, my_errno);
        goto err;
      }
      if (init_io_cache(&mi->file, fd, IO_SIZE*2, READ_CACHE, 0L,
                        0, MYF(MY_WME)))
      {
        sql_print_error("Failed to create a cache on master info file (\
file '%s')", fname);
        goto err;
      }
    }

    mi->fd = fd;
    int port, connect_retry, master_log_pos, lines;
    int ssl= 0, ssl_verify_server_cert= 0;
    float master_heartbeat_period= 0.0;
    char *first_non_digit;

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

    /*
       The first row is temporarily stored in mi->master_log_name,
       if it is line count and not binlog name (new format) it will be
       overwritten by the second row later.
    */
    if (init_strvar_from_file(mi->master_log_name,
                              sizeof(mi->master_log_name), &mi->file,
                              ""))
      goto errwithmsg;

    lines= strtoul(mi->master_log_name, &first_non_digit, 10);

    if (mi->master_log_name[0]!='\0' &&
        *first_non_digit=='\0' && lines >= LINES_IN_MASTER_INFO_WITH_SSL)
    {
      /* Seems to be new format => read master log name from next line */
      if (init_strvar_from_file(mi->master_log_name,
            sizeof(mi->master_log_name), &mi->file, ""))
        goto errwithmsg;
    }
    else
      lines= 7;

    if (init_intvar_from_file(&master_log_pos, &mi->file, 4) ||
        init_strvar_from_file(mi->host, sizeof(mi->host), &mi->file,
                              master_host) ||
        init_strvar_from_file(mi->user, sizeof(mi->user), &mi->file,
                              master_user) ||
        init_strvar_from_file(mi->password, SCRAMBLED_PASSWORD_CHAR_LENGTH+1,
                              &mi->file, master_password) ||
        init_intvar_from_file(&port, &mi->file, master_port) ||
        init_intvar_from_file(&connect_retry, &mi->file,
                              master_connect_retry))
      goto errwithmsg;

    /*
       If file has ssl part use it even if we have server without
       SSL support. But these option will be ignored later when
       slave will try connect to master, so in this case warning
       is printed.
     */
    if (lines >= LINES_IN_MASTER_INFO_WITH_SSL)
    {
      if (init_intvar_from_file(&ssl, &mi->file, master_ssl) ||
          init_strvar_from_file(mi->ssl_ca, sizeof(mi->ssl_ca),
                                &mi->file, master_ssl_ca) ||
          init_strvar_from_file(mi->ssl_capath, sizeof(mi->ssl_capath),
                                &mi->file, master_ssl_capath) ||
          init_strvar_from_file(mi->ssl_cert, sizeof(mi->ssl_cert),
                                &mi->file, master_ssl_cert) ||
          init_strvar_from_file(mi->ssl_cipher, sizeof(mi->ssl_cipher),
                                &mi->file, master_ssl_cipher) ||
          init_strvar_from_file(mi->ssl_key, sizeof(mi->ssl_key),
                                &mi->file, master_ssl_key))
        goto errwithmsg;

      /*
        Starting from 5.1.16 ssl_verify_server_cert might be
        in the file
      */
      if (lines >= LINE_FOR_MASTER_SSL_VERIFY_SERVER_CERT &&
          init_intvar_from_file(&ssl_verify_server_cert, &mi->file, 0))
        goto errwithmsg;
      /*
        Starting from 6.0 master_heartbeat_period might be
        in the file
      */
      if (lines >= LINE_FOR_MASTER_HEARTBEAT_PERIOD &&
          init_floatvar_from_file(&master_heartbeat_period, &mi->file, 0.0))
        goto errwithmsg;
      /*
        Starting from 6.0 list of server_id of ignorable servers might be
        in the file
      */
      if (lines >= LINE_FOR_REPLICATE_IGNORE_SERVER_IDS &&
          init_dynarray_intvar_from_file(&mi->ignore_server_ids, &mi->file))
      {
        sql_print_error("Failed to initialize master info ignore_server_ids");
        goto errwithmsg;
      }
    }

#ifndef HAVE_OPENSSL
    if (ssl)
      sql_print_warning("SSL information in the master info file "
                      "('%s') are ignored because this MySQL slave was compiled "
                      "without SSL support.", fname);
#endif /* HAVE_OPENSSL */

    /*
      This has to be handled here as init_intvar_from_file can't handle
      my_off_t types
    */
    mi->master_log_pos= (my_off_t) master_log_pos;
    mi->port= (uint) port;
    mi->connect_retry= (uint) connect_retry;
    mi->ssl= (my_bool) ssl;
    mi->ssl_verify_server_cert= ssl_verify_server_cert;
    mi->heartbeat_period= master_heartbeat_period;
  }
  DBUG_PRINT("master_info",("log_file_name: %s  position: %ld",
                            mi->master_log_name,
                            (ulong) mi->master_log_pos));

  mi->rli.mi = mi;
  if (init_relay_log_info(&mi->rli, slave_info_fname))
    goto err;

  mi->inited = 1;
  // now change cache READ -> WRITE - must do this before flush_master_info
  reinit_io_cache(&mi->file, WRITE_CACHE, 0L, 0, 1);
  if ((error=test(flush_master_info(mi, 1))))
    sql_print_error("Failed to flush master info file");
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(error);

errwithmsg:
  sql_print_error("Error reading master configuration");

err:
  if (fd >= 0)
  {
    my_close(fd, MYF(0));
    end_io_cache(&mi->file);
  }
  mi->fd= -1;
  pthread_mutex_unlock(&mi->data_lock);
  DBUG_RETURN(1);
}


/*
  RETURN
     2 - flush relay log failed
     1 - flush master info failed
     0 - all ok
*/
int flush_master_info(Master_info* mi, bool flush_relay_log_cache)
{
  IO_CACHE* file = &mi->file;
  char lbuf[22];

  DBUG_ENTER("flush_master_info");
  DBUG_PRINT("enter",("master_pos: %ld", (long) mi->master_log_pos));

  /*
    Flush the relay log to disk. If we don't do it, then the relay log while
    have some part (its last kilobytes) in memory only, so if the slave server
    dies now, with, say, from master's position 100 to 150 in memory only (not
    on disk), and with position 150 in master.info, then when the slave
    restarts, the I/O thread will fetch binlogs from 150, so in the relay log
    we will have "[0, 100] U [150, infinity[" and nobody will notice it, so the
    SQL thread will jump from 100 to 150, and replication will silently break.

    When we come to this place in code, relay log may or not be initialized;
    the caller is responsible for setting 'flush_relay_log_cache' accordingly.
  */
  if (flush_relay_log_cache &&
      flush_io_cache(mi->rli.relay_log.get_log_file()))
    DBUG_RETURN(2);
  
  /*
    produce a line listing the total number and all the ignored server_id:s
  */
  char* ignore_server_ids_buf;
  {
    ignore_server_ids_buf=
      (char *) my_malloc((sizeof(::server_id) * 3 + 1) *
                         (1 + mi->ignore_server_ids.elements), MYF(MY_WME));
    if (!ignore_server_ids_buf)
      DBUG_RETURN(1);
    for (ulong i= 0, cur_len= my_sprintf(ignore_server_ids_buf,
                                         (ignore_server_ids_buf, "%u",
                                          mi->ignore_server_ids.elements));
         i < mi->ignore_server_ids.elements; i++)
    {
      ulong s_id;
      get_dynamic(&mi->ignore_server_ids, (uchar*) &s_id, i);
      cur_len +=my_sprintf(ignore_server_ids_buf + cur_len,
                           (ignore_server_ids_buf + cur_len,
                            " %lu", s_id));
    }
  }
  /*
    We flushed the relay log BEFORE the master.info file, because if we crash
    now, we will get a duplicate event in the relay log at restart. If we
    flushed in the other order, we would get a hole in the relay log.
    And duplicate is better than hole (with a duplicate, in later versions we
    can add detection and scrap one event; with a hole there's nothing we can
    do).
  */

  /*
     In certain cases this code may create master.info files that seems
     corrupted, because of extra lines filled with garbage in the end
     file (this happens if new contents take less space than previous
     contents of file). But because of number of lines in the first line
     of file we don't care about this garbage.
  */
  char heartbeat_buf[sizeof(mi->heartbeat_period) * 4]; // buffer to suffice always
  my_sprintf(heartbeat_buf, (heartbeat_buf, "%.3f", mi->heartbeat_period));
  my_b_seek(file, 0L);
  my_b_printf(file,
              "%u\n%s\n%s\n%s\n%s\n%s\n%d\n%d\n%d\n%s\n%s\n%s\n%s\n%s\n%d\n%s\n%s\n",
              LINES_IN_MASTER_INFO,
              mi->master_log_name, llstr(mi->master_log_pos, lbuf),
              mi->host, mi->user,
              mi->password, mi->port, mi->connect_retry,
              (int)(mi->ssl), mi->ssl_ca, mi->ssl_capath, mi->ssl_cert,
              mi->ssl_cipher, mi->ssl_key, mi->ssl_verify_server_cert,
              heartbeat_buf, ignore_server_ids_buf);

  my_free(ignore_server_ids_buf, MYF(0));
  DBUG_RETURN(-flush_io_cache(file));
}


void end_master_info(Master_info* mi)
{
  DBUG_ENTER("end_master_info");

  if (!mi->inited)
    DBUG_VOID_RETURN;
  end_relay_log_info(&mi->rli);
  if (mi->fd >= 0)
  {
    end_io_cache(&mi->file);
    (void)my_close(mi->fd, MYF(MY_WME));
    mi->fd = -1;
  }
  mi->inited = 0;

  DBUG_VOID_RETURN;
}


#endif /* HAVE_REPLICATION */
