/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "sql_priv.h"
#include "unireg.h"
#include "sql_parse.h"                          // check_access
#include "global_threads.h"
#ifdef HAVE_REPLICATION

#include "sql_acl.h"                            // SUPER_ACL
#include "log_event.h"
#include "rpl_filter.h"
#include <my_dir.h>
#include "rpl_handler.h"
#include "rpl_master.h"
#include "debug_sync.h"

int max_binlog_dump_events = 0; // unlimited
my_bool opt_sporadic_binlog_dump_fail = 0;

#ifndef DBUG_OFF
static int binlog_dump_count = 0;
#endif

#define SLAVE_LIST_CHUNK 128
#define SLAVE_ERRMSG_SIZE (FN_REFLEN+64)
HASH slave_list;
extern TYPELIB binlog_checksum_typelib;


#define get_object(p, obj, msg) \
{\
  uint len = (uint)*p++;  \
  if (p + len > p_end || len >= sizeof(obj)) \
  {\
    errmsg= msg;\
    goto err; \
  }\
  strmake(obj,(char*) p,len); \
  p+= len; \
}\

extern "C" uint32
*slave_list_key(SLAVE_INFO* si, size_t *len,
		my_bool not_used __attribute__((unused)))
{
  *len = 4;
  return &si->server_id;
}

extern "C" void slave_info_free(void *s)
{
  my_free(s);
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_slave_list;

static PSI_mutex_info all_slave_list_mutexes[]=
{
  { &key_LOCK_slave_list, "LOCK_slave_list", PSI_FLAG_GLOBAL}
};

static void init_all_slave_list_mutexes(void)
{
  int count;

  count= array_elements(all_slave_list_mutexes);
  mysql_mutex_register("sql", all_slave_list_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

void init_slave_list()
{
#ifdef HAVE_PSI_INTERFACE
  init_all_slave_list_mutexes();
#endif

  my_hash_init(&slave_list, system_charset_info, SLAVE_LIST_CHUNK, 0, 0,
               (my_hash_get_key) slave_list_key,
               (my_hash_free_key) slave_info_free, 0);
  mysql_mutex_init(key_LOCK_slave_list, &LOCK_slave_list, MY_MUTEX_INIT_FAST);
}

void end_slave_list()
{
  /* No protection by a mutex needed as we are only called at shutdown */
  if (my_hash_inited(&slave_list))
  {
    my_hash_free(&slave_list);
    mysql_mutex_destroy(&LOCK_slave_list);
  }
}

/**
  Register slave in 'slave_list' hash table.

  @return
    0	ok
  @return
    1	Error.   Error message sent to client
*/

int register_slave(THD* thd, uchar* packet, uint packet_length)
{
  int res;
  SLAVE_INFO *si;
  uchar *p= packet, *p_end= packet + packet_length;
  const char *errmsg= "Wrong parameters to function register_slave";

  if (check_access(thd, REPL_SLAVE_ACL, any_db, NULL, NULL, 0, 0))
    return 1;
  if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
    goto err2;

  thd->server_id= si->server_id= uint4korr(p);
  p+= 4;
  get_object(p,si->host, "Failed to register slave: too long 'report-host'");
  get_object(p,si->user, "Failed to register slave: too long 'report-user'");
  get_object(p,si->password, "Failed to register slave; too long 'report-password'");
  if (p+10 > p_end)
    goto err;
  si->port= uint2korr(p);
  p += 2;
  /* 
     We need to by pass the bytes used in the fake rpl_recovery_rank
     variable. It was removed in patch for BUG#13963. But this would 
     make a server with that patch unable to connect to an old master.
     See: BUG#49259
  */
  p += 4;
  if (!(si->master_id= uint4korr(p)))
    si->master_id= server_id;
  si->thd= thd;

  mysql_mutex_lock(&LOCK_slave_list);
  unregister_slave(thd, false, false/*need_lock_slave_list=false*/);
  res= my_hash_insert(&slave_list, (uchar*) si);
  mysql_mutex_unlock(&LOCK_slave_list);
  return res;

err:
  my_free(si);
  my_message(ER_UNKNOWN_ERROR, errmsg, MYF(0)); /* purecov: inspected */
err2:
  return 1;
}

void unregister_slave(THD* thd, bool only_mine, bool need_lock_slave_list)
{
  if (thd->server_id)
  {
    if (need_lock_slave_list)
      mysql_mutex_lock(&LOCK_slave_list);
    else
      mysql_mutex_assert_owner(&LOCK_slave_list);

    SLAVE_INFO* old_si;
    if ((old_si = (SLAVE_INFO*)my_hash_search(&slave_list,
                                              (uchar*)&thd->server_id, 4)) &&
	(!only_mine || old_si->thd == thd))
    my_hash_delete(&slave_list, (uchar*)old_si);

    if (need_lock_slave_list)
      mysql_mutex_unlock(&LOCK_slave_list);
  }
}


/**
  Execute a SHOW SLAVE HOSTS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_slave_hosts(THD* thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_slave_hosts");

  field_list.push_back(new Item_return_int("Server_id", 10,
					   MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Host", 20));
  if (opt_show_slave_auth_info)
  {
    field_list.push_back(new Item_empty_string("User",20));
    field_list.push_back(new Item_empty_string("Password",20));
  }
  field_list.push_back(new Item_return_int("Port", 7, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Master_id", 10,
					   MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Slave_UUID", UUID_LENGTH));

  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  mysql_mutex_lock(&LOCK_slave_list);

  for (uint i = 0; i < slave_list.records; ++i)
  {
    SLAVE_INFO* si = (SLAVE_INFO*) my_hash_element(&slave_list, i);
    protocol->prepare_for_resend();
    protocol->store((uint32) si->server_id);
    protocol->store(si->host, &my_charset_bin);
    if (opt_show_slave_auth_info)
    {
      protocol->store(si->user, &my_charset_bin);
      protocol->store(si->password, &my_charset_bin);
    }
    protocol->store((uint32) si->port);
    protocol->store((uint32) si->master_id);

    /* get slave's UUID */
    String slave_uuid;
    if (get_slave_uuid(si->thd, &slave_uuid))
      protocol->store(slave_uuid.c_ptr_safe(), &my_charset_bin);
    if (protocol->write())
    {
      mysql_mutex_unlock(&LOCK_slave_list);
      DBUG_RETURN(TRUE);
    }
  }
  mysql_mutex_unlock(&LOCK_slave_list);
  my_eof(thd);
  DBUG_RETURN(FALSE);
}


/**
   Internal to mysql_binlog_send() routine that recalculates checksum for
   a FD event (asserted) that needs additional arranment prior sending to slave.
*/
inline void fix_checksum(String *packet, ulong ev_offset)
{
  /* recalculate the crc for this event */
  uint data_len = uint4korr(packet->ptr() + ev_offset + EVENT_LEN_OFFSET);
  ha_checksum crc= my_checksum(0L, NULL, 0);
  DBUG_ASSERT(data_len == 
              LOG_EVENT_MINIMAL_HEADER_LEN + FORMAT_DESCRIPTION_HEADER_LEN +
              BINLOG_CHECKSUM_ALG_DESC_LEN + BINLOG_CHECKSUM_LEN);
  crc= my_checksum(crc, (uchar *)packet->ptr() + ev_offset, data_len -
                   BINLOG_CHECKSUM_LEN);
  int4store(packet->ptr() + ev_offset + data_len - BINLOG_CHECKSUM_LEN, crc);
}


static user_var_entry * get_binlog_checksum_uservar(THD * thd)
{
  LEX_STRING name=  { C_STRING_WITH_LEN("master_binlog_checksum")};
  user_var_entry *entry= 
    (user_var_entry*) my_hash_search(&thd->user_vars, (uchar*) name.str,
                                  name.length);
  return entry;
}

/**
  Function for calling in mysql_binlog_send
  to check if slave initiated checksum-handshake.

  @param[in]    thd  THD to access a user variable

  @return        TRUE if handshake took place, FALSE otherwise
*/

static bool is_slave_checksum_aware(THD * thd)
{
  DBUG_ENTER("is_slave_checksum_aware");
  user_var_entry *entry= get_binlog_checksum_uservar(thd);
  DBUG_RETURN(entry? true  : false);
}

/**
  Function for calling in mysql_binlog_send
  to get the value of @@binlog_checksum of the master at
  time of checksum-handshake.

  The value tells the master whether to compute or not, and the slave
  to verify or not the first artificial Rotate event's checksum.

  @param[in]    thd  THD to access a user variable

  @return       value of @@binlog_checksum alg according to
                @c enum enum_binlog_checksum_alg
*/

static uint8 get_binlog_checksum_value_at_connect(THD * thd)
{
  uint8 ret;

  DBUG_ENTER("get_binlog_checksum_value_at_connect");
  user_var_entry *entry= get_binlog_checksum_uservar(thd);
  if (!entry)
  {
    ret= BINLOG_CHECKSUM_ALG_UNDEF;
  }
  else
  {
    DBUG_ASSERT(entry->type() == STRING_RESULT);
    String str;
    uint dummy_errors;
    str.copy(entry->ptr(), entry->length(), &my_charset_bin, &my_charset_bin,
             &dummy_errors);
    ret= (uint8) find_type ((char*) str.ptr(), &binlog_checksum_typelib, 1) - 1;
    DBUG_ASSERT(ret <= BINLOG_CHECKSUM_ALG_CRC32); // while it's just on CRC32 alg
  }
  DBUG_RETURN(ret);
}

/*
    fake_rotate_event() builds a fake (=which does not exist physically in any
    binlog) Rotate event, which contains the name of the binlog we are going to
    send to the slave (because the slave may not know it if it just asked for
    MASTER_LOG_FILE='', MASTER_LOG_POS=4).
    < 4.0.14, fake_rotate_event() was called only if the requested pos was 4.
    After this version we always call it, so that a 3.23.58 slave can rely on
    it to detect if the master is 4.0 (and stop) (the _fake_ Rotate event has
    zeros in the good positions which, by chance, make it possible for the 3.23
    slave to detect that this event is unexpected) (this is luck which happens
    because the master and slave disagree on the size of the header of
    Log_event).

    Relying on the event length of the Rotate event instead of these
    well-placed zeros was not possible as Rotate events have a variable-length
    part.
*/

static int fake_rotate_event(NET* net, String* packet, char* log_file_name,
                             ulonglong position, const char** errmsg,
                             uint8 checksum_alg_arg)
{
  DBUG_ENTER("fake_rotate_event");
  char header[LOG_EVENT_HEADER_LEN], buf[ROTATE_HEADER_LEN+100];

  /*
    this Rotate is to be sent with checksum if and only if
    slave's get_master_version_and_clock time handshake value 
    of master's @@global.binlog_checksum was TRUE
  */

  my_bool do_checksum= checksum_alg_arg != BINLOG_CHECKSUM_ALG_OFF &&
    checksum_alg_arg != BINLOG_CHECKSUM_ALG_UNDEF;

  /*
    'when' (the timestamp) is set to 0 so that slave could distinguish between
    real and fake Rotate events (if necessary)
  */
  memset(header, 0, 4);
  header[EVENT_TYPE_OFFSET] = ROTATE_EVENT;

  char* p = log_file_name+dirname_length(log_file_name);
  uint ident_len = (uint) strlen(p);
  ulong event_len = ident_len + LOG_EVENT_HEADER_LEN + ROTATE_HEADER_LEN +
    (do_checksum ? BINLOG_CHECKSUM_LEN : 0);
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, LOG_EVENT_ARTIFICIAL_F);

  // TODO: check what problems this may cause and fix them
  int4store(header + LOG_POS_OFFSET, 0);

  packet->append(header, sizeof(header));
  int8store(buf+R_POS_OFFSET,position);
  packet->append(buf, ROTATE_HEADER_LEN);
  packet->append(p, ident_len);

  if (do_checksum)
  {
    char b[BINLOG_CHECKSUM_LEN];
    ha_checksum crc= my_checksum(0L, NULL, 0);
    crc= my_checksum(crc, (uchar*)header, sizeof(header));
    crc= my_checksum(crc, (uchar*)buf, ROTATE_HEADER_LEN);
    crc= my_checksum(crc, (uchar*)p, ident_len);
    int4store(b, crc);
    packet->append(b, sizeof(b));
  }

  if (my_net_write(net, (uchar*) packet->ptr(), packet->length()))
  {
    *errmsg = "failed on my_net_write()";
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/*
  Reset thread transmit packet buffer for event sending

  This function allocates header bytes for event transmission, and
  should be called before store the event data to the packet buffer.
*/
static int reset_transmit_packet(THD *thd, ushort flags,
                                 ulong *ev_offset, const char **errmsg)
{
  int ret= 0;
  String *packet= &thd->packet;

  /* reserve and set default header */
  packet->length(0);
  packet->set("\0", 1, &my_charset_bin);

  if (RUN_HOOK(binlog_transmit, reserve_header, (thd, flags, packet)))
  {
    *errmsg= "Failed to run hook 'reserve_header'";
    my_errno= ER_UNKNOWN_ERROR;
    ret= 1;
  }
  *ev_offset= packet->length();
  DBUG_PRINT("info", ("rpl_master.cc:reset_transmit_packet returns %d", ret));
  return ret;
}

static int send_file(THD *thd)
{
  NET* net = &thd->net;
  int fd = -1, error = 1;
  size_t bytes;
  char fname[FN_REFLEN+1];
  const char *errmsg = 0;
  int old_timeout;
  unsigned long packet_len;
  uchar buf[IO_SIZE];				// It's safe to alloc this
  DBUG_ENTER("send_file");

  /*
    The client might be slow loading the data, give him wait_timeout to do
    the job
  */
  old_timeout= net->read_timeout;
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  /*
    We need net_flush here because the client will not know it needs to send
    us the file name until it has processed the load event entry
  */
  if (net_flush(net) || (packet_len = my_net_read(net)) == packet_error)
  {
    errmsg = "while reading file name";
    goto err;
  }

  // terminate with \0 for fn_format
  *((char*)net->read_pos +  packet_len) = 0;
  fn_format(fname, (char*) net->read_pos + 1, "", "", 4);
  // this is needed to make replicate-ignore-db
  if (!strcmp(fname,"/dev/null"))
    goto end;

  if ((fd= mysql_file_open(key_file_send_file,
                           fname, O_RDONLY, MYF(0))) < 0)
  {
    errmsg = "on open of file";
    goto err;
  }

  while ((long) (bytes= mysql_file_read(fd, buf, IO_SIZE, MYF(0))) > 0)
  {
    if (my_net_write(net, buf, bytes))
    {
      errmsg = "while writing data to client";
      goto err;
    }
  }

 end:
  if (my_net_write(net, (uchar*) "", 0) || net_flush(net) ||
      (my_net_read(net) == packet_error))
  {
    errmsg = "while negotiating file transfer close";
    goto err;
  }
  error = 0;

 err:
  my_net_set_read_timeout(net, old_timeout);
  if (fd >= 0)
    mysql_file_close(fd, MYF(0));
  if (errmsg)
  {
    sql_print_error("Failed in send_file() %s", errmsg);
    DBUG_PRINT("error", ("%s", errmsg));
  }
  DBUG_RETURN(error);
}


int test_for_non_eof_log_read_errors(int error, const char **errmsg)
{
  if (error == LOG_READ_EOF)
    return 0;
  my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
  switch (error) {
  case LOG_READ_BOGUS:
    *errmsg = "bogus data in log event";
    break;
  case LOG_READ_TOO_LARGE:
    *errmsg = "log event entry exceeded max_allowed_packet; \
Increase max_allowed_packet on master";
    break;
  case LOG_READ_IO:
    *errmsg = "I/O error reading log event";
    break;
  case LOG_READ_MEM:
    *errmsg = "memory allocation failed reading log event";
    break;
  case LOG_READ_TRUNC:
    *errmsg = "binlog truncated in the middle of event; consider out of disk space on master";
    break;
  case LOG_READ_CHECKSUM_FAILURE:
    *errmsg = "event read from binlog did not pass crc check";
    break;
  default:
    *errmsg = "unknown error reading log event on the master";
    break;
  }
  return error;
}


/**
  An auxiliary function for calling in mysql_binlog_send
  to initialize the heartbeat timeout in waiting for a binlogged event.

  @param[in]    thd  THD to access a user variable

  @return        heartbeat period an ulonglong of nanoseconds
                 or zero if heartbeat was not demanded by slave
*/ 
static ulonglong get_heartbeat_period(THD * thd)
{
  my_bool null_value;
  LEX_STRING name=  { C_STRING_WITH_LEN("master_heartbeat_period")};
  user_var_entry *entry= 
    (user_var_entry*) my_hash_search(&thd->user_vars, (uchar*) name.str,
                                  name.length);
  return entry? entry->val_int(&null_value) : 0;
}

/*
  Function prepares and sends repliation heartbeat event.

  @param net                net object of THD
  @param packet             buffer to store the heartbeat instance
  @param event_coordinates  binlog file name and position of the last
                            real event master sent from binlog

  @note 
    Among three essential pieces of heartbeat data Log_event::when
    is computed locally.
    The  error to send is serious and should force terminating
    the dump thread.
*/
static int send_heartbeat_event(NET* net, String* packet,
                                const struct event_coordinates *coord,
                                uint8 checksum_alg_arg)
{
  DBUG_ENTER("send_heartbeat_event");
  char header[LOG_EVENT_HEADER_LEN];
  my_bool do_checksum= checksum_alg_arg != BINLOG_CHECKSUM_ALG_OFF &&
    checksum_alg_arg != BINLOG_CHECKSUM_ALG_UNDEF;
  /*
    'when' (the timestamp) is set to 0 so that slave could distinguish between
    real and fake Rotate events (if necessary)
  */
  memset(header, 0, 4);  // when

  header[EVENT_TYPE_OFFSET] = HEARTBEAT_LOG_EVENT;

  char* p= coord->file_name + dirname_length(coord->file_name);

  uint ident_len = strlen(p);
  ulong event_len = ident_len + LOG_EVENT_HEADER_LEN +
    (do_checksum ? BINLOG_CHECKSUM_LEN : 0);
  int4store(header + SERVER_ID_OFFSET, server_id);
  int4store(header + EVENT_LEN_OFFSET, event_len);
  int2store(header + FLAGS_OFFSET, 0);

  int4store(header + LOG_POS_OFFSET, coord->pos);  // log_pos

  packet->append(header, sizeof(header));
  packet->append(p, ident_len);             // log_file_name

  if (do_checksum)
  {
    char b[BINLOG_CHECKSUM_LEN];
    ha_checksum crc= my_checksum(0L, NULL, 0);
    crc= my_checksum(crc, (uchar*) header, sizeof(header));
    crc= my_checksum(crc, (uchar*) p, ident_len);
    int4store(b, crc);
    packet->append(b, sizeof(b));
  }

  if (my_net_write(net, (uchar*) packet->ptr(), packet->length()) ||
      net_flush(net))
  {
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


/**
  If there are less than BYTES bytes left to read in the packet,
  report error.
*/
#define CHECK_PACKET_SIZE(BYTES)                                        \
  do {                                                                  \
    if (packet_bytes_todo < BYTES)                                      \
      goto error_malformed_packet;                                      \
  } while (0)

/**
  Auxiliary macro used to define READ_INT and READ_STRING.

  Check that there are at least BYTES more bytes to read, then read
  the bytes using the given DECODER, then advance the reading
  position.
*/
#define READ(DECODE, BYTES)                                             \
  do {                                                                  \
    CHECK_PACKET_SIZE(BYTES);                                           \
    DECODE;                                                             \
    packet_position+= BYTES;                                            \
    packet_bytes_todo-= BYTES;                                          \
  } while (0)

#define SKIP(BYTES) READ((void)(0), BYTES)

/**
  Check that there are at least BYTES more bytes to read, then read
  the bytes and decode them into the given integer VAR, then advance
  the reading position.
*/
#define READ_INT(VAR, BYTES)                                            \
  READ(VAR= uint ## BYTES ## korr(packet_position), BYTES)

/**
  Check that there are at least BYTES more bytes to read and that
  BYTES+1 is not greater than BUFFER_SIZE, then read the bytes into
  the given variable VAR, then advance the reading position.
*/
#define READ_STRING(VAR, BYTES, BUFFER_SIZE)                            \
  do {                                                                  \
    if (BUFFER_SIZE <= BYTES)                                           \
      goto error_malformed_packet;                                      \
    READ(memcpy(VAR, packet_position, BYTES), BYTES);                   \
    VAR[BYTES]= '\0';                                                   \
  } while (0)


bool com_binlog_dump(THD *thd, char *packet, uint packet_length)
{
  DBUG_ENTER("com_binlog_dump");
  ulong pos;
  String slave_uuid;
  const uchar* packet_position= (uchar *) packet;
  uint packet_bytes_todo= packet_length;

  status_var_increment(thd->status_var.com_other);
  thd->enable_slow_log= opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL))
    DBUG_RETURN(false);

  /*
    4 bytes is too little, but changing the protocol would break
    compatibility.  This has been fixed in the new protocol. @see
    com_binlog_dump_gtid().
  */
  READ_INT(pos, 4);
  SKIP(2); /* flags field is unused */
  READ_INT(thd->server_id, 4);

  get_slave_uuid(thd, &slave_uuid);
  kill_zombie_dump_threads(&slave_uuid);

  general_log_print(thd, thd->get_command(), "Log: '%s'  Pos: %ld",
                    packet + 10, (long) pos);
  mysql_binlog_send(thd, thd->strdup(packet + 10), (my_off_t) pos, NULL);

  unregister_slave(thd, true, true/*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  DBUG_RETURN(true);

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  DBUG_RETURN(true);
}


bool com_binlog_dump_gtid(THD *thd, char *packet, uint packet_length)
{
  DBUG_ENTER("com_binlog_dump_gtid");
  /*
    Before going GA, we need to make this protocol extensible without
    breaking compatitibilty. /Alfranio.
  */
  String slave_uuid;
  uint32 data_size= 0;
  uint64 pos= 0;
  char name[FN_REFLEN + 1];
  uint32 name_size= 0;
  char* gtid_string= NULL;
  const uchar* packet_position= (uchar *) packet;
  uint packet_bytes_todo= packet_length;
  Sid_map sid_map(NULL/*no sid_lock because this is a completely local object*/);
  Gtid_set slave_gtid_executed(&sid_map);

  status_var_increment(thd->status_var.com_other);
  thd->enable_slow_log= opt_log_slow_admin_statements;
  if (check_global_access(thd, REPL_SLAVE_ACL))
    DBUG_RETURN(false);

  SKIP(2); /* flags field is unused */
  READ_INT(thd->server_id, 4);
  READ_INT(name_size, 4);
  READ_STRING(name, name_size, sizeof(name));
  READ_INT(pos, 8);
  READ_INT(data_size, 4);
  CHECK_PACKET_SIZE(data_size);
  if (slave_gtid_executed.add_gtid_encoding(packet_position, data_size) !=
      RETURN_STATUS_OK)
    DBUG_RETURN(true);
  gtid_string= slave_gtid_executed.to_string();
  DBUG_PRINT("info", ("Slave %d requested to read %s at position %llu gtid set "
                      "'%s'.", thd->server_id, name, pos, gtid_string));

  get_slave_uuid(thd, &slave_uuid);
  kill_zombie_dump_threads(&slave_uuid);
  general_log_print(thd, thd->get_command(), "Log: '%s' Pos: %llu GTIDs: '%s'",
                    name, pos, gtid_string);
  my_free(gtid_string);
  mysql_binlog_send(thd, name, (my_off_t) pos, &slave_gtid_executed);

  unregister_slave(thd, true, true/*need_lock_slave_list=true*/);
  /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
  DBUG_RETURN(true);

error_malformed_packet:
  my_error(ER_MALFORMED_PACKET, MYF(0));
  DBUG_RETURN(true);
}


void mysql_binlog_send(THD* thd, char* log_ident, my_off_t pos,
                       const Gtid_set* slave_gtid_executed)
{
  /**
    @todo: Clean up loop so that, among other things, we only have one
    call to send_file(). This is WL#5721.
  */
#define GOTO_ERR                                                        \
  do {                                                                  \
    DBUG_PRINT("info", ("mysql_binlog_send fails; goto err from line %d", \
                        __LINE__));                                     \
    goto err;                                                           \
  } while (0)
  LOG_INFO linfo;
  char *log_file_name = linfo.log_file_name;
  char search_file_name[FN_REFLEN], *name;

  ulong ev_offset;
  bool using_gtid_protocol= slave_gtid_executed != NULL;
  bool searching_first_gtid= using_gtid_protocol;
  bool skip_group= false;
  bool binlog_has_previous_gtids_log_event= false;
  Sid_map *sid_map= slave_gtid_executed ? slave_gtid_executed->get_sid_map() : NULL;

  IO_CACHE log;
  File file = -1;
  String* packet = &thd->packet;
  int error;
  const char *errmsg = "Unknown error";
  char error_text[MAX_SLAVE_ERRMSG]; // to be send to slave via my_message()
  NET* net = &thd->net;
  mysql_mutex_t *log_lock;
  mysql_cond_t *log_cond;
  bool binlog_can_be_corrupted= FALSE;
  uint8 current_checksum_alg= BINLOG_CHECKSUM_ALG_UNDEF;
  Format_description_log_event fdle(BINLOG_VERSION), *p_fdle= &fdle;

#ifndef DBUG_OFF
  int left_events = max_binlog_dump_events;
#endif
  int old_max_allowed_packet= thd->variables.max_allowed_packet;
  /*
    Dump thread sends ER_MASTER_FATAL_ERROR_READING_BINLOG instead of the real
    errors happend on master to slave when erorr is encountered.
    So set a temporary Diagnostics_area to thd. The low level error is always
    set into the temporary Diagnostics_area and be ingored. The original
    Diagnostics_area will be restored at the end of this function.
    ER_MASTER_FATAL_ERROR_READING_BINLOG will be set to the original
    Diagnostics_area.
  */
  Diagnostics_area temp_da(thd->query_id, false);
  thd->push_diagnostics_area(&temp_da);

  DBUG_ENTER("mysql_binlog_send");
  DBUG_PRINT("enter",("log_ident: '%s'  pos: %ld", log_ident, (long) pos));

  memset(&log, 0, sizeof(log));
  /* 
     heartbeat_period from @master_heartbeat_period user variable
  */
  ulonglong heartbeat_period= get_heartbeat_period(thd);
  struct timespec heartbeat_buf;
  struct timespec *heartbeat_ts= NULL;
  const LOG_POS_COORD start_coord= { log_ident, pos },
    *p_start_coord= &start_coord;
  LOG_POS_COORD coord_buf= { log_file_name, BIN_LOG_HEADER_SIZE },
    *p_coord= &coord_buf;
  if (heartbeat_period != LL(0))
  {
    heartbeat_ts= &heartbeat_buf;
    set_timespec_nsec(*heartbeat_ts, 0);
  }
  if (log_warnings > 1)
    sql_print_information("Start binlog_dump to master_thread_id(%lu) slave_server(%d), pos(%s, %lu)",
                        thd->thread_id, thd->server_id, log_ident, (ulong)pos);
  if (RUN_HOOK(binlog_transmit, transmit_start, (thd, 0/*flags*/, log_ident, pos)))
  {
    errmsg= "Failed to run hook 'transmit_start'";
    my_errno= ER_UNKNOWN_ERROR;
    GOTO_ERR;
  }

#ifndef DBUG_OFF
  if (opt_sporadic_binlog_dump_fail && (binlog_dump_count++ % 2))
  {
    errmsg = "Master fails in COM_BINLOG_DUMP because of --opt-sporadic-binlog-dump-fail";
    my_errno= ER_UNKNOWN_ERROR;
    GOTO_ERR;
  }
#endif

  if (!mysql_bin_log.is_open())
  {
    errmsg = "Binary log is not open";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }
  if (!server_id_supplied)
  {
    errmsg = "Misconfigured master - server_id was not set";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }

  if (slave_gtid_executed != NULL)
  {
    global_sid_lock->wrlock();
    if (!gtid_state->get_lost_gtids()->is_subset(slave_gtid_executed))
    {
      global_sid_lock->unlock();
      errmsg= ER(ER_MASTER_HAS_PURGED_REQUIRED_GTIDS);
      my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
      GOTO_ERR;
    }
    global_sid_lock->unlock();
  }

  name=search_file_name;
  if (log_ident[0])
    mysql_bin_log.make_log_name(search_file_name, log_ident);
  else
    name=0;					// Find first log

  linfo.index_file_offset = 0;

  if (mysql_bin_log.find_log_pos(&linfo, name, 1))
  {
    errmsg = "Could not find first log file name in binary log index file";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = &linfo;
  mysql_mutex_unlock(&LOCK_thread_count);

  if ((file=open_binlog_file(&log, log_file_name, &errmsg)) < 0)
  {
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }
  if (pos < BIN_LOG_HEADER_SIZE)
  {
    errmsg= "Client requested master to start replication from position < 4";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }
  if (pos > my_b_filelength(&log))
  {
    errmsg= "Client requested master to start replication from position > file size";
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }

  /* reset transmit packet for the fake rotate event below */
  if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
    GOTO_ERR;

  /*
    Tell the client about the log name with a fake Rotate event;
    this is needed even if we also send a Format_description_log_event
    just after, because that event does not contain the binlog's name.
    Note that as this Rotate event is sent before
    Format_description_log_event, the slave cannot have any info to
    understand this event's format, so the header len of
    Rotate_log_event is FROZEN (so in 5.0 it will have a header shorter
    than other events except FORMAT_DESCRIPTION_EVENT).
    Before 4.0.14 we called fake_rotate_event below only if (pos ==
    BIN_LOG_HEADER_SIZE), because if this is false then the slave
    already knows the binlog's name.
    Since, we always call fake_rotate_event; if the slave already knew
    the log's name (ex: CHANGE MASTER TO MASTER_LOG_FILE=...) this is
    useless but does not harm much. It is nice for 3.23 (>=.58) slaves
    which test Rotate events to see if the master is 4.0 (then they
    choose to stop because they can't replicate 4.0); by always calling
    fake_rotate_event we are sure that 3.23.58 and newer will detect the
    problem as soon as replication starts (BUG#198).
    Always calling fake_rotate_event makes sending of normal
    (=from-binlog) Rotate events a priori unneeded, but it is not so
    simple: the 2 Rotate events are not equivalent, the normal one is
    before the Stop event, the fake one is after. If we don't send the
    normal one, then the Stop event will be interpreted (by existing 4.0
    slaves) as "the master stopped", which is wrong. So for safety,
    given that we want minimum modification of 4.0, we send the normal
    and fake Rotates.
  */
  if (fake_rotate_event(net, packet, log_file_name, pos, &errmsg,
      get_binlog_checksum_value_at_connect(current_thd)))
  {
    /*
       This error code is not perfect, as fake_rotate_event() does not
       read anything from the binlog; if it fails it's because of an
       error in my_net_write(), fortunately it will say so in errmsg.
    */
    my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
    GOTO_ERR;
  }

  /*
    Adding MAX_LOG_EVENT_HEADER_LEN, since a binlog event can become
    this larger than the corresponding packet (query) sent 
    from client to master.
  */
  thd->variables.max_allowed_packet= MAX_MAX_ALLOWED_PACKET;

  /*
    We can set log_lock now, it does not move (it's a member of
    mysql_bin_log, and it's already inited, and it will be destroyed
    only at shutdown).
  */
  p_coord->pos= pos; // the first hb matches the slave's last seen value
  log_lock= mysql_bin_log.get_log_lock();
  log_cond= mysql_bin_log.get_log_cond();
  if (pos > BIN_LOG_HEADER_SIZE)
  {
    /* reset transmit packet for the event read from binary log
       file */
    if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
      GOTO_ERR;

     /*
       Try to find a Format_description_log_event at the beginning of
       the binlog
     */
    if (!(error = Log_event::read_log_event(&log, packet, log_lock, 0)))
    { 
      DBUG_PRINT("info", ("read_log_event returned 0 on line %d", __LINE__));
      /*
        The packet has offsets equal to the normal offsets in a
        binlog event + ev_offset (the first ev_offset characters are
        the header (default \0)).
      */
      DBUG_PRINT("info",
                 ("Looked for a Format_description_log_event, found event type %s",
                  Log_event::get_type_str((Log_event_type)(*packet)[EVENT_TYPE_OFFSET + ev_offset])));
      if ((*packet)[EVENT_TYPE_OFFSET + ev_offset] == FORMAT_DESCRIPTION_EVENT)
      {
        current_checksum_alg= get_checksum_alg(packet->ptr() + ev_offset,
                                               packet->length() - ev_offset);
        DBUG_ASSERT(current_checksum_alg == BINLOG_CHECKSUM_ALG_OFF ||
                    current_checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF ||
                    current_checksum_alg == BINLOG_CHECKSUM_ALG_CRC32);
        if (!is_slave_checksum_aware(thd) &&
            current_checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
            current_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
        {
          my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
          errmsg= "Slave can not handle replication events with the checksum "
            "that master is configured to log";
          sql_print_warning("Master is configured to log replication events "
                            "with checksum, but will not send such events to "
                            "slaves that cannot process them");
          GOTO_ERR;
        }
        binlog_can_be_corrupted= test((*packet)[FLAGS_OFFSET+ev_offset] &
                                      LOG_EVENT_BINLOG_IN_USE_F);
        (*packet)[FLAGS_OFFSET+ev_offset] &= ~LOG_EVENT_BINLOG_IN_USE_F;
        /*
          mark that this event with "log_pos=0", so the slave
          should not increment master's binlog position
          (rli->group_master_log_pos)
        */
        int4store((char*) packet->ptr()+LOG_POS_OFFSET+ev_offset, 0);
        /*
          if reconnect master sends FD event with `created' as 0
          to avoid destroying temp tables.
        */
        int4store((char*) packet->ptr()+LOG_EVENT_MINIMAL_HEADER_LEN+
                  ST_CREATED_OFFSET+ev_offset, (ulong) 0);

        /* fix the checksum due to latest changes in header */
        if (current_checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
            current_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
          fix_checksum(packet, ev_offset);

        /* send it */
        if (my_net_write(net, (uchar*) packet->ptr(), packet->length()))
        {
          errmsg = "Failed on my_net_write()";
          my_errno= ER_UNKNOWN_ERROR;
          GOTO_ERR;
        }

        /*
          No need to save this event. We are only doing simple reads
          (no real parsing of the events) so we don't need it. And so
          we don't need the artificial Format_description_log_event of
          3.23&4.x.
        */
      }
    }
    else
    {
      if (test_for_non_eof_log_read_errors(error, &errmsg))
        GOTO_ERR;
      /*
        It's EOF, nothing to do, go on reading next events, the
        Format_description_log_event will be found naturally if it is written.
      */
    }
  } /* end of if (pos > BIN_LOG_HEADER_SIZE); */
  else
  {
    /* The Format_description_log_event event will be found naturally. */
  }

  /* seek to the requested position, to start the requested dump */
  my_b_seek(&log, pos);			// Seek will done on next read

  while (!net->error && net->vio != 0 && !thd->killed)
  {
    Log_event_type event_type= UNKNOWN_EVENT;
    bool goto_next_binlog= false;

    /* reset the transmit packet for the event read from binary log
       file */
    if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
      GOTO_ERR;

    while (!(error= Log_event::read_log_event(&log, packet, log_lock,
                                              current_checksum_alg)))
    {
      DBUG_PRINT("info", ("read_log_event returned 0 on line %d", __LINE__));
#ifndef DBUG_OFF
      if (max_binlog_dump_events && !left_events--)
      {
        net_flush(net);
        errmsg = "Debugging binlog dump abort";
        my_errno= ER_UNKNOWN_ERROR;
        GOTO_ERR;
      }
#endif
      /*
        log's filename does not change while it's active
      */
      p_coord->pos= uint4korr(packet->ptr() + ev_offset + LOG_POS_OFFSET);

      event_type= (Log_event_type)((*packet)[LOG_EVENT_OFFSET+ev_offset]);
      DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                      {
                        if (event_type == XID_EVENT)
                        {
                          net_flush(net);
                          const char act[]=
                            "now "
                            "wait_for signal.continue";
                          DBUG_ASSERT(opt_debug_sync_timeout > 0);
                          DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                             STRING_WITH_LEN(act)));
                        }
                      });

      switch (event_type)
      {
      case FORMAT_DESCRIPTION_EVENT:
        skip_group= false;
        current_checksum_alg= get_checksum_alg(packet->ptr() + ev_offset,
                                               packet->length() - ev_offset);
        DBUG_ASSERT(current_checksum_alg == BINLOG_CHECKSUM_ALG_OFF ||
                    current_checksum_alg == BINLOG_CHECKSUM_ALG_UNDEF ||
                    current_checksum_alg == BINLOG_CHECKSUM_ALG_CRC32);
        if (!is_slave_checksum_aware(thd) &&
            current_checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
            current_checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF)
        {
          my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
          errmsg= "Slave can not handle replication events with the checksum "
            "that master is configured to log";
          sql_print_warning("Master is configured to log replication events "
                            "with checksum, but will not send such events to "
                            "slaves that cannot process them");
          GOTO_ERR;
        }
        binlog_can_be_corrupted= test((*packet)[FLAGS_OFFSET+ev_offset] &
                                      LOG_EVENT_BINLOG_IN_USE_F);
        (*packet)[FLAGS_OFFSET+ev_offset] &= ~LOG_EVENT_BINLOG_IN_USE_F;
        /*
          Fixes the information on the checksum algorithm when a new
          format description is read. Notice that this only necessary
          when we need to filter out some transactions which were
          already processed.
        */
        p_fdle->checksum_alg= current_checksum_alg;
        break;

      case ANONYMOUS_GTID_LOG_EVENT:
        /* do nothing */
        break;
      case GTID_LOG_EVENT:
        if (gtid_mode == 0)
        {
          my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
          errmsg= ER(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF);
          GOTO_ERR;
        }
        if (using_gtid_protocol)
        {
          /*
            The current implementation checks if the GTID was not processed
            by the slave. This means that everytime a GTID is read, one needs
            to check if it was already processed by the slave. If this is the
            case, the group is not sent. Otherwise, it must be sent.

            I think we can do better than that. /Alfranio.
          */
          ulonglong checksum_size=
            ((p_fdle->checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
              p_fdle->checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF) ?
             BINLOG_CHECKSUM_LEN + ev_offset : ev_offset);
          /**
            @todo: use a local sid_map to avoid the lookup in the
            global one here /Sven
          */
          Gtid_log_event gtid_ev(packet->ptr() + ev_offset,
                                 packet->length() - checksum_size,
                                 p_fdle);
          skip_group= slave_gtid_executed->contains_gtid(gtid_ev.get_sidno(sid_map),
                                                     gtid_ev.get_gno());
          searching_first_gtid= skip_group;
          DBUG_PRINT("info", ("Dumping GTID sidno(%d) gno(%lld) skip group(%d) "
                              "searching gtid(%d).",
                              gtid_ev.get_sidno(sid_map), gtid_ev.get_gno(),
                              skip_group, searching_first_gtid));
        }
        break;

      case STOP_EVENT:
        binlog_can_be_corrupted= false;
        /* FALLTHROUGH */
      case INCIDENT_EVENT:
        skip_group= searching_first_gtid;
        break;

      case PREVIOUS_GTIDS_LOG_EVENT:
        binlog_has_previous_gtids_log_event= true;
        if (gtid_mode == 0)
        {
          my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
          errmsg= ER(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF);
          GOTO_ERR;
        }
        /* FALLTHROUGH */
      case ROTATE_EVENT:
        skip_group= false;
        break;

      default:
        if (!binlog_has_previous_gtids_log_event && using_gtid_protocol)
          /*
            If we come here, it means we are seeing a 'normal' DML/DDL
            event (e.g. query_log_event) without having seen any
            Previous_gtid_log_event. That means we are in an old
            binlog (no previous_gtids_log_event). When using the GTID
            protocol, that means we must skip the entire binary log
            and jump to the next one.
          */
          goto_next_binlog= true;

        break;
      }

      if (goto_next_binlog)
        // stop reading from this binlog
        break;

      DBUG_PRINT("info", ("EVENT_TYPE %d SEARCHING %d SKIP_GROUP %d file %s pos %lld\n",
                 event_type, searching_first_gtid, skip_group, log_file_name,
                 my_b_tell(&log)));

      /*
        Introduced this code to make the gcc 4.6.1 compiler happy. When
        warnings are converted to errors, the compiler complains about
        the fact that binlog_can_be_corrupted is defined but never used.

        We need to check if this is a dead code or if someone removed any
        code by mistake.

        /Alfranio
      */
      if (binlog_can_be_corrupted)
      {
        /*
           Don't try to print out warning messages because this generates
           erroneous messages in the error log and causes performance
           problems.

           /Alfranio
        */
      }

      pos = my_b_tell(&log);
      if (RUN_HOOK(binlog_transmit, before_send_event,
                   (thd, 0/*flags*/, packet, log_file_name, pos)))
      {
        my_errno= ER_UNKNOWN_ERROR;
        errmsg= "run 'before_send_event' hook failed";
        GOTO_ERR;
      }

      if (skip_group == false && my_net_write(net, (uchar*) packet->ptr(), packet->length()))
      {
        errmsg = "Failed on my_net_write()";
        my_errno= ER_UNKNOWN_ERROR;
        GOTO_ERR;
      }

      DBUG_EXECUTE_IF("dump_thread_wait_before_send_xid",
                      {
                        if (event_type == XID_EVENT)
                        {
                          net_flush(net);
                        }
                      });

      DBUG_PRINT("info", ("log event code %d", event_type));
      if (skip_group == false && event_type == LOAD_EVENT)
      {
	if (send_file(thd))
	{
          errmsg = "failed in send_file()";
          my_errno= ER_UNKNOWN_ERROR;
          GOTO_ERR;
	}
      }

      if (RUN_HOOK(binlog_transmit, after_send_event, (thd, 0/*flags*/, packet)))
      {
        errmsg= "Failed to run hook 'after_send_event'";
        my_errno= ER_UNKNOWN_ERROR;
        GOTO_ERR;
      }

      /* reset transmit packet for next loop */
      if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
        GOTO_ERR;
    }

    /*
      TODO: now that we are logging the offset, check to make sure
      the recorded offset and the actual match.
      Guilhem 2003-06: this is not true if this master is a slave
      <4.0.15 running with --log-slave-updates, because then log_pos may
      be the offset in the-master-of-this-master's binlog.
    */
    if (test_for_non_eof_log_read_errors(error, &errmsg))
      GOTO_ERR;

    if (mysql_bin_log.is_active(log_file_name) && !goto_next_binlog)
    {
      /*
        Block until there is more data in the log
      */
      if (net_flush(net))
      {
        errmsg = "failed on net_flush()";
        my_errno= ER_UNKNOWN_ERROR;
        GOTO_ERR;
      }

      /*
	We may have missed the update broadcast from the log
	that has just happened, let's try to catch it if it did.
	If we did not miss anything, we just wait for other threads
	to signal us.
      */
      {
	log.error=0;
	bool read_packet = 0;

#ifndef DBUG_OFF
        if (max_binlog_dump_events && !left_events--)
        {
          errmsg = "Debugging binlog dump abort";
          my_errno= ER_UNKNOWN_ERROR;
          GOTO_ERR;
        }
#endif

        /* reset the transmit packet for the event read from binary log
           file */
        if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
          GOTO_ERR;
        
	/*
	  No one will update the log while we are reading
	  now, but we'll be quick and just read one record

	  TODO:
          Add an counter that is incremented for each time we update the
          binary log.  We can avoid the following read if the counter
          has not been updated since last read.
	*/

        mysql_mutex_lock(log_lock);
        switch (error= Log_event::read_log_event(&log, packet, (mysql_mutex_t*) 0,
                                                 current_checksum_alg)) {
	case 0:
          DBUG_PRINT("info", ("read_log_event returned 0 on line %d",
                              __LINE__));
	  /* we read successfully, so we'll need to send it to the slave */
          mysql_mutex_unlock(log_lock);
	  read_packet = 1;
          p_coord->pos= uint4korr(packet->ptr() + ev_offset + LOG_POS_OFFSET);
          event_type= (Log_event_type)((*packet)[LOG_EVENT_OFFSET+ev_offset]);
          DBUG_ASSERT(event_type != FORMAT_DESCRIPTION_EVENT);
	  break;

	case LOG_READ_EOF:
        {
          int ret;
          ulong signal_cnt;
	  DBUG_PRINT("wait",("waiting for data in binary log"));
	  if (thd->server_id==0) // for mysqlbinlog (mysqlbinlog.server_id==0)
	  {
            mysql_mutex_unlock(log_lock);
	    goto end;
	  }

#ifndef DBUG_OFF
          ulong hb_info_counter= 0;
#endif
          PSI_stage_info old_stage;
          signal_cnt= mysql_bin_log.signal_cnt;
          do 
          {
            if (heartbeat_period != 0)
            {
              DBUG_ASSERT(heartbeat_ts);
              set_timespec_nsec(*heartbeat_ts, heartbeat_period);
            }
            thd->ENTER_COND(log_cond, log_lock,
                            &stage_master_has_sent_all_binlog_to_slave,
                            &old_stage);
            ret= mysql_bin_log.wait_for_update_bin_log(thd, heartbeat_ts);
            DBUG_ASSERT(ret == 0 || (heartbeat_period != 0));
            if (ret == ETIMEDOUT || ret == ETIME)
            {
#ifndef DBUG_OFF
              if (hb_info_counter < 3)
              {
                sql_print_information("master sends heartbeat message");
                hb_info_counter++;
                if (hb_info_counter == 3)
                  sql_print_information("the rest of heartbeat info skipped ...");
              }
#endif
              /* reset transmit packet for the heartbeat event */
              if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
              {
                thd->EXIT_COND(&old_stage);
                GOTO_ERR;
              }
              if (send_heartbeat_event(net, packet, p_coord, current_checksum_alg))
              {
                errmsg = "Failed on my_net_write()";
                my_errno= ER_UNKNOWN_ERROR;
                thd->EXIT_COND(&old_stage);
                GOTO_ERR;
              }
            }
            else
            {
              DBUG_PRINT("wait",("binary log received update or a broadcast signal caught"));
            }
          } while (signal_cnt == mysql_bin_log.signal_cnt && !thd->killed);
          thd->EXIT_COND(&old_stage);
        }
        break;
            
        default:
          mysql_mutex_unlock(log_lock);
          test_for_non_eof_log_read_errors(error, &errmsg);
          GOTO_ERR;
        }

        if (read_packet)
        {
          switch (event_type)
          {
          case ANONYMOUS_GTID_LOG_EVENT:
            /* do nothing */
            break;
          case GTID_LOG_EVENT:
            if (gtid_mode == 0)
            {
              my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
              errmsg= ER(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF);
              GOTO_ERR;
            }
            if (using_gtid_protocol)
            {
              ulonglong checksum_size=
                ((p_fdle->checksum_alg != BINLOG_CHECKSUM_ALG_OFF &&
                  p_fdle->checksum_alg != BINLOG_CHECKSUM_ALG_UNDEF) ?
                 BINLOG_CHECKSUM_LEN + ev_offset : ev_offset);
              Gtid_log_event gtid_ev(packet->ptr() + ev_offset,
                                     packet->length() - checksum_size,
                                     p_fdle);
              skip_group=
                slave_gtid_executed->contains_gtid(gtid_ev.get_sidno(sid_map),
                                               gtid_ev.get_gno());
              searching_first_gtid= skip_group;
              DBUG_PRINT("info", ("Dumping GTID sidno(%d) gno(%lld) "
                                  "skip group(%d) searching gtid(%d).",
                                  gtid_ev.get_sidno(sid_map), gtid_ev.get_gno(),
                                  skip_group, searching_first_gtid));
            }
            break;

          case STOP_EVENT:
            binlog_can_be_corrupted= false;
            /* FALLTHROUGH */
          case INCIDENT_EVENT:
            skip_group= searching_first_gtid;
            break;

          case PREVIOUS_GTIDS_LOG_EVENT:
            binlog_has_previous_gtids_log_event= true;
            if (gtid_mode == 0)
            {
              my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
              errmsg= ER(ER_FOUND_GTID_EVENT_WHEN_GTID_MODE_IS_OFF);
              GOTO_ERR;
            }
            /* FALLTHROUGH */
          case ROTATE_EVENT:
            skip_group= false;
            break;
          default:
            if (!binlog_has_previous_gtids_log_event && using_gtid_protocol)
              /*
                If we come here, it means we are seeing a 'normal' DML/DDL
                event (e.g. query_log_event) without having seen any
                Previous_gtid_log_event. That means we are in an old
                binlog (no previous_gtids_log_event). When using the GTID
                protocol, that means we must skip the entire binary log
                and jump to the next one.
              */
              goto_next_binlog= true;

            break;
          }

          if (!skip_group && !goto_next_binlog)
          {
            THD_STAGE_INFO(thd, stage_sending_binlog_event_to_slave);
            pos = my_b_tell(&log);
            if (RUN_HOOK(binlog_transmit, before_send_event,
                         (thd, 0/*flags*/, packet, log_file_name, pos)))
            {
              my_errno= ER_UNKNOWN_ERROR;
              errmsg= "run 'before_send_event' hook failed";
              GOTO_ERR;
            }

            if (my_net_write(net, (uchar*) packet->ptr(), packet->length()) )
            {
             errmsg = "Failed on my_net_write()";
             my_errno= ER_UNKNOWN_ERROR;
             GOTO_ERR;
            }

            if (event_type == LOAD_EVENT)
            {
              if (send_file(thd))
              {
                errmsg = "failed in send_file()";
                my_errno= ER_UNKNOWN_ERROR;
                GOTO_ERR;
              }
            }

            if (RUN_HOOK(binlog_transmit, after_send_event, (thd, 0/*flags*/, packet)))
            {
              my_errno= ER_UNKNOWN_ERROR;
              errmsg= "Failed to run hook 'after_send_event'";
              GOTO_ERR;
            }
          }
        }

        log.error=0;
      }
    }
    else
      goto_next_binlog= true;

    if (goto_next_binlog)
    {
      // need this to break out of the for loop from switch
      bool loop_breaker = 0;

      // clear flag because we open a new binlog
      binlog_has_previous_gtids_log_event= false;

      THD_STAGE_INFO(thd, stage_finished_reading_one_binlog_switching_to_next_binlog);
      switch (mysql_bin_log.find_next_log(&linfo, 1)) {
      case 0:
        break;
      case LOG_INFO_EOF:
        if (mysql_bin_log.is_active(log_file_name))
        {
          loop_breaker= 0;
          break;
        }
      default:
        errmsg = "could not find next log";
        my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
        GOTO_ERR;
      }

      if (loop_breaker)
        break;

      end_io_cache(&log);
      mysql_file_close(file, MYF(MY_WME));

      /* reset transmit packet for the possible fake rotate event */
      if (reset_transmit_packet(thd, 0/*flags*/, &ev_offset, &errmsg))
        GOTO_ERR;
      
      /*
        Call fake_rotate_event() in case the previous log (the one which
        we have just finished reading) did not contain a Rotate event.
        There are at least two cases when this can happen:

        - The previous binary log was the last one before the master was
          shutdown and restarted.

        - The previous binary log was GTID-free (did not contain a
          Previous_gtids_log_event) and the slave is connecting using
          the GTID protocol.

        This way we tell the slave about the new log's name and
        position.  If the binlog is 5.0 or later, the next event we
        are going to read and send is Format_description_log_event.
      */
      if ((file=open_binlog_file(&log, log_file_name, &errmsg)) < 0 ||
          fake_rotate_event(net, packet, log_file_name, BIN_LOG_HEADER_SIZE,
                            &errmsg, current_checksum_alg))
      {
        my_errno= ER_MASTER_FATAL_ERROR_READING_BINLOG;
        GOTO_ERR;
      }

      p_coord->file_name= log_file_name; // reset to the next
    }
  }

end:
  thd->pop_diagnostics_area();
  end_io_cache(&log);
  mysql_file_close(file, MYF(MY_WME));

  (void) RUN_HOOK(binlog_transmit, transmit_stop, (thd, 0/*flags*/));
  my_eof(thd);
  THD_STAGE_INFO(thd, stage_waiting_to_finalize_termination);
  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  mysql_mutex_unlock(&LOCK_thread_count);
  thd->variables.max_allowed_packet= old_max_allowed_packet;
  DBUG_VOID_RETURN;

err:
  THD_STAGE_INFO(thd, stage_waiting_to_finalize_termination);
  if (my_errno == ER_MASTER_FATAL_ERROR_READING_BINLOG && my_b_inited(&log))
  {
    /* 
       detailing the fatal error message with coordinates 
       of the last position read.
    */
    my_snprintf(error_text, sizeof(error_text),
                "%s; the first event '%s' at %lld, "
                "the last event read from '%s' at %lld, "
                "the last byte read from '%s' at %lld.",
                errmsg,
                p_start_coord->file_name, p_start_coord->pos,
                p_coord->file_name, p_coord->pos,
                log_file_name, my_b_tell(&log));
  }
  else
  {
    strncpy(error_text, errmsg, sizeof(error_text));
    error_text[sizeof(error_text) - 1]= '\0';
  }
  end_io_cache(&log);
  (void) RUN_HOOK(binlog_transmit, transmit_stop, (thd, 0/*flags*/));
  /*
    Exclude  iteration through thread list
    this is needed for purge_logs() - it will iterate through
    thread list and update thd->current_linfo->index_file_offset
    this mutex will make sure that it never tried to update our linfo
    after we return from this stack frame
  */
  mysql_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  mysql_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    mysql_file_close(file, MYF(MY_WME));
  thd->variables.max_allowed_packet= old_max_allowed_packet;

  thd->pop_diagnostics_area();
  my_message(my_errno, error_text, MYF(0));
  DBUG_VOID_RETURN;
}


/**
  An auxiliary function extracts slave UUID.

  @param[in]    thd  THD to access a user variable
  @param[out]   value String to return UUID value.

  @return       if success value is returned else NULL is returned.
*/
String *get_slave_uuid(THD *thd, String *value)
{
  uchar name[]= "slave_uuid";

  if (value == NULL)
    return NULL;
  user_var_entry *entry=
    (user_var_entry*) my_hash_search(&thd->user_vars, name, sizeof(name)-1);
  if (entry && entry->length() > 0)
  {
    value->copy(entry->ptr(), entry->length(), NULL);
    return value;
  }
  else
    return NULL;
}

/*

  Kill all Binlog_dump threads which previously talked to the same slave
  ("same" means with the same server id). Indeed, if the slave stops, if the
  Binlog_dump thread is waiting (mysql_cond_wait) for binlog update, then it
  will keep existing until a query is written to the binlog. If the master is
  idle, then this could last long, and if the slave reconnects, we could have 2
  Binlog_dump threads in SHOW PROCESSLIST, until a query is written to the
  binlog. To avoid this, when the slave reconnects and sends COM_BINLOG_DUMP,
  the master kills any existing thread with the slave's server id (if this id is
  not zero; it will be true for real slaves, but false for mysqlbinlog when it
  sends COM_BINLOG_DUMP to get a remote binlog dump).

  SYNOPSIS
    kill_zombie_dump_threads()
    slave_uuid      the slave's UUID

*/


void kill_zombie_dump_threads(String *slave_uuid)
{
  if (slave_uuid->length() == 0)
    return;
  DBUG_ASSERT(slave_uuid->length() == UUID_LENGTH);

  mysql_mutex_lock(&LOCK_thread_count);
  THD *tmp= NULL;
  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
  {
    if ((*it) != current_thd && ((*it)->get_command() == COM_BINLOG_DUMP ||
                                 (*it)->get_command() == COM_BINLOG_DUMP_GTID))
    {
      String tmp_uuid;
      if (get_slave_uuid((*it), &tmp_uuid) != NULL &&
          !strncmp(slave_uuid->c_ptr(), tmp_uuid.c_ptr(), UUID_LENGTH))
      {
        tmp= *it;
        mysql_mutex_lock(&tmp->LOCK_thd_data);	// Lock from delete
        break;
      }
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {
    /*
      Here we do not call kill_one_thread() as
      it will be slow because it will iterate through the list
      again. We just to do kill the thread ourselves.
    */
    tmp->awake(THD::KILL_QUERY);
    mysql_mutex_unlock(&tmp->LOCK_thd_data);
  }
}


/**
  Execute a RESET MASTER statement.

  @param thd Pointer to THD object of the client thread executing the
  statement.

  @retval 0 success
  @retval 1 error
*/
int reset_master(THD* thd)
{
  if (!mysql_bin_log.is_open())
  {
    my_message(ER_FLUSH_MASTER_BINLOG_CLOSED,
               ER(ER_FLUSH_MASTER_BINLOG_CLOSED), MYF(ME_BELL+ME_WAITTANG));
    return 1;
  }

  if (mysql_bin_log.reset_logs(thd))
    return 1;
  (void) RUN_HOOK(binlog_transmit, after_reset_master, (thd, 0 /* flags */));
  return 0;
}


/**
  Execute a SHOW MASTER STATUS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_master_status(THD* thd)
{
  Protocol *protocol= thd->protocol;
  char* gtid_set_buffer= NULL;
  int gtid_set_size= 0;
  List<Item> field_list;

  DBUG_ENTER("show_binlog_info");

  global_sid_lock->wrlock();
  const Gtid_set* gtid_set= gtid_state->get_logged_gtids();
  if ((gtid_set_size= gtid_set->to_string(&gtid_set_buffer)) < 0)
  {
    global_sid_lock->unlock();
    my_eof(thd);
    my_free(gtid_set_buffer);
    DBUG_RETURN(true);
  }
  global_sid_lock->unlock();

  field_list.push_back(new Item_empty_string("File", FN_REFLEN));
  field_list.push_back(new Item_return_int("Position",20,
					   MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("Binlog_Do_DB",255));
  field_list.push_back(new Item_empty_string("Binlog_Ignore_DB",255));
  field_list.push_back(new Item_empty_string("Executed_Gtid_Set",
                                             gtid_set_size));

  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    my_free(gtid_set_buffer);
    DBUG_RETURN(true);
  }
  protocol->prepare_for_resend();

  if (mysql_bin_log.is_open())
  {
    LOG_INFO li;
    mysql_bin_log.get_current_log(&li);
    int dir_len = dirname_length(li.log_file_name);
    protocol->store(li.log_file_name + dir_len, &my_charset_bin);
    protocol->store((ulonglong) li.pos);
    protocol->store(binlog_filter->get_do_db());
    protocol->store(binlog_filter->get_ignore_db());
    protocol->store(gtid_set_buffer, &my_charset_bin);
    if (protocol->write())
    {
      my_free(gtid_set_buffer);
      DBUG_RETURN(true);
    }
  }
  my_eof(thd);
  my_free(gtid_set_buffer);
  DBUG_RETURN(false);
}


/**
  Execute a SHOW BINARY LOGS statement.

  @param thd Pointer to THD object for the client thread executing the
  statement.

  @retval FALSE success
  @retval TRUE failure
*/
bool show_binlogs(THD* thd)
{
  IO_CACHE *index_file;
  LOG_INFO cur;
  File file;
  char fname[FN_REFLEN];
  List<Item> field_list;
  uint length;
  int cur_dir_len;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_binlogs");

  if (!mysql_bin_log.is_open())
  {
    my_error(ER_NO_BINARY_LOGGING, MYF(0));
    DBUG_RETURN(TRUE);
  }

  field_list.push_back(new Item_empty_string("Log_name", 255));
  field_list.push_back(new Item_return_int("File_size", 20,
                                           MYSQL_TYPE_LONGLONG));
  if (protocol->send_result_set_metadata(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
  
  mysql_mutex_lock(mysql_bin_log.get_log_lock());
  mysql_bin_log.lock_index();
  index_file=mysql_bin_log.get_index_file();
  
  mysql_bin_log.raw_get_current_log(&cur); // dont take mutex
  mysql_mutex_unlock(mysql_bin_log.get_log_lock()); // lockdep, OK
  
  cur_dir_len= dirname_length(cur.log_file_name);

  reinit_io_cache(index_file, READ_CACHE, (my_off_t) 0, 0, 0);

  /* The file ends with EOF or empty line */
  while ((length=my_b_gets(index_file, fname, sizeof(fname))) > 1)
  {
    int dir_len;
    ulonglong file_length= 0;                   // Length if open fails
    fname[--length] = '\0';                     // remove the newline

    protocol->prepare_for_resend();
    dir_len= dirname_length(fname);
    length-= dir_len;
    protocol->store(fname + dir_len, length, &my_charset_bin);

    if (!(strncmp(fname+dir_len, cur.log_file_name+cur_dir_len, length)))
      file_length= cur.pos;  /* The active log, use the active position */
    else
    {
      /* this is an old log, open it and find the size */
      if ((file= mysql_file_open(key_file_binlog,
                                 fname, O_RDONLY | O_SHARE | O_BINARY,
                                 MYF(0))) >= 0)
      {
        file_length= (ulonglong) mysql_file_seek(file, 0L, MY_SEEK_END, MYF(0));
        mysql_file_close(file, MYF(0));
      }
    }
    protocol->store(file_length);
    if (protocol->write())
    {
      DBUG_PRINT("info", ("stopping dump thread because protocol->write failed at line %d", __LINE__));
      goto err;
    }
  }
  mysql_bin_log.unlock_index();
  my_eof(thd);
  DBUG_RETURN(FALSE);

err:
  mysql_bin_log.unlock_index();
  DBUG_RETURN(TRUE);
}

#endif /* HAVE_REPLICATION */
