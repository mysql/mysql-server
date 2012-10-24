/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* 

   TODO: print the catalog (some USE catalog.db ????).

   Standalone program to read a MySQL binary log (or relay log).

   Should be able to read any file of these categories, even with
   --start-position.
   An important fact: the Format_desc event of the log is at most the 3rd event
   of the log; if it is the 3rd then there is this combination:
   Format_desc_of_slave, Rotate_of_master, Format_desc_of_master.
*/

#define MYSQL_CLIENT
#undef MYSQL_SERVER
#include "client_priv.h"
#include "my_default.h"
#include <my_time.h>
/* That one is necessary for defines of OPTION_NO_FOREIGN_KEY_CHECKS etc */
#include "sql_priv.h"
#include <signal.h>
#include <my_dir.h>

/*
  error() is used in macro BINLOG_ERROR which is invoked in
  rpl_gtid.h, hence the early forward declaration.
*/
static void error(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
static void warning(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);

#include "rpl_gtid.h"
#include "log_event.h"
#include "log_event_old.h"
#include "sql_common.h"
#include "my_dir.h"
#include <welcome_copyright_notice.h> // ORACLE_WELCOME_COPYRIGHT_NOTICE
#include "sql_string.h"
#include "my_decimal.h"
#include "rpl_constants.h"

#include <algorithm>

using std::min;
using std::max;

#define BIN_LOG_HEADER_SIZE	4U
#define PROBE_HEADER_LEN	(EVENT_LEN_OFFSET+4)
#define INTVAR_DYNAMIC_INIT	16
#define INTVAR_DYNAMIC_INCR	1


#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES)

char server_version[SERVER_VERSION_LENGTH];
ulong server_id = 0;
/* 
  One statement can result in a sequence of several events: Intvar_log_events,
  User_var_log_events, and Rand_log_events, followed by one
  Query_log_event. If statements are filtered out, the filter has to be
  checked for the Query_log_event. So we have to buffer the Intvar,
  User_var, and Rand events and their corresponding log postions until we see 
  the Query_log_event. This dynamic array buff_ev is used to buffer a structure 
  which stores such an event and the corresponding log position.
*/

DYNAMIC_ARRAY buff_ev;

// needed by net_serv.c
ulong bytes_sent = 0L, bytes_received = 0L;
ulong mysqld_net_retry_count = 10L;
ulong open_files_limit;
ulong opt_binlog_rows_event_max_size;
uint test_flags = 0; 
static uint opt_protocol= 0;
static FILE *result_file;

#ifndef DBUG_OFF
static const char* default_dbug_option = "d:t:o,/tmp/mysqlbinlog.trace";
#endif
static const char *load_default_groups[]= { "mysqlbinlog","client",0 };

static my_bool one_database=0, disable_log_bin= 0;
static my_bool opt_hexdump= 0;
const char *base64_output_mode_names[]=
{"NEVER", "AUTO", "UNSPEC", "DECODE-ROWS", NullS};
TYPELIB base64_output_mode_typelib=
  { array_elements(base64_output_mode_names) - 1, "",
    base64_output_mode_names, NULL };
static enum_base64_output_mode opt_base64_output_mode= BASE64_OUTPUT_UNSPEC;
static char *opt_base64_output_mode_str= 0;
static my_bool opt_remote_alias= 0;
const char *remote_proto_names[]=
{"BINLOG-DUMP-NON-GTIDS", "BINLOG-DUMP-GTIDS", NullS};
TYPELIB remote_proto_typelib=
  { array_elements(remote_proto_names) - 1, "",
    remote_proto_names, NULL };
static enum enum_remote_proto {
  BINLOG_DUMP_NON_GTID= 0,
  BINLOG_DUMP_GTID= 1,
  BINLOG_LOCAL= 2
} opt_remote_proto= BINLOG_LOCAL;
static char *opt_remote_proto_str= 0;
static char *database= 0;
static char *output_file= 0;
static my_bool force_opt= 0, short_form= 0;
static my_bool debug_info_flag, debug_check_flag;
static my_bool force_if_open_opt= 1, raw_mode= 0;
static my_bool to_last_remote_log= 0, stop_never= 0;
static my_bool opt_verify_binlog_checksum= 1;
static ulonglong offset = 0;
static uint stop_never_server_id= 1;
static char* host = 0;
static int port= 0;
static uint my_end_arg;
static const char* sock= 0;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;

#ifdef HAVE_SMEM
static char *shared_memory_base_name= 0;
#endif
static char* user = 0;
static char* pass = 0;
static char *opt_bind_addr = NULL;
static char *charset= 0;

static uint verbose= 0;

static ulonglong start_position, stop_position;
#define start_position_mot ((my_off_t)start_position)
#define stop_position_mot  ((my_off_t)stop_position)

static char *start_datetime_str, *stop_datetime_str;
static my_time_t start_datetime= 0, stop_datetime= MY_TIME_T_MAX;
static ulonglong rec_count= 0;
static ushort binlog_flags = 0; 
static MYSQL* mysql = NULL;
static char* dirname_for_local_load= 0;
static uint opt_server_id_bits = 0;
static ulong opt_server_id_mask = 0;
Sid_map *global_sid_map= NULL;
Checkable_rwlock *global_sid_lock= NULL;
Gtid_set *gtid_set_included= NULL;
Gtid_set *gtid_set_excluded= NULL;


/**
  Pointer to the Format_description_log_event of the currently active binlog.

  This will be changed each time a new Format_description_log_event is
  found in the binlog. It is finally destroyed at program termination.
*/
static Format_description_log_event* glob_description_event= NULL;

/**
  Exit status for functions in this file.
*/
enum Exit_status {
  /** No error occurred and execution should continue. */
  OK_CONTINUE= 0,
  /** An error occurred and execution should stop. */
  ERROR_STOP,
  /** No error occurred but execution should stop. */
  OK_STOP
};

/*
  Options that will be used to filter out events.
*/
static char *opt_include_gtids_str= NULL,
            *opt_exclude_gtids_str= NULL;
static my_bool opt_skip_gtids= 0;
static bool filter_based_on_gtids= false;
static Exit_status dump_local_log_entries(PRINT_EVENT_INFO *print_event_info,
                                          const char* logname);
static Exit_status dump_remote_log_entries(PRINT_EVENT_INFO *print_event_info,
                                           const char* logname);
static Exit_status dump_log_entries(const char* logname);
static Exit_status safe_connect();

/*
  This strucure is used to store the event and the log postion of the events 
  which is later used to print the event details from correct log postions.
  The Log_event *event is used to store the pointer to the current event and 
  the event_pos is used to store the current event log postion.
*/

struct buff_event_info
  {
    Log_event *event;
    my_off_t event_pos;
  };

struct buff_event_info buff_event;

class Load_log_processor
{
  char target_dir_name[FN_REFLEN];
  size_t target_dir_name_len;

  /*
    When we see first event corresponding to some LOAD DATA statement in
    binlog, we create temporary file to store data to be loaded.
    We add name of this file to file_names array using its file_id as index.
    If we have Create_file event (i.e. we have binary log in pre-5.0.3
    format) we also store save event object to be able which is needed to
    emit LOAD DATA statement when we will meet Exec_load_data event.
    If we have Begin_load_query event we simply store 0 in
    File_name_record::event field.
  */
  struct File_name_record
  {
    char *fname;
    Create_file_log_event *event;
  };
  /*
    @todo Should be a map (e.g., a hash map), not an array.  With the
    present implementation, the number of elements in this array is
    about the number of files loaded since the server started, which
    may be big after a few years.  We should be able to use existing
    library data structures for this. /Sven
  */
  DYNAMIC_ARRAY file_names;

  /**
    Looks for a non-existing filename by adding a numerical suffix to
    the given base name, creates the generated file, and returns the
    filename by modifying the filename argument.

    @param[in,out] filename Base filename

    @param[in,out] file_name_end Pointer to last character of
    filename.  The numerical suffix will be written to this position.
    Note that there must be a least five bytes of allocated memory
    after file_name_end.

    @retval -1 Error (can't find new filename).
    @retval >=0 Found file.
  */
  File create_unique_file(char *filename, char *file_name_end)
    {
      File res;
      /* If we have to try more than 1000 times, something is seriously wrong */
      for (uint version= 0; version<1000; version++)
      {
	sprintf(file_name_end,"-%x",version);
	if ((res= my_create(filename,0,
			    O_CREAT|O_EXCL|O_BINARY|O_WRONLY,MYF(0)))!=-1)
	  return res;
      }
      return -1;
    }

public:
  Load_log_processor() {}
  ~Load_log_processor() {}

  int init()
  {
    return init_dynamic_array(&file_names, sizeof(File_name_record),
			      100, 100);
  }

  void init_by_dir_name(const char *dir)
    {
      target_dir_name_len= (convert_dirname(target_dir_name, dir, NullS) -
			    target_dir_name);
    }
  void init_by_cur_dir()
    {
      if (my_getwd(target_dir_name,sizeof(target_dir_name),MYF(MY_WME)))
	exit(1);
      target_dir_name_len= strlen(target_dir_name);
    }
  void destroy()
  {
    File_name_record *ptr= (File_name_record *)file_names.buffer;
    File_name_record *end= ptr + file_names.elements;
    for (; ptr < end; ptr++)
    {
      if (ptr->fname)
      {
        my_free(ptr->fname);
        delete ptr->event;
        memset(ptr, 0, sizeof(File_name_record));
      }
    }

    delete_dynamic(&file_names);
  }

  /**
    Obtain Create_file event for LOAD DATA statement by its file_id
    and remove it from this Load_log_processor's list of events.

    Checks whether we have already seen a Create_file_log_event with
    the given file_id.  If yes, returns a pointer to the event and
    removes the event from array describing active temporary files.
    From this moment, the caller is responsible for freeing the memory
    occupied by the event.

    @param[in] file_id File id identifying LOAD DATA statement.

    @return Pointer to Create_file_log_event, or NULL if we have not
    seen any Create_file_log_event with this file_id.
  */
  Create_file_log_event *grab_event(uint file_id)
    {
      File_name_record *ptr;
      Create_file_log_event *res;

      if (file_id >= file_names.elements)
        return 0;
      ptr= dynamic_element(&file_names, file_id, File_name_record*);
      if ((res= ptr->event))
        memset(ptr, 0, sizeof(File_name_record));
      return res;
    }

  /**
    Obtain file name of temporary file for LOAD DATA statement by its
    file_id and remove it from this Load_log_processor's list of events.

    @param[in] file_id Identifier for the LOAD DATA statement.

    Checks whether we have already seen Begin_load_query event for
    this file_id. If yes, returns the file name of the corresponding
    temporary file and removes the filename from the array of active
    temporary files.  From this moment, the caller is responsible for
    freeing the memory occupied by this name.

    @return String with the name of the temporary file, or NULL if we
    have not seen any Begin_load_query_event with this file_id.
  */
  char *grab_fname(uint file_id)
    {
      File_name_record *ptr;
      char *res= 0;

      if (file_id >= file_names.elements)
        return 0;
      ptr= dynamic_element(&file_names, file_id, File_name_record*);
      if (!ptr->event)
      {
        res= ptr->fname;
        memset(ptr, 0, sizeof(File_name_record));
      }
      return res;
    }
  Exit_status process(Create_file_log_event *ce);
  Exit_status process(Begin_load_query_log_event *ce);
  Exit_status process(Append_block_log_event *ae);
  File prepare_new_file_for_old_format(Load_log_event *le, char *filename);
  Exit_status load_old_format_file(NET* net, const char *server_fname,
                                   uint server_fname_len, File file);
  Exit_status process_first_event(const char *bname, size_t blen,
                                  const uchar *block,
                                  size_t block_len, uint file_id,
                                  Create_file_log_event *ce);
};


/**
  Creates and opens a new temporary file in the directory specified by previous call to init_by_dir_name() or init_by_cur_dir().

  @param[in] le The basename of the created file will start with the
  basename of the file pointed to by this Load_log_event.

  @param[out] filename Buffer to save the filename in.

  @return File handle >= 0 on success, -1 on error.
*/
File Load_log_processor::prepare_new_file_for_old_format(Load_log_event *le,
							 char *filename)
{
  size_t len;
  char *tail;
  File file;
  
  fn_format(filename, le->fname, target_dir_name, "", MY_REPLACE_DIR);
  len= strlen(filename);
  tail= filename + len;
  
  if ((file= create_unique_file(filename,tail)) < 0)
  {
    error("Could not construct local filename %s.",filename);
    return -1;
  }
  
  le->set_fname_outside_temp_buf(filename,len+(uint) strlen(tail));
  
  return file;
}


/**
  Reads a file from a server and saves it locally.

  @param[in,out] net The server to read from.

  @param[in] server_fname The name of the file that the server should
  read.

  @param[in] server_fname_len The length of server_fname.

  @param[in,out] file The file to write to.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
Exit_status Load_log_processor::load_old_format_file(NET* net,
                                                     const char*server_fname,
                                                     uint server_fname_len,
                                                     File file)
{
  uchar buf[FN_REFLEN+1];
  buf[0] = 0;
  memcpy(buf + 1, server_fname, server_fname_len + 1);
  if (my_net_write(net, buf, server_fname_len +2) || net_flush(net))
  {
    error("Failed requesting the remote dump of %s.", server_fname);
    return ERROR_STOP;
  }
  
  for (;;)
  {
    ulong packet_len = my_net_read(net);
    if (packet_len == 0)
    {
      if (my_net_write(net, (uchar*) "", 0) || net_flush(net))
      {
        error("Failed sending the ack packet.");
        return ERROR_STOP;
      }
      /*
	we just need to send something, as the server will read but
	not examine the packet - this is because mysql_load() sends 
	an OK when it is done
      */
      break;
    }
    else if (packet_len == packet_error)
    {
      error("Failed reading a packet during the dump of %s.", server_fname);
      return ERROR_STOP;
    }
    
    if (packet_len > UINT_MAX)
    {
      error("Illegal length of packet read from net.");
      return ERROR_STOP;
    }
    if (my_write(file, (uchar*) net->read_pos, 
		 (uint) packet_len, MYF(MY_WME|MY_NABP)))
      return ERROR_STOP;
  }
  
  return OK_CONTINUE;
}


/**
  Process the first event in the sequence of events representing a
  LOAD DATA statement.

  Creates a temporary file to be used in LOAD DATA and writes first
  block of data to it. Registers its file name (and optional
  Create_file event) in the array of active temporary files.

  @param bname Base name for temporary file to be created.
  @param blen Base name length.
  @param block First block of data to be loaded.
  @param block_len First block length.
  @param file_id Identifies the LOAD DATA statement.
  @param ce Pointer to Create_file event object if we are processing
  this type of event.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
Exit_status Load_log_processor::process_first_event(const char *bname,
                                                    size_t blen,
                                                    const uchar *block,
                                                    size_t block_len,
                                                    uint file_id,
                                                    Create_file_log_event *ce)
{
  uint full_len= target_dir_name_len + blen + 9 + 9 + 1;
  Exit_status retval= OK_CONTINUE;
  char *fname, *ptr;
  File file;
  File_name_record rec;
  DBUG_ENTER("Load_log_processor::process_first_event");

  if (!(fname= (char*) my_malloc(full_len,MYF(MY_WME))))
  {
    error("Out of memory.");
    delete ce;
    DBUG_RETURN(ERROR_STOP);
  }

  memcpy(fname, target_dir_name, target_dir_name_len);
  ptr= fname + target_dir_name_len;
  memcpy(ptr,bname,blen);
  ptr+= blen;
  ptr+= sprintf(ptr, "-%x", file_id);

  if ((file= create_unique_file(fname,ptr)) < 0)
  {
    error("Could not construct local filename %s%s.",
          target_dir_name,bname);
    my_free(fname);
    delete ce;
    DBUG_RETURN(ERROR_STOP);
  }

  rec.fname= fname;
  rec.event= ce;

  /*
     fname is freed in process_event()
     after Execute_load_query_log_event or Execute_load_log_event
     will have been processed, otherwise in Load_log_processor::destroy()
  */
  if (set_dynamic(&file_names, &rec, file_id))
  {
    error("Out of memory.");
    my_free(fname);
    delete ce;
    DBUG_RETURN(ERROR_STOP);
  }

  if (ce)
    ce->set_fname_outside_temp_buf(fname, (uint) strlen(fname));

  if (my_write(file, (uchar*)block, block_len, MYF(MY_WME|MY_NABP)))
  {
    error("Failed writing to file.");
    retval= ERROR_STOP;
  }
  if (my_close(file, MYF(MY_WME)))
  {
    error("Failed closing file.");
    retval= ERROR_STOP;
  }
  DBUG_RETURN(retval);
}


/**
  Process the given Create_file_log_event.

  @see Load_log_processor::process_first_event(const char*,uint,const char*,uint,uint,Create_file_log_event*)

  @param ce Create_file_log_event to process.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
Exit_status  Load_log_processor::process(Create_file_log_event *ce)
{
  const char *bname= ce->fname + dirname_length(ce->fname);
  uint blen= ce->fname_len - (bname-ce->fname);

  return process_first_event(bname, blen, ce->block, ce->block_len,
                             ce->file_id, ce);
}


/**
  Process the given Begin_load_query_log_event.

  @see Load_log_processor::process_first_event(const char*,uint,const char*,uint,uint,Create_file_log_event*)

  @param ce Begin_load_query_log_event to process.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
Exit_status Load_log_processor::process(Begin_load_query_log_event *blqe)
{
  return process_first_event("SQL_LOAD_MB", 11, blqe->block, blqe->block_len,
                             blqe->file_id, 0);
}


/**
  Process the given Append_block_log_event.

  Appends the chunk of the file contents specified by the event to the
  file created by a previous Begin_load_query_log_event or
  Create_file_log_event.

  If the file_id for the event does not correspond to any file
  previously registered through a Begin_load_query_log_event or
  Create_file_log_event, this member function will print a warning and
  return OK_CONTINUE.  It is safe to return OK_CONTINUE, because no
  query will be written for this event.  We should not print an error
  and fail, since the missing file_id could be because a (valid)
  --start-position has been specified after the Begin/Create event but
  before this Append event.

  @param ae Append_block_log_event to process.

  @retval ERROR_STOP An error occurred - the program should terminate.

  @retval OK_CONTINUE No error, the program should continue.
*/
Exit_status Load_log_processor::process(Append_block_log_event *ae)
{
  DBUG_ENTER("Load_log_processor::process");
  const char* fname= ((ae->file_id < file_names.elements) ?
                       dynamic_element(&file_names, ae->file_id,
                                       File_name_record*)->fname : 0);

  if (fname)
  {
    File file;
    Exit_status retval= OK_CONTINUE;
    if (((file= my_open(fname,
			O_APPEND|O_BINARY|O_WRONLY,MYF(MY_WME))) < 0))
    {
      error("Failed opening file %s", fname);
      DBUG_RETURN(ERROR_STOP);
    }
    if (my_write(file,(uchar*)ae->block,ae->block_len,MYF(MY_WME|MY_NABP)))
    {
      error("Failed writing to file %s", fname);
      retval= ERROR_STOP;
    }
    if (my_close(file,MYF(MY_WME)))
    {
      error("Failed closing file %s", fname);
      retval= ERROR_STOP;
    }
    DBUG_RETURN(retval);
  }

  /*
    There is no Create_file event (a bad binlog or a big
    --start-position). Assuming it's a big --start-position, we just do
    nothing and print a warning.
  */
  warning("Ignoring Append_block as there is no "
          "Create_file event for file_id: %u", ae->file_id);
  DBUG_RETURN(OK_CONTINUE);
}


static Load_log_processor load_processor;


/**
  Replace windows-style backslashes by forward slashes so it can be
  consumed by the mysql client, which requires Unix path.

  @todo This is only useful under windows, so may be ifdef'ed out on
  other systems.  /Sven

  @todo If a Create_file_log_event contains a filename with a
  backslash (valid under unix), then we have problems under windows.
  /Sven

  @param[in,out] fname Filename to modify. The filename is modified
  in-place.
*/
static void convert_path_to_forward_slashes(char *fname)
{
  while (*fname)
  {
    if (*fname == '\\')
      *fname= '/';
    fname++;
  }
}


/**
  Indicates whether the given database should be filtered out,
  according to the --database=X option.

  @param log_dbname Name of database.

  @return nonzero if the database with the given name should be
  filtered out, 0 otherwise.
*/
static bool shall_skip_database(const char *log_dbname)
{
  return one_database &&
         (log_dbname != NULL) &&
         strcmp(log_dbname, database);
}


/**
  Checks whether the given event should be filtered out,
  according to the include-gtids, exclude-gtids and
  skip-gtids options.

  @param ev Pointer to the event to be checked.

  @return true if the event should be filtered out,
          false, otherwise.
*/
static bool shall_skip_gtids(Log_event* ev)
{
  bool filtered= false;

  switch (ev->get_type_code())
  {
    case GTID_LOG_EVENT:
    case ANONYMOUS_GTID_LOG_EVENT:
    {
       Gtid_log_event *gtid= (Gtid_log_event *) ev;
       if (opt_include_gtids_str != NULL)
       {
         filtered= filtered ||
           !gtid_set_included->contains_gtid(gtid->get_sidno(true),
                                            gtid->get_gno());
       }

       if (opt_exclude_gtids_str != NULL)
       {
         filtered= filtered ||
           gtid_set_excluded->contains_gtid(gtid->get_sidno(true),
                                           gtid->get_gno());
       }
       filter_based_on_gtids= filtered;
       filtered= filtered || opt_skip_gtids;
    }
    break;
    default:
      filtered= filter_based_on_gtids;
    break;
  }
  
  return filtered;
}

/**
  Print the given event, and either delete it or delegate the deletion
  to someone else.

  The deletion may be delegated in two cases: (1) the event is a
  Format_description_log_event, and is saved in
  glob_description_event; (2) the event is a Create_file_log_event,
  and is saved in load_processor.

  @param[in,out] print_event_info Parameters and context state
  determining how to print.
  @param[in] ev Log_event to process.
  @param[in] pos Offset from beginning of binlog file.
  @param[in] logname Name of input binlog.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
  @retval OK_STOP No error, but the end of the specified range of
  events to process has been reached and the program should terminate.
*/
Exit_status process_event(PRINT_EVENT_INFO *print_event_info, Log_event *ev,
                          my_off_t pos, const char *logname)
{
  char ll_buff[21];
  Log_event_type ev_type= ev->get_type_code();
  my_bool destroy_evt= TRUE;
  DBUG_ENTER("process_event");
  print_event_info->short_form= short_form;
  Exit_status retval= OK_CONTINUE;
  IO_CACHE *const head= &print_event_info->head_cache;

  /*
    Format events are not concerned by --offset and such, we always need to
    read them to be able to process the wanted events.
  */
  if (((rec_count >= offset) &&
       ((my_time_t) (ev->when.tv_sec) >= start_datetime)) ||
      (ev_type == FORMAT_DESCRIPTION_EVENT))
  {
    if (ev_type != FORMAT_DESCRIPTION_EVENT)
    {
      /*
        We have found an event after start_datetime, from now on print
        everything (in case the binlog has timestamps increasing and
        decreasing, we do this to avoid cutting the middle).
      */
      start_datetime= 0;
      offset= 0; // print everything and protect against cycling rec_count
      /*
        Skip events according to the --server-id flag.  However, don't
        skip format_description or rotate events, because they they
        are really "global" events that are relevant for the entire
        binlog, even if they have a server_id.  Also, we have to read
        the format_description event so that we can parse subsequent
        events.
      */
      if (ev_type != ROTATE_EVENT &&
          server_id && (server_id != ev->server_id))
        goto end;
    }
    if (((my_time_t) (ev->when.tv_sec) >= stop_datetime)
        || (pos >= stop_position_mot))
    {
      /* end the program */
      retval= OK_STOP;
      goto end;
    }
    if (!short_form)
      my_b_printf(&print_event_info->head_cache,
                  "# at %s\n",llstr(pos,ll_buff));

    if (!opt_hexdump)
      print_event_info->hexdump_from= 0; /* Disabled */
    else
      print_event_info->hexdump_from= pos;

    print_event_info->base64_output_mode= opt_base64_output_mode;

    DBUG_PRINT("debug", ("event_type: %s", ev->get_type_str()));

    if (shall_skip_gtids(ev))
      goto end;

    switch (ev_type) {
    case QUERY_EVENT:
    {
      bool parent_query_skips=
          !((Query_log_event*) ev)->is_trans_keyword() &&
           shall_skip_database(((Query_log_event*) ev)->db);
           
      for (uint i= 0; i < buff_ev.elements; i++) 
      {
        buff_event_info pop_event_array= *dynamic_element(&buff_ev, i, buff_event_info *);
        Log_event *temp_event= pop_event_array.event;
        my_off_t temp_log_pos= pop_event_array.event_pos;
        print_event_info->hexdump_from= (opt_hexdump ? temp_log_pos : 0); 
        if (!parent_query_skips)
          temp_event->print(result_file, print_event_info);
        delete temp_event;
      }
      
      print_event_info->hexdump_from= (opt_hexdump ? pos : 0);
      reset_dynamic(&buff_ev);

      if (parent_query_skips)
        goto end;
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
      break;
      
      destroy_evt= TRUE;
    }
          
    case INTVAR_EVENT:
    {
      destroy_evt= FALSE;
      buff_event.event= ev;
      buff_event.event_pos= pos;
      insert_dynamic(&buff_ev, (uchar*) &buff_event);
      break;
    }
    	
    case RAND_EVENT:
    {
      destroy_evt= FALSE;
      buff_event.event= ev;
      buff_event.event_pos= pos;      
      insert_dynamic(&buff_ev, (uchar*) &buff_event);
      break;
    }
    
    case USER_VAR_EVENT:
    {
      destroy_evt= FALSE;
      buff_event.event= ev;
      buff_event.event_pos= pos;      
      insert_dynamic(&buff_ev, (uchar*) &buff_event);
      break; 
    }


    case CREATE_FILE_EVENT:
    {
      Create_file_log_event* ce= (Create_file_log_event*)ev;
      /*
        We test if this event has to be ignored. If yes, we don't save
        this event; this will have the good side-effect of ignoring all
        related Append_block and Exec_load.
        Note that Load event from 3.23 is not tested.
      */
      if (shall_skip_database(ce->db))
        goto end;                // Next event
      /*
	We print the event, but with a leading '#': this is just to inform 
	the user of the original command; the command we want to execute 
	will be a derivation of this original command (we will change the 
	filename and use LOCAL), prepared in the 'case EXEC_LOAD_EVENT' 
	below.
      */
      {
      ce->print(result_file, print_event_info, TRUE);
        if (head->error == -1)
          goto err;
      }
      // If this binlog is not 3.23 ; why this test??
      if (glob_description_event->binlog_version >= 3)
      {
        /*
          transfer the responsibility for destroying the event to
          load_processor
        */
        ev= NULL;
        if ((retval= load_processor.process(ce)) != OK_CONTINUE)
          goto end;
      }
      break;
    }

    case APPEND_BLOCK_EVENT:
      /*
        Append_block_log_events can safely print themselves even if
        the subsequent call load_processor.process fails, because the
        output of Append_block_log_event::print is only a comment.
      */
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
      if ((retval= load_processor.process((Append_block_log_event*) ev)) !=
          OK_CONTINUE)
        goto end;
      break;

    case EXEC_LOAD_EVENT:
    {
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
      Execute_load_log_event *exv= (Execute_load_log_event*)ev;
      Create_file_log_event *ce= load_processor.grab_event(exv->file_id);
      /*
	if ce is 0, it probably means that we have not seen the Create_file
	event (a bad binlog, or most probably --start-position is after the
	Create_file event). Print a warning comment.
      */
      if (ce)
      {
        /*
          We must not convert earlier, since the file is used by
          my_open() in Load_log_processor::append().
        */
        convert_path_to_forward_slashes((char*) ce->fname);
	ce->print(result_file, print_event_info, TRUE);
	my_free((void*)ce->fname);
	delete ce;
        if (head->error == -1)
          goto err;
      }
      else
        warning("Ignoring Execute_load_log_event as there is no "
                "Create_file event for file_id: %u", exv->file_id);
      break;
    }
    case FORMAT_DESCRIPTION_EVENT:
      delete glob_description_event;
      glob_description_event= (Format_description_log_event*) ev;
      print_event_info->common_header_len=
        glob_description_event->common_header_len;
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
      if (opt_remote_proto == BINLOG_LOCAL)
      {
        ev->free_temp_buf(); // free memory allocated in dump_local_log_entries
      }
      else
      {
        /*
          disassociate but not free dump_remote_log_entries time memory
        */
        ev->temp_buf= 0;
      }
      /*
        We don't want this event to be deleted now, so let's hide it (I
        (Guilhem) should later see if this triggers a non-serious Valgrind
        error). Not serious error, because we will free description_event
        later.
      */
      ev= 0;
      if (!force_if_open_opt &&
          (glob_description_event->flags & LOG_EVENT_BINLOG_IN_USE_F))
      {
        error("Attempting to dump binlog '%s', which was not closed properly. "
              "Most probably, mysqld is still writing it, or it crashed. "
              "Rerun with --force-if-open to ignore this problem.", logname);
        DBUG_RETURN(ERROR_STOP);
      }
      break;
    case BEGIN_LOAD_QUERY_EVENT:
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
      if ((retval= load_processor.process((Begin_load_query_log_event*) ev)) !=
          OK_CONTINUE)
        goto end;
      break;
    case EXECUTE_LOAD_QUERY_EVENT:
    {
      Execute_load_query_log_event *exlq= (Execute_load_query_log_event*)ev;
      char *fname= load_processor.grab_fname(exlq->file_id);

      if (!shall_skip_database(exlq->db))
      {
        if (fname)
        {
          convert_path_to_forward_slashes(fname);
          exlq->print(result_file, print_event_info, fname);
          if (head->error == -1)
          {
            if (fname)
              my_free(fname);
            goto err;
          }
        }
        else
          warning("Ignoring Execute_load_query since there is no "
                  "Begin_load_query event for file_id: %u", exlq->file_id);
      }

      if (fname)
	my_free(fname);
      break;
    }
    case TABLE_MAP_EVENT:
    {
      Table_map_log_event *map= ((Table_map_log_event *)ev);
      if (shall_skip_database(map->get_db_name()))
      {
        print_event_info->m_table_map_ignored.set_table(map->get_table_id(), map);
        destroy_evt= FALSE;
        goto end;
      }
    }
    case ROWS_QUERY_LOG_EVENT:
    case WRITE_ROWS_EVENT:
    case DELETE_ROWS_EVENT:
    case UPDATE_ROWS_EVENT:
    case WRITE_ROWS_EVENT_V1:
    case UPDATE_ROWS_EVENT_V1:
    case DELETE_ROWS_EVENT_V1:
    case PRE_GA_WRITE_ROWS_EVENT:
    case PRE_GA_DELETE_ROWS_EVENT:
    case PRE_GA_UPDATE_ROWS_EVENT:
    {
      bool stmt_end= FALSE;
      Table_map_log_event *ignored_map= NULL;
      if (ev_type == WRITE_ROWS_EVENT ||
          ev_type == DELETE_ROWS_EVENT ||
          ev_type == UPDATE_ROWS_EVENT ||
          ev_type == WRITE_ROWS_EVENT_V1 ||
          ev_type == DELETE_ROWS_EVENT_V1 ||
          ev_type == UPDATE_ROWS_EVENT_V1)
      {
        Rows_log_event *new_ev= (Rows_log_event*) ev;
        if (new_ev->get_flags(Rows_log_event::STMT_END_F))
          stmt_end= TRUE;
        ignored_map= print_event_info->m_table_map_ignored.get_table(new_ev->get_table_id());
      }
      else if (ev_type == PRE_GA_WRITE_ROWS_EVENT ||
               ev_type == PRE_GA_DELETE_ROWS_EVENT ||
               ev_type == PRE_GA_UPDATE_ROWS_EVENT)
      {
        Old_rows_log_event *old_ev= (Old_rows_log_event*) ev;
        if (old_ev->get_flags(Rows_log_event::STMT_END_F))
          stmt_end= TRUE;
        ignored_map= print_event_info->m_table_map_ignored.get_table(old_ev->get_table_id());
      }

      bool skip_event= (ignored_map != NULL);
      /*
        end of statement check:
           i) destroy/free ignored maps
          ii) if skip event, flush cache now
      */
      if (stmt_end)
      {
        /*
          Now is safe to clear ignored map (clear_tables will also
          delete original table map events stored in the map).
        */
        if (print_event_info->m_table_map_ignored.count() > 0)
          print_event_info->m_table_map_ignored.clear_tables();

        /*
           One needs to take into account an event that gets
           filtered but was last event in the statement. If this is
           the case, previous rows events that were written into
           IO_CACHEs still need to be copied from cache to
           result_file (as it would happen in ev->print(...) if
           event was not skipped).
        */
        if (skip_event)
          if ((copy_event_cache_to_file_and_reinit(&print_event_info->head_cache, result_file) ||
              copy_event_cache_to_file_and_reinit(&print_event_info->body_cache, result_file)))
            goto err;
      }

      /* skip the event check */
      if (skip_event)
        goto end;

      /*
        These events must be printed in base64 format, if printed.
        base64 format requires a FD event to be safe, so if no FD
        event has been printed, we give an error.  Except if user
        passed --short-form, because --short-form disables printing
        row events.
      */
      if (!print_event_info->printed_fd_event && !short_form &&
          ev_type != TABLE_MAP_EVENT && ev_type != ROWS_QUERY_LOG_EVENT &&
          opt_base64_output_mode != BASE64_OUTPUT_DECODE_ROWS)
      {
        const char* type_str= ev->get_type_str();
        if (opt_base64_output_mode == BASE64_OUTPUT_NEVER)
          error("--base64-output=never specified, but binlog contains a "
                "%s event which must be printed in base64.",
                type_str);
        else
          error("malformed binlog: it does not contain any "
                "Format_description_log_event. I now found a %s event, which "
                "is not safe to process without a "
                "Format_description_log_event.",
                type_str);
        goto err;
      }

      ev->print(result_file, print_event_info);
      print_event_info->have_unflushed_events= TRUE;
      /* Flush head and body cache to result_file */
      if (stmt_end)
      {
        print_event_info->have_unflushed_events= FALSE;
        if (copy_event_cache_to_file_and_reinit(&print_event_info->head_cache, result_file) ||
            copy_event_cache_to_file_and_reinit(&print_event_info->body_cache, result_file))
          goto err;
        goto end;
      }
      break;
    }
    default:
      ev->print(result_file, print_event_info);
      if (head->error == -1)
        goto err;
    }
    /* Flush head cache to result_file for every event */
    if (copy_event_cache_to_file_and_reinit(&print_event_info->head_cache,
                                            result_file))
      goto err;
  }

  goto end;

err:
  retval= ERROR_STOP;
end:
  rec_count++;
  /*
    Destroy the log_event object. If reading from a remote host,
    set the temp_buf to NULL so that memory isn't freed twice.
  */
  if (ev)
  {
    if (opt_remote_proto != BINLOG_LOCAL)
      ev->temp_buf= 0;
    if (destroy_evt) /* destroy it later if not set (ignored table map) */
      delete ev;
  }
  DBUG_RETURN(retval);
}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"base64-output", OPT_BASE64_OUTPUT_MODE,
    /* 'unspec' is not mentioned because it is just a placeholder. */
   "Determine when the output statements should be base64-encoded BINLOG "
   "statements: 'never' disables it and works only for binlogs without "
   "row-based events; 'decode-rows' decodes row events into commented pseudo-SQL "
   "statements if the --verbose option is also given; 'auto' prints base64 "
   "only when necessary (i.e., for row-based events and format description "
   "events).  If no --base64-output[=name] option is given at all, the "
   "default is 'auto'.",
   &opt_base64_output_mode_str, &opt_base64_output_mode_str,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"bind-address", 0, "IP address to bind to.",
   (uchar**) &opt_bind_addr, (uchar**) &opt_bind_addr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  /*
    mysqlbinlog needs charsets knowledge, to be able to convert a charset
    number found in binlog to a charset name (to be able to print things
    like this:
    SET @`a`:=_cp850 0x4DFC6C6C6572 COLLATE `cp850_general_ci`;
  */
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'd', "List entries for just this database (local log only).",
   &database, &database, 0, GET_STR_ALLOC, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#ifndef DBUG_OFF
  {"debug", '#', "Output debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit .",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   &debug_info_flag, &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", OPT_DEFAULT_AUTH,
   "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"disable-log-bin", 'D', "Disable binary log. This is useful, if you "
    "enabled --to-last-log and are sending the output to the same MySQL server. "
    "This way you could avoid an endless loop. You would also like to use it "
    "when restoring after a crash to avoid duplication of the statements you "
    "already have. NOTE: you will need a SUPER privilege to use this option.",
   &disable_log_bin, &disable_log_bin, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force-if-open", 'F', "Force if binlog was not closed properly.",
   &force_if_open_opt, &force_if_open_opt, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
  {"force-read", 'f', "Force reading unknown binlog events.",
   &force_opt, &force_opt, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"hexdump", 'H', "Augment output with hexadecimal and ASCII event dump.",
   &opt_hexdump, &opt_hexdump, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"host", 'h', "Get the binlog from server.", &host, &host,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"local-load", 'l', "Prepare local temporary files for LOAD DATA INFILE in the specified directory.",
   &dirname_for_local_load, &dirname_for_local_load, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"offset", 'o', "Skip the first N entries.", &offset, &offset,
   0, GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p', "Password to connect to remote server.",
   0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
    &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &port, &port, 0, GET_INT, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
   "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"read-from-remote-server", 'R', "Read binary logs from a MySQL server. "
   "This is an alias for read-from-remote-master=BINLOG-DUMP-NON-GTIDS.",
   &opt_remote_alias, &opt_remote_alias, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"read-from-remote-master", OPT_REMOTE_PROTO,
   "Read binary logs from a MySQL server through the COM_BINLOG_DUMP or "
   "COM_BINLOG_DUMP_GTID commands by setting the option to either "
   "BINLOG-DUMP-NON-GTIDS or BINLOG-DUMP-GTIDS, respectively. If "
   "--read-from-remote-master=BINLOG-DUMP-GTIDS is combined with "
   "--exclude-gtids, transactions can be filtered out on the master "
   "avoiding unnecessary network traffic.",
   &opt_remote_proto_str, &opt_remote_proto_str, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"raw", OPT_RAW_OUTPUT, "Requires -R. Output raw binlog data instead of SQL "
   "statements, output is to log files.",
   &raw_mode, &raw_mode, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"result-file", 'r', "Direct output to a given file. With --raw this is a "
   "prefix for the file names.",
   &output_file, &output_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"server-id", OPT_SERVER_ID,
   "Extract only binlog entries created by the server having the given id.",
   &server_id, &server_id, 0, GET_ULONG,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-id-bits", 0,
   "Set number of significant bits in server-id",
   &opt_server_id_bits, &opt_server_id_bits,
   /* Default + Max 32 bits, minimum 7 bits */
   0, GET_UINT, REQUIRED_ARG, 32, 7, 32, 0, 0, 0},
  {"set-charset", OPT_SET_CHARSET,
   "Add 'SET NAMES character_set' to the output.", &charset,
   &charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name,
   &shared_memory_base_name,
   0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"short-form", 's', "Just show regular queries: no extra info and no "
   "row-based events. This is for testing only, and should not be used in "
   "production systems. If you want to suppress base64-output, consider "
   "using --base64-output=never instead.",
   &short_form, &short_form, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &sock, &sock, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"start-datetime", OPT_START_DATETIME,
   "Start reading the binlog at first event having a datetime equal or "
   "posterior to the argument; the argument must be a date and time "
   "in the local time zone, in any format accepted by the MySQL server "
   "for DATETIME and TIMESTAMP types, for example: 2004-12-25 11:25:56 "
   "(you should probably use quotes for your shell to set it properly).",
   &start_datetime_str, &start_datetime_str,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"start-position", 'j',
   "Start reading the binlog at position N. Applies to the first binlog "
   "passed on the command line.",
   &start_position, &start_position, 0, GET_ULL,
   REQUIRED_ARG, BIN_LOG_HEADER_SIZE, BIN_LOG_HEADER_SIZE,
   /* COM_BINLOG_DUMP accepts only 4 bytes for the position */
   (ulonglong)(~(uint32)0), 0, 0, 0},
  {"stop-datetime", OPT_STOP_DATETIME,
   "Stop reading the binlog at first event having a datetime equal or "
   "posterior to the argument; the argument must be a date and time "
   "in the local time zone, in any format accepted by the MySQL server "
   "for DATETIME and TIMESTAMP types, for example: 2004-12-25 11:25:56 "
   "(you should probably use quotes for your shell to set it properly).",
   &stop_datetime_str, &stop_datetime_str,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"stop-never", OPT_STOP_NEVER, "Wait for more data from the server "
   "instead of stopping at the end of the last log. Implicitly sets "
   "--to-last-log but instead of stopping at the end of the last log "
   "it continues to wait till the server disconnects.",
   &stop_never, &stop_never, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"stop-never-slave-server-id", OPT_WAIT_SERVER_ID,
   "The slave server ID used for stop-never",
   &stop_never_server_id, &stop_never_server_id, 0,
   GET_UINT, REQUIRED_ARG, 65535, 1, 65535, 0, 0, 0},
  {"stop-position", OPT_STOP_POSITION,
   "Stop reading the binlog at position N. Applies to the last binlog "
   "passed on the command line.",
   &stop_position, &stop_position, 0, GET_ULL,
   REQUIRED_ARG, (longlong)(~(my_off_t)0), BIN_LOG_HEADER_SIZE,
   (ulonglong)(~(my_off_t)0), 0, 0, 0},
  {"to-last-log", 't', "Requires -R. Will not stop at the end of the "
   "requested binlog but rather continue printing until the end of the last "
   "binlog of the MySQL server. If you send the output to the same MySQL "
   "server, that may lead to an endless loop.",
   &to_last_remote_log, &to_last_remote_log, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "Connect to the remote server as username.",
   &user, &user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"verbose", 'v', "Reconstruct pseudo-SQL statements out of row events. "
                   "-v -v adds comments on column data types.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"open_files_limit", OPT_OPEN_FILES_LIMIT,
   "Used to reserve file descriptors for use by this program.",
   &open_files_limit, &open_files_limit, 0, GET_ULONG,
   REQUIRED_ARG, MY_NFILE, 8, OS_FILE_LIMIT, 0, 1, 0},
  {"verify-binlog-checksum", 'c', "Verify checksum binlog events.",
   (uchar**) &opt_verify_binlog_checksum, (uchar**) &opt_verify_binlog_checksum,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"binlog-row-event-max-size", OPT_BINLOG_ROWS_EVENT_MAX_SIZE,
   "The maximum size of a row-based binary log event in bytes. Rows will be "
   "grouped into events smaller than this size if possible. "
   "This value must be a multiple of 256.",
   &opt_binlog_rows_event_max_size,
   &opt_binlog_rows_event_max_size, 0,
   GET_ULONG, REQUIRED_ARG,
   /* def_value 4GB */ UINT_MAX, /* min_value */ 256,
   /* max_value */ ULONG_MAX, /* sub_size */ 0,
   /* block_size */ 256, /* app_type */ 0},
  {"skip-gtids", OPT_MYSQLBINLOG_SKIP_GTIDS,
   "Do not print Global Transaction Identifier information "
   "(SET GTID_NEXT=... etc).",
   &opt_skip_gtids, &opt_skip_gtids, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"include-gtids", OPT_MYSQLBINLOG_INCLUDE_GTIDS,
   "Print events whose Global Transaction Identifiers "
   "were provided.",
   &opt_include_gtids_str, &opt_include_gtids_str, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"exclude-gtids", OPT_MYSQLBINLOG_EXCLUDE_GTIDS,
   "Print all events but those whose Global Transaction "
   "Identifiers were provided.",
   &opt_exclude_gtids_str, &opt_exclude_gtids_str, 0,
   GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


/**
  Auxiliary function used by error() and warning().

  Prints the given text (normally "WARNING: " or "ERROR: "), followed
  by the given vprintf-style string, followed by a newline.

  @param format Printf-style format string.
  @param args List of arguments for the format string.
  @param msg Text to print before the string.
*/
static void error_or_warning(const char *format, va_list args, const char *msg)
{
  fprintf(stderr, "%s: ", msg);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

/**
  Prints a message to stderr, prefixed with the text "ERROR: " and
  suffixed with a newline.

  @param format Printf-style format string, followed by printf
  varargs.
*/
static void error(const char *format,...)
{
  va_list args;
  va_start(args, format);
  error_or_warning(format, args, "ERROR");
  va_end(args);
}


/**
  This function is used in log_event.cc to report errors.

  @param format Printf-style format string, followed by printf
  varargs.
*/
static void sql_print_error(const char *format,...)
{
  va_list args;
  va_start(args, format);
  error_or_warning(format, args, "ERROR");
  va_end(args);
}

/**
  Prints a message to stderr, prefixed with the text "WARNING: " and
  suffixed with a newline.

  @param format Printf-style format string, followed by printf
  varargs.
*/
static void warning(const char *format,...)
{
  va_list args;
  va_start(args, format);
  error_or_warning(format, args, "WARNING");
  va_end(args);
}

/**
  Frees memory for global variables in this file.
*/
static void cleanup()
{
  my_free(pass);
  my_free(database);
  my_free(host);
  my_free(user);
  my_free(dirname_for_local_load);
  
  for (uint i= 0; i < buff_ev.elements; i++)
  {
    buff_event_info pop_event_array= *dynamic_element(&buff_ev, i, buff_event_info *);
    delete (pop_event_array.event);
  }
  delete_dynamic(&buff_ev);
  
  delete glob_description_event;
  if (mysql)
    mysql_close(mysql);
}


static void print_version()
{
  printf("%s Ver 3.3 for %s at %s\n", my_progname, SYSTEM_TYPE, MACHINE_TYPE);
}


static void usage()
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("\
Dumps a MySQL binary log in a format usable for viewing or for piping to\n\
the mysql command line client.\n\n");
  printf("Usage: %s [options] log-files\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


static my_time_t convert_str_to_timestamp(const char* str)
{
  MYSQL_TIME_STATUS status;
  MYSQL_TIME l_time;
  long dummy_my_timezone;
  my_bool dummy_in_dst_time_gap;
  /* We require a total specification (date AND time) */
  if (str_to_datetime(str, (uint) strlen(str), &l_time, 0, &status) ||
      l_time.time_type != MYSQL_TIMESTAMP_DATETIME || status.warnings)
  {
    error("Incorrect date and time argument: %s", str);
    exit(1);
  }
  /*
    Note that Feb 30th, Apr 31st cause no error messages and are mapped to
    the next existing day, like in mysqld. Maybe this could be changed when
    mysqld is changed too (with its "strict" mode?).
  */
  return
    my_system_gmt_sec(&l_time, &dummy_my_timezone, &dummy_in_dst_time_gap);
}


extern "C" my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  bool tty_password=0;
  switch (optid) {
#ifndef DBUG_OFF
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
#endif
  case 'd':
    one_database = 1;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";                     // Don't require password
    if (argument)
    {
      my_free(pass);
      char *start=argument;
      pass= my_strdup(argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
        start[1]=0;				/* Cut length of argument */
    }
    else
      tty_password=1;
    break;
  case 'R':
    opt_remote_alias= 1;
    opt_remote_proto= BINLOG_DUMP_NON_GTID;
    break;
  case OPT_REMOTE_PROTO:
    opt_remote_proto= (enum_remote_proto)
      (find_type_or_exit(argument, &remote_proto_typelib, opt->name) - 1);
    break;
  case OPT_MYSQL_PROTOCOL:
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
    break;
  case OPT_START_DATETIME:
    start_datetime= convert_str_to_timestamp(start_datetime_str);
    break;
  case OPT_STOP_DATETIME:
    stop_datetime= convert_str_to_timestamp(stop_datetime_str);
    break;
  case OPT_BASE64_OUTPUT_MODE:
    opt_base64_output_mode= (enum_base64_output_mode)
      (find_type_or_exit(argument, &base64_output_mode_typelib, opt->name)-1);
    break;
  case 'v':
    if (argument == disabled_my_option)
      verbose= 0;
    else
      verbose++;
    break;
  case 'V':
    print_version();
    exit(0);
  case OPT_STOP_NEVER:
    /* wait-for-data implicitly sets to-last-log */
    to_last_remote_log= 1;
    break;
  case '?':
    usage();
    exit(0);
  }
  if (tty_password)
    pass= get_tty_password(NullS);

  return 0;
}


static int parse_args(int *argc, char*** argv)
{
  int ho_error;

  result_file = stdout;
  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;
  return 0;
}


/**
  Create and initialize the global mysql object, and connect to the
  server.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
static Exit_status safe_connect()
{
  mysql= mysql_init(NULL);

  if (!mysql)
  {
    error("Failed on mysql_init.");
    return ERROR_STOP;
  }

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);

  if (opt_protocol)
    mysql_options(mysql, MYSQL_OPT_PROTOCOL, (char*) &opt_protocol);
  if (opt_bind_addr)
    mysql_options(mysql, MYSQL_OPT_BIND, opt_bind_addr);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(mysql, MYSQL_SHARED_MEMORY_BASE_NAME,
                  shared_memory_base_name);
#endif
  mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "program_name", "mysqlbinlog");
  if (!mysql_real_connect(mysql, host, user, pass, 0, port, sock, 0))
  {
    error("Failed on connect: %s", mysql_error(mysql));
    return ERROR_STOP;
  }
  mysql->reconnect= 1;
  return OK_CONTINUE;
}


/**
  High-level function for dumping a named binlog.

  This function calls dump_remote_log_entries() or
  dump_local_log_entries() to do the job.

  @param[in] logname Name of input binlog.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
  @retval OK_STOP No error, but the end of the specified range of
  events to process has been reached and the program should terminate.
*/
static Exit_status dump_log_entries(const char* logname)
{
  DBUG_ENTER("dump_log_entries");

  Exit_status rc= OK_CONTINUE;
  PRINT_EVENT_INFO print_event_info;
  if (!print_event_info.init_ok())
    DBUG_RETURN(ERROR_STOP);
  /*
     Set safe delimiter, to dump things
     like CREATE PROCEDURE safely
  */
  if (!raw_mode)
  {
    fprintf(result_file, "DELIMITER /*!*/;\n");
  }
  strmov(print_event_info.delimiter, "/*!*/;");
  
  print_event_info.verbose= short_form ? 0 : verbose;

  switch (opt_remote_proto)
  {
    case BINLOG_LOCAL:
      rc= dump_local_log_entries(&print_event_info, logname);
    break;
    case BINLOG_DUMP_NON_GTID:
    case BINLOG_DUMP_GTID:
      rc= dump_remote_log_entries(&print_event_info, logname);
    break;
    default:
      DBUG_ASSERT(0);
    break;
  }

  if (buff_ev.elements > 0)
    warning("The range of printed events ends with an Intvar_event, "
            "Rand_event or User_var_event with no matching Query_log_event. "
            "This might be because the last statement was not fully written "
            "to the log, or because you are using a --stop-position or "
            "--stop-datetime that refers to an event in the middle of a "
            "statement. The event(s) from the partial statement have not been "
            "written to output. ");

  else if (print_event_info.have_unflushed_events)
    warning("The range of printed events ends with a row event or "
            "a table map event that does not have the STMT_END_F "
            "flag set. This might be because the last statement "
            "was not fully written to the log, or because you are "
            "using a --stop-position or --stop-datetime that refers "
            "to an event in the middle of a statement. The event(s) "
            "from the partial statement have not been written to output.");

  /* Set delimiter back to semicolon */
  if (!raw_mode)
  {
    fprintf(result_file, "DELIMITER ;\n");
    strmov(print_event_info.delimiter, ";");
  }
  DBUG_RETURN(rc);
}


/**
  When reading a remote binlog, this function is used to grab the
  Format_description_log_event in the beginning of the stream.
  
  This is not as smart as check_header() (used for local log); it will
  not work for a binlog which mixes format. TODO: fix this.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
*/
static Exit_status check_master_version()
{
  DBUG_ENTER("check_master_version");
  MYSQL_RES* res = 0;
  MYSQL_ROW row;
  const char* version;

  if (mysql_query(mysql, "SELECT VERSION()") ||
      !(res = mysql_store_result(mysql)))
  {
    error("Could not find server version: "
          "Query failed when checking master version: %s", mysql_error(mysql));
    DBUG_RETURN(ERROR_STOP);
  }
  if (!(row = mysql_fetch_row(res)))
  {
    error("Could not find server version: "
          "Master returned no rows for SELECT VERSION().");
    goto err;
  }

  if (!(version = row[0]))
  {
    error("Could not find server version: "
          "Master reported NULL for the version.");
    goto err;
  }
  /* 
     Make a notice to the server that this client
     is checksum-aware. It does not need the first fake Rotate
     necessary checksummed. 
     That preference is specified below.
  */
  if (mysql_query(mysql, "SET @master_binlog_checksum='NONE'"))
  {
    error("Could not notify master about checksum awareness."
          "Master returned '%s'", mysql_error(mysql));
    goto err;
  }
  delete glob_description_event;
  switch (*version) {
  case '3':
    glob_description_event= new Format_description_log_event(1);
    break;
  case '4':
    glob_description_event= new Format_description_log_event(3);
    break;
  case '5':
    /*
      The server is soon going to send us its Format_description log
      event, unless it is a 5.0 server with 3.23 or 4.0 binlogs.
      So we first assume that this is 4.0 (which is enough to read the
      Format_desc event if one comes).
    */
    glob_description_event= new Format_description_log_event(3);
    break;
  default:
    glob_description_event= NULL;
    error("Could not find server version: "
          "Master reported unrecognized MySQL version '%s'.", version);
    goto err;
  }
  if (!glob_description_event || !glob_description_event->is_valid())
  {
    error("Failed creating Format_description_log_event; out of memory?");
    goto err;
  }

  mysql_free_result(res);
  DBUG_RETURN(OK_CONTINUE);

err:
  mysql_free_result(res);
  DBUG_RETURN(ERROR_STOP);
}


/**
  Requests binlog dump from a remote server and prints the events it
  receives.

  @param[in,out] print_event_info Parameters and context state
  determining how to print.
  @param[in] logname Name of input binlog.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
  @retval OK_STOP No error, but the end of the specified range of
  events to process has been reached and the program should terminate.
*/
static Exit_status dump_remote_log_entries(PRINT_EVENT_INFO *print_event_info,
                                           const char* logname)
{
  uchar *command_buffer= NULL;
  size_t command_size= 0;
  ulong len= 0;
  uint logname_len= 0;
  uint server_id= 0;
  NET* net= NULL;
  my_off_t old_off= start_position_mot;
  char fname[FN_REFLEN + 1];
  char log_file_name[FN_REFLEN + 1];
  Exit_status retval= OK_CONTINUE;
  enum enum_server_command command= COM_END;

  DBUG_ENTER("dump_remote_log_entries");

  fname[0]= log_file_name[0]= 0;

  /*
    Even if we already read one binlog (case of >=2 binlogs on command line),
    we cannot re-use the same connection as before, because it is now dead
    (COM_BINLOG_DUMP kills the thread when it finishes).
  */
  if ((retval= safe_connect()) != OK_CONTINUE)
    DBUG_RETURN(retval);
  net= &mysql->net;

  if ((retval= check_master_version()) != OK_CONTINUE)
    DBUG_RETURN(retval);

  /*
    Fake a server ID to log continously. This will show as a
    slave on the mysql server.
  */
  server_id= ((to_last_remote_log && stop_never) ? stop_never_server_id : 0);
  size_t tlen = strlen(logname);
  if (tlen > UINT_MAX) 
  {
    error("Log name too long.");
    DBUG_RETURN(ERROR_STOP);
  }
  const uint BINLOG_NAME_INFO_SIZE= logname_len= tlen;
  
  if (opt_remote_proto == BINLOG_DUMP_NON_GTID)
  {
    command= COM_BINLOG_DUMP;
    size_t allocation_size= ::BINLOG_POS_OLD_INFO_SIZE +
      BINLOG_NAME_INFO_SIZE + ::BINLOG_FLAGS_INFO_SIZE +
      ::BINLOG_SERVER_ID_INFO_SIZE + 1;
    if (!(command_buffer= (uchar *) my_malloc(allocation_size, MYF(MY_WME))))
    {
      error("Got fatal error allocating memory.");
      DBUG_RETURN(ERROR_STOP);
    }
    uchar* ptr_buffer= command_buffer;

    /*
      COM_BINLOG_DUMP accepts only 4 bytes for the position, so
      we are forced to cast to uint32.
    */
    int4store(ptr_buffer, (uint32) start_position);
    ptr_buffer+= ::BINLOG_POS_OLD_INFO_SIZE;
    int2store(ptr_buffer, binlog_flags);
    ptr_buffer+= ::BINLOG_FLAGS_INFO_SIZE;
    int4store(ptr_buffer, server_id);
    ptr_buffer+= ::BINLOG_SERVER_ID_INFO_SIZE;
    memcpy(ptr_buffer, logname, BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= BINLOG_NAME_INFO_SIZE;

    command_size= ptr_buffer - command_buffer;
    DBUG_ASSERT(command_size == (allocation_size - 1));
  }
  else
  {
    command= COM_BINLOG_DUMP_GTID;

    global_sid_lock->rdlock();

    // allocate buffer
    size_t encoded_data_size= gtid_set_excluded->get_encoded_length();
    size_t allocation_size=
      ::BINLOG_FLAGS_INFO_SIZE + ::BINLOG_SERVER_ID_INFO_SIZE +
      ::BINLOG_NAME_SIZE_INFO_SIZE + BINLOG_NAME_INFO_SIZE +
      ::BINLOG_POS_INFO_SIZE + ::BINLOG_DATA_SIZE_INFO_SIZE +
      encoded_data_size + 1;
    if (!(command_buffer= (uchar *) my_malloc(allocation_size, MYF(MY_WME))))
    {
      error("Got fatal error allocating memory.");
      global_sid_lock->unlock();
      DBUG_RETURN(ERROR_STOP);
    }
    uchar* ptr_buffer= command_buffer;

    int2store(ptr_buffer, binlog_flags);
    ptr_buffer+= ::BINLOG_FLAGS_INFO_SIZE;
    int4store(ptr_buffer, server_id);
    ptr_buffer+= ::BINLOG_SERVER_ID_INFO_SIZE;
    int4store(ptr_buffer, BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= ::BINLOG_NAME_SIZE_INFO_SIZE;
    memcpy(ptr_buffer, logname, BINLOG_NAME_INFO_SIZE);
    ptr_buffer+= BINLOG_NAME_INFO_SIZE;
    int8store(ptr_buffer, start_position);
    ptr_buffer+= ::BINLOG_POS_INFO_SIZE;
    int4store(ptr_buffer, encoded_data_size);
    ptr_buffer+= ::BINLOG_DATA_SIZE_INFO_SIZE;
    gtid_set_excluded->encode(ptr_buffer);
    ptr_buffer+= encoded_data_size;

    global_sid_lock->unlock();

    command_size= ptr_buffer - command_buffer;
    DBUG_ASSERT(command_size == (allocation_size - 1));
  }

  if (simple_command(mysql, command, command_buffer, command_size, 1))
  {
    error("Got fatal error sending the log dump command.");
    my_free(command_buffer);
    DBUG_RETURN(ERROR_STOP);
  }
  my_free(command_buffer);

  for (;;)
  {
    const char *error_msg= NULL;
    Log_event *ev= NULL;
    Log_event_type type= UNKNOWN_EVENT;

    len= cli_safe_read(mysql);
    if (len == packet_error)
    {
      error("Got error reading packet from server: %s", mysql_error(mysql));
      DBUG_RETURN(ERROR_STOP);
    }
    if (len < 8 && net->read_pos[0] == 254)
      break; // end of data
    DBUG_PRINT("info",( "len: %lu  net->read_pos[5]: %d\n",
			len, net->read_pos[5]));
    /*
      In raw mode We only need the full event details if it is a 
      ROTATE_EVENT or FORMAT_DESCRIPTION_EVENT
    */

    type= (Log_event_type) net->read_pos[1 + EVENT_TYPE_OFFSET];
    if (!raw_mode || (type == ROTATE_EVENT) || (type == FORMAT_DESCRIPTION_EVENT))
    {
      if (!(ev= Log_event::read_log_event((const char*) net->read_pos + 1 ,
                                          len - 1, &error_msg,
                                          glob_description_event,
                                          opt_verify_binlog_checksum)))
      {
        error("Could not construct log event object: %s", error_msg);
        DBUG_RETURN(ERROR_STOP);
      }
      /*
        If reading from a remote host, ensure the temp_buf for the
        Log_event class is pointing to the incoming stream.
      */
      ev->register_temp_buf((char *) net->read_pos + 1);
    }
    if (raw_mode || (type != LOAD_EVENT))
    {
      /*
        If this is a Rotate event, maybe it's the end of the requested binlog;
        in this case we are done (stop transfer).
        This is suitable for binlogs, not relay logs (but for now we don't read
        relay logs remotely because the server is not able to do that). If one
        day we read relay logs remotely, then we will have a problem with the
        detection below: relay logs contain Rotate events which are about the
        binlogs, so which would trigger the end-detection below.
      */
      if (type == ROTATE_EVENT)
      {
        Rotate_log_event *rev= (Rotate_log_event *)ev;
        /*
          If this is a fake Rotate event, and not about our log, we can stop
          transfer. If this a real Rotate event (so it's not about our log,
          it's in our log describing the next log), we print it (because it's
          part of our log) and then we will stop when we receive the fake one
          soon.
        */
        if (raw_mode)
        {
          if (output_file != 0)
          {
            my_snprintf(log_file_name, sizeof(log_file_name), "%s%s",
                        output_file, rev->new_log_ident);
          }
          else
          {
            strmov(log_file_name, rev->new_log_ident);
          }
        }

        if (rev->when.tv_sec == 0)
        {
          if (!to_last_remote_log)
          {
            if ((rev->ident_len != logname_len) ||
                memcmp(rev->new_log_ident, logname, logname_len))
            {
              DBUG_RETURN(OK_CONTINUE);
            }
            /*
              Otherwise, this is a fake Rotate for our log, at the very
              beginning for sure. Skip it, because it was not in the original
              log. If we are running with to_last_remote_log, we print it,
              because it serves as a useful marker between binlogs then.
            */
            continue;
          }
          len= 1; // fake Rotate, so don't increment old_off
        }
      }
      else if (type == FORMAT_DESCRIPTION_EVENT)
      {
        /*
          This could be an fake Format_description_log_event that server
          (5.0+) automatically sends to a slave on connect, before sending
          a first event at the requested position.  If this is the case,
          don't increment old_off. Real Format_description_log_event always
          starts from BIN_LOG_HEADER_SIZE position.
        */
        // fake event when not in raw mode, don't increment old_off
        if ((old_off != BIN_LOG_HEADER_SIZE) && (!raw_mode))
          len= 1;
        if (raw_mode)
        {
          if (result_file && (result_file != stdout))
            my_fclose(result_file, MYF(0));
          if (!(result_file = my_fopen(log_file_name, O_WRONLY | O_BINARY,
                                       MYF(MY_WME))))
          {
            error("Could not create log file '%s'", log_file_name);
            DBUG_RETURN(ERROR_STOP);
          }
          my_fwrite(result_file, (const uchar*) BINLOG_MAGIC,
                    BIN_LOG_HEADER_SIZE, MYF(0));
          /*
            Need to handle these events correctly in raw mode too 
            or this could get messy
          */
          delete glob_description_event;
          glob_description_event= (Format_description_log_event*) ev;
          print_event_info->common_header_len= glob_description_event->common_header_len;
          ev->temp_buf= 0;
          ev= 0;
        }
      }
      
      if (type == LOAD_EVENT)
      {
        DBUG_ASSERT(raw_mode);
        warning("Attempting to load a remote pre-4.0 binary log that contains "
                "LOAD DATA INFILE statements. The file will not be copied from "
                "the remote server. ");
      }

      if (raw_mode)
      {
        my_fwrite(result_file, net->read_pos + 1 , len - 1, MYF(0));
        if (ev)
        {
          ev->temp_buf=0;
          delete ev;
        }
      }
      else
      {
        retval= process_event(print_event_info, ev, old_off, logname);
      }

      if (retval != OK_CONTINUE)
        DBUG_RETURN(retval);
    }
    else
    {
      Load_log_event *le= (Load_log_event*)ev;
      const char *old_fname= le->fname;
      uint old_len= le->fname_len;
      File file;

      if ((file= load_processor.prepare_new_file_for_old_format(le,fname)) < 0)
        DBUG_RETURN(ERROR_STOP);

      retval= process_event(print_event_info, ev, old_off, logname);
      if (retval != OK_CONTINUE)
      {
        my_close(file,MYF(MY_WME));
        DBUG_RETURN(retval);
      }
      retval= load_processor.load_old_format_file(net,old_fname,old_len,file);
      my_close(file,MYF(MY_WME));
      if (retval != OK_CONTINUE)
        DBUG_RETURN(retval);
    }
    /*
      Let's adjust offset for remote log as for local log to produce
      similar text and to have --stop-position to work identically.
    */
    old_off+= len-1;
  }

  DBUG_RETURN(OK_CONTINUE);
}


/**
  Reads the @c Format_description_log_event from the beginning of a
  local input file.

  The @c Format_description_log_event is only read if it is outside
  the range specified with @c --start-position; otherwise, it will be
  seen later.  If this is an old binlog, a fake @c
  Format_description_event is created.  This also prints a @c
  Format_description_log_event to the output, unless we reach the
  --start-position range.  In this case, it is assumed that a @c
  Format_description_log_event will be found when reading events the
  usual way.

  @param file The file to which a @c Format_description_log_event will
  be printed.

  @param[in,out] print_event_info Parameters and context state
  determining how to print.

  @param[in] logname Name of input binlog.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
  @retval OK_STOP No error, but the end of the specified range of
  events to process has been reached and the program should terminate.
*/
static Exit_status check_header(IO_CACHE* file,
                                PRINT_EVENT_INFO *print_event_info,
                                const char* logname)
{
  DBUG_ENTER("check_header");
  uchar header[BIN_LOG_HEADER_SIZE];
  uchar buf[PROBE_HEADER_LEN];
  my_off_t tmp_pos, pos;
  MY_STAT my_file_stat;

  delete glob_description_event;
  if (!(glob_description_event= new Format_description_log_event(3)))
  {
    error("Failed creating Format_description_log_event; out of memory?");
    DBUG_RETURN(ERROR_STOP);
  }

  pos= my_b_tell(file);

  /* fstat the file to check if the file is a regular file. */
  if (my_fstat(file->file, &my_file_stat, MYF(0)) == -1)
  {
    error("Unable to stat the file.");
    DBUG_RETURN(ERROR_STOP);
  }
  if ((my_file_stat.st_mode & S_IFMT) == S_IFREG)
    my_b_seek(file, (my_off_t)0);

  if (my_b_read(file, header, sizeof(header)))
  {
    error("Failed reading header; probably an empty file.");
    DBUG_RETURN(ERROR_STOP);
  }
  if (memcmp(header, BINLOG_MAGIC, sizeof(header)))
  {
    error("File is not a binary log file.");
    DBUG_RETURN(ERROR_STOP);
  }

  /*
    Imagine we are running with --start-position=1000. We still need
    to know the binlog format's. So we still need to find, if there is
    one, the Format_desc event, or to know if this is a 3.23
    binlog. So we need to first read the first events of the log,
    those around offset 4.  Even if we are reading a 3.23 binlog from
    the start (no --start-position): we need to know the header length
    (which is 13 in 3.23, 19 in 4.x) to be able to successfully print
    the first event (Start_log_event_v3). So even in this case, we
    need to "probe" the first bytes of the log *before* we do a real
    read_log_event(). Because read_log_event() needs to know the
    header's length to work fine.
  */
  for(;;)
  {
    tmp_pos= my_b_tell(file); /* should be 4 the first time */
    if (my_b_read(file, buf, sizeof(buf)))
    {
      if (file->error)
      {
        error("Could not read entry at offset %llu: "
              "Error in log format or read error.", (ulonglong)tmp_pos);
        DBUG_RETURN(ERROR_STOP);
      }
      /*
        Otherwise this is just EOF : this log currently contains 0-2
        events.  Maybe it's going to be filled in the next
        milliseconds; then we are going to have a problem if this a
        3.23 log (imagine we are locally reading a 3.23 binlog which
        is being written presently): we won't know it in
        read_log_event() and will fail().  Similar problems could
        happen with hot relay logs if --start-position is used (but a
        --start-position which is posterior to the current size of the log).
        These are rare problems anyway (reading a hot log + when we
        read the first events there are not all there yet + when we
        read a bit later there are more events + using a strange
        --start-position).
      */
      break;
    }
    else
    {
      DBUG_PRINT("info",("buf[EVENT_TYPE_OFFSET=%d]=%d",
                         EVENT_TYPE_OFFSET, buf[EVENT_TYPE_OFFSET]));
      /* always test for a Start_v3, even if no --start-position */
      if (buf[EVENT_TYPE_OFFSET] == START_EVENT_V3)
      {
        /* This is 3.23 or 4.x */
        if (uint4korr(buf + EVENT_LEN_OFFSET) < 
            (LOG_EVENT_MINIMAL_HEADER_LEN + START_V3_HEADER_LEN))
        {
          /* This is 3.23 (format 1) */
          delete glob_description_event;
          if (!(glob_description_event= new Format_description_log_event(1)))
          {
            error("Failed creating Format_description_log_event; "
                  "out of memory?");
            DBUG_RETURN(ERROR_STOP);
          }
        }
        break;
      }
      else if (tmp_pos >= start_position)
        break;
      else if (buf[EVENT_TYPE_OFFSET] == FORMAT_DESCRIPTION_EVENT)
      {
        /* This is 5.0 */
        Format_description_log_event *new_description_event;
        my_b_seek(file, tmp_pos); /* seek back to event's start */
        if (!(new_description_event= (Format_description_log_event*) 
              Log_event::read_log_event(file, glob_description_event,
                                        opt_verify_binlog_checksum)))
          /* EOF can't be hit here normally, so it's a real error */
        {
          error("Could not read a Format_description_log_event event at "
                "offset %llu; this could be a log format error or read error.",
                (ulonglong)tmp_pos);
          DBUG_RETURN(ERROR_STOP);
        }
        if (opt_base64_output_mode == BASE64_OUTPUT_AUTO)
        {
          /*
            process_event will delete *description_event and set it to
            the new one, so we should not do it ourselves in this
            case.
          */
          Exit_status retval= process_event(print_event_info,
                                            new_description_event, tmp_pos,
                                            logname);
          if (retval != OK_CONTINUE)
            DBUG_RETURN(retval);
        }
        else
        {
          delete glob_description_event;
          glob_description_event= new_description_event;
        }
        DBUG_PRINT("info",("Setting description_event"));
      }
      else if (buf[EVENT_TYPE_OFFSET] == ROTATE_EVENT)
      {
        Log_event *ev;
        my_b_seek(file, tmp_pos); /* seek back to event's start */
        if (!(ev= Log_event::read_log_event(file, glob_description_event,
                                            opt_verify_binlog_checksum)))
        {
          /* EOF can't be hit here normally, so it's a real error */
          error("Could not read a Rotate_log_event event at offset %llu;"
                " this could be a log format error or read error.",
                (ulonglong)tmp_pos);
          DBUG_RETURN(ERROR_STOP);
        }
        delete ev;
      }
      else
        break;
    }
  }
  my_b_seek(file, pos);
  DBUG_RETURN(OK_CONTINUE);
}


/**
  Reads a local binlog and prints the events it sees.

  @param[in] logname Name of input binlog.

  @param[in,out] print_event_info Parameters and context state
  determining how to print.

  @retval ERROR_STOP An error occurred - the program should terminate.
  @retval OK_CONTINUE No error, the program should continue.
  @retval OK_STOP No error, but the end of the specified range of
  events to process has been reached and the program should terminate.
*/
static Exit_status dump_local_log_entries(PRINT_EVENT_INFO *print_event_info,
                                          const char* logname)
{
  File fd = -1;
  IO_CACHE cache,*file= &cache;
  uchar tmp_buff[BIN_LOG_HEADER_SIZE];
  Exit_status retval= OK_CONTINUE;

  if (logname && strcmp(logname, "-") != 0)
  {
    /* read from normal file */
    if ((fd = my_open(logname, O_RDONLY | O_BINARY, MYF(MY_WME))) < 0)
      return ERROR_STOP;
    if (init_io_cache(file, fd, 0, READ_CACHE, start_position_mot, 0,
		      MYF(MY_WME | MY_NABP)))
    {
      my_close(fd, MYF(MY_WME));
      return ERROR_STOP;
    }
    if ((retval= check_header(file, print_event_info, logname)) != OK_CONTINUE)
      goto end;
  }
  else
  {
    /* read from stdin */
    /*
      Windows opens stdin in text mode by default. Certain characters
      such as CTRL-Z are interpeted as events and the read() method
      will stop. CTRL-Z is the EOF marker in Windows. to get past this
      you have to open stdin in binary mode. Setmode() is used to set
      stdin in binary mode. Errors on setting this mode result in 
      halting the function and printing an error message to stderr.
    */
#if defined (__WIN__) || (_WIN64)
    if (_setmode(fileno(stdin), O_BINARY) == -1)
    {
      error("Could not set binary mode on stdin.");
      return ERROR_STOP;
    }
#endif 
    if (init_io_cache(file, my_fileno(stdin), 0, READ_CACHE, (my_off_t) 0,
		      0, MYF(MY_WME | MY_NABP | MY_DONT_CHECK_FILESIZE)))
    {
      error("Failed to init IO cache.");
      return ERROR_STOP;
    }
    if ((retval= check_header(file, print_event_info, logname)) != OK_CONTINUE)
      goto end;
    if (start_position)
    {
      /* skip 'start_position' characters from stdin */
      uchar buff[IO_SIZE];
      my_off_t length,tmp;
      for (length= start_position_mot ; length > 0 ; length-=tmp)
      {
	tmp= min<size_t>(length, sizeof(buff));
	if (my_b_read(file, buff, (uint) tmp))
        {
          error("Failed reading from file.");
          goto err;
        }
      }
    }
  }

  if (!glob_description_event || !glob_description_event->is_valid())
  {
    error("Invalid Format_description log event; could be out of memory.");
    goto err;
  }

  if (!start_position && my_b_read(file, tmp_buff, BIN_LOG_HEADER_SIZE))
  {
    error("Failed reading from file.");
    goto err;
  }
  for (;;)
  {
    char llbuff[21];
    my_off_t old_off = my_b_tell(file);

    Log_event* ev = Log_event::read_log_event(file, glob_description_event,
                                              opt_verify_binlog_checksum);
    if (!ev)
    {
      /*
        if binlog wasn't closed properly ("in use" flag is set) don't complain
        about a corruption, but treat it as EOF and move to the next binlog.
      */
      if (glob_description_event->flags & LOG_EVENT_BINLOG_IN_USE_F)
        file->error= 0;
      else if (file->error)
      {
        error("Could not read entry at offset %s: "
              "Error in log format or read error.",
              llstr(old_off,llbuff));
        goto err;
      }
      // file->error == 0 means EOF, that's OK, we break in this case
      goto end;
    }
    if ((retval= process_event(print_event_info, ev, old_off, logname)) !=
        OK_CONTINUE)
      goto end;
  }

  /* NOTREACHED */

err:
  retval= ERROR_STOP;

end:
  if (fd >= 0)
    my_close(fd, MYF(MY_WME));
  /*
    Since the end_io_cache() writes to the
    file errors may happen.
   */
  if (end_io_cache(file))
    retval= ERROR_STOP;

  return retval;
}

/* Post processing of arguments to check for conflicts and other setups */
static int args_post_process(void)
{
  DBUG_ENTER("args_post_process");

  if (opt_remote_alias && opt_remote_proto != BINLOG_DUMP_NON_GTID)
  {
    error("The option read-from-remote-server cannot be used when "
          "read-from-remote-master is defined and is not equal to "
          "BINLOG-DUMP-NON-GTIDS");
    DBUG_RETURN(ERROR_STOP);
  }

  if (raw_mode)
  {
    if (one_database)
      warning("The --database option is ignored with --raw mode");

    if (opt_remote_proto == BINLOG_LOCAL)
    {
      error("You need to set --read-from-remote-master={BINLOG_DUMP_NON_GTID, "
            "BINLOG_DUMP_GTID} for --raw mode");
      DBUG_RETURN(ERROR_STOP);
    }

    if (opt_remote_proto == BINLOG_DUMP_NON_GTID &&
        (opt_exclude_gtids_str != NULL || opt_include_gtids_str != NULL))
    {
      error("You cannot set --exclude-gtids or --include-gtids for --raw-mode "
            "when --read-from-remote-master=BINLOG_DUMP_NON_GTID");
      DBUG_RETURN(ERROR_STOP);
    }

    if (opt_remote_proto == BINLOG_DUMP_GTID && opt_include_gtids_str != NULL)
    {
      error("You cannot set --include-gtids for --raw-mode "
            "when --read-from-remote-master=BINLOG_DUMP_GTID for");
      DBUG_RETURN(ERROR_STOP);
    }

    if (stop_position != (ulonglong)(~(my_off_t)0))
      warning("The --stop-position option is ignored in raw mode");

    if (stop_datetime != MY_TIME_T_MAX)
      warning("The --stop-datetime option is ignored in raw mode");
  }
  else if (output_file)
  {
    if (!(result_file = my_fopen(output_file, O_WRONLY | O_BINARY, MYF(MY_WME))))
    {
      error("Could not create log file '%s'", output_file);
      DBUG_RETURN(ERROR_STOP);
    }
  }

  global_sid_lock->rdlock();

  if (opt_include_gtids_str != NULL)
  {
    if (gtid_set_included->add_gtid_text(opt_include_gtids_str) !=
        RETURN_STATUS_OK)
    {
      error("Could not configure --include-gtids '%s'", opt_include_gtids_str);
      global_sid_lock->unlock();
      DBUG_RETURN(ERROR_STOP);
    }
  }

  if (opt_exclude_gtids_str != NULL)
  {
    if (gtid_set_excluded->add_gtid_text(opt_exclude_gtids_str) !=
        RETURN_STATUS_OK)
    {
      error("Could not configure --exclude-gtids '%s'", opt_exclude_gtids_str);
      global_sid_lock->unlock();
      DBUG_RETURN(ERROR_STOP);
    }
  }

  global_sid_lock->unlock();

  DBUG_RETURN(OK_CONTINUE);
}

/**
   GTID cleanup destroys objects and reset their pointer.
   Function is reentrant.
*/
inline void gtid_client_cleanup()
{
  delete global_sid_lock;
  delete global_sid_map;
  delete gtid_set_excluded;
  delete gtid_set_included;
  global_sid_lock= NULL;
  global_sid_map= NULL;
  gtid_set_excluded= NULL;
  gtid_set_included= NULL;
}

/**
   GTID initialization.

   @return true if allocation does not succeed
           false if OK
*/
inline bool gtid_client_init()
{
  bool res=
    (!(global_sid_lock= new Checkable_rwlock) ||
     !(global_sid_map= new Sid_map(global_sid_lock)) ||
     !(gtid_set_excluded= new Gtid_set(global_sid_map)) ||
     !(gtid_set_included= new Gtid_set(global_sid_map)));
  if (res)
  {
    gtid_client_cleanup();
  }
  return res;
}

int main(int argc, char** argv)
{
  char **defaults_argv;
  Exit_status retval= OK_CONTINUE;
  ulonglong save_stop_position;
  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);

  my_init_time(); // for time functions
   /*
    A pointer of type Log_event can point to
     INTVAR
     USER_VAR
     RANDOM
    events,  when we allocate a element of sizeof(Log_event*) 
    for the DYNAMIC_ARRAY.
  */

  if((my_init_dynamic_array(&buff_ev, sizeof(buff_event_info), 
                            INTVAR_DYNAMIC_INIT, INTVAR_DYNAMIC_INCR)))
    exit(1);

  my_getopt_use_args_separator= TRUE;
  if (load_defaults("my", load_default_groups, &argc, &argv))
    exit(1);
  my_getopt_use_args_separator= FALSE;
  defaults_argv= argv;

  parse_args(&argc, &argv);

  if (!argc)
  {
    usage();
    free_defaults(defaults_argv);
    my_end(my_end_arg);
    exit(1);
  }

  if (gtid_client_init())
  {
    error("Could not initialize GTID structuress.");
    exit(1);
  }

  /* Check for argument conflicts and do any post-processing */
  if (args_post_process() == ERROR_STOP)
    exit(1);

  if (opt_base64_output_mode == BASE64_OUTPUT_UNSPEC)
    opt_base64_output_mode= BASE64_OUTPUT_AUTO;

  opt_server_id_mask = (opt_server_id_bits == 32)?
    ~ ulong(0) : (1 << opt_server_id_bits) -1;

  my_set_max_open_files(open_files_limit);

  MY_TMPDIR tmpdir;
  tmpdir.list= 0;
  if (!dirname_for_local_load)
  {
    if (init_tmpdir(&tmpdir, 0))
      exit(1);
    dirname_for_local_load= my_strdup(my_tmpdir(&tmpdir), MY_WME);
  }

  if (load_processor.init())
    exit(1);
  if (dirname_for_local_load)
    load_processor.init_by_dir_name(dirname_for_local_load);
  else
    load_processor.init_by_cur_dir();

  if (!raw_mode)
  {
    fprintf(result_file,
            "/*!40019 SET @@session.max_insert_delayed_threads=0*/;\n");

    if (disable_log_bin)
      fprintf(result_file,
              "/*!32316 SET @OLD_SQL_LOG_BIN=@@SQL_LOG_BIN, SQL_LOG_BIN=0*/;\n");

    /*
      In mysqlbinlog|mysql, don't want mysql to be disconnected after each
      transaction (which would be the case with GLOBAL.COMPLETION_TYPE==2).
    */
    fprintf(result_file,
            "/*!50003 SET @OLD_COMPLETION_TYPE=@@COMPLETION_TYPE,"
            "COMPLETION_TYPE=0*/;\n");

    if (charset)
      fprintf(result_file,
              "\n/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;"
              "\n/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;"
              "\n/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;"
              "\n/*!40101 SET NAMES %s */;\n", charset);
  }

  for (save_stop_position= stop_position, stop_position= ~(my_off_t)0 ;
       (--argc >= 0) ; )
  {
    if (argc == 0) // last log, --stop-position applies
      stop_position= save_stop_position;
    if ((retval= dump_log_entries(*argv++)) != OK_CONTINUE)
      break;

    // For next log, --start-position does not apply
    start_position= BIN_LOG_HEADER_SIZE;
  }

  if (!raw_mode)
  {
    /*
      Issue a ROLLBACK in case the last printed binlog was crashed and had half
      of transaction.
    */
    fprintf(result_file,
          "# End of log file\nROLLBACK /* added by mysqlbinlog */;\n"
          "/*!50003 SET COMPLETION_TYPE=@OLD_COMPLETION_TYPE*/;\n");
    if (disable_log_bin)
      fprintf(result_file, "/*!32316 SET SQL_LOG_BIN=@OLD_SQL_LOG_BIN*/;\n");

    if (charset)
      fprintf(result_file,
              "/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;\n"
              "/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;\n"
              "/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;\n");
  }

  if (tmpdir.list)
    free_tmpdir(&tmpdir);
  if (result_file && (result_file != stdout))
    my_fclose(result_file, MYF(0));
  cleanup();

  if (defaults_argv)
    free_defaults(defaults_argv);
  my_free_open_file_info();
  load_processor.destroy();
  /* We cannot free DBUG, it is used in global destructors after exit(). */
  my_end(my_end_arg | MY_DONT_FREE_DBUG);
  gtid_client_cleanup();

  exit(retval == ERROR_STOP ? 1 : 0);
  /* Keep compilers happy. */
  DBUG_RETURN(retval == ERROR_STOP ? 1 : 0);
}

/*
  We must include this here as it's compiled with different options for
  the server
*/

#include "decimal.c"
#include "my_decimal.cc"
#include "log_event.cc"
#include "log_event_old.cc"
#include "rpl_utility.cc"
#include "rpl_gtid_sid_map.cc"
#include "rpl_gtid_misc.cc"
#include "uuid.cc"
#include "rpl_gtid_set.cc"
#include "rpl_gtid_specification.cc"
