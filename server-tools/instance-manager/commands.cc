/* Copyright (C) 2004 MySQL AB

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

#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "commands.h"

#include <my_global.h>
#include <m_ctype.h>
#include <mysql.h>
#include <my_dir.h>

#include "buffer.h"
#include "guardian.h"
#include "instance_map.h"
#include "log.h"
#include "manager.h"
#include "messages.h"
#include "mysqld_error.h"
#include "mysql_manager_error.h"
#include "options.h"
#include "priv.h"
#include "protocol.h"


/*
  modify_defaults_to_im_error -- a map of error codes of
  mysys::modify_defaults_file() into Instance Manager error codes.
*/

static const int modify_defaults_to_im_error[]= { 0, ER_OUT_OF_RESOURCES,
                                                  ER_ACCESS_OPTION_FILE };


/*
  Add a string to a buffer.

  SYNOPSYS
    put_to_buff()
    buff              buffer to add the string
    str               string to add
    position          offset in the buff to add a string

  DESCRIPTION

  Function to add a string to the buffer. It is different from
  store_to_protocol_packet, which is used in the protocol.cc.
  The last one also stores the length of the string in a special way.
  This is required for MySQL client/server protocol support only.

  RETURN
    0 - ok
    1 - error occured
*/

static inline int put_to_buff(Buffer *buff, const char *str, uint *position)
{
  uint len= strlen(str);
  if (buff->append(*position, str, len))
    return 1;

  *position+= len;
  return 0;
}


static int parse_version_number(const char *version_str, char *version,
                                uint version_size)
{
  const char *start= version_str;
  const char *end;

  // skip garbage
  while (!my_isdigit(default_charset_info, *start))
    start++;

  end= start;
  // skip digits and dots
  while (my_isdigit(default_charset_info, *end) || *end == '.')
    end++;

  if ((uint)(end - start) >= version_size)
    return -1;

  strncpy(version, start, end-start);
  version[end-start]= '\0';

  return 0;
}


/**************************************************************************
 Implementation of Instance_name.
**************************************************************************/

Instance_name::Instance_name(const LEX_STRING *name)
{
  str.str= str_buffer;
  str.length= name->length;

  if (str.length > MAX_INSTANCE_NAME_SIZE - 1)
    str.length= MAX_INSTANCE_NAME_SIZE - 1;

  strmake(str.str, name->str, str.length);
}

/**************************************************************************
 Implementation of Show_instances.
**************************************************************************/

/*
  Implementation of SHOW INSTANCES statement.

  Possible error codes:
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Show_instances::execute(st_net *net, ulong connection_id)
{
  int err_code;

  if ((err_code= write_header(net)) ||
      (err_code= write_data(net)))
    return err_code;

  if (send_eof(net) || net_flush(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


int Show_instances::write_header(st_net *net)
{
  LIST name, state;
  LEX_STRING name_field, state_field;
  LIST *field_list;

  name_field.str= (char *) "instance_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;

  state_field.str= (char *) "state";
  state_field.length= DEFAULT_FIELD_LENGTH;
  state.data= &state_field;

  field_list= list_add(NULL, &state);
  field_list= list_add(field_list, &name);

  return send_fields(net, field_list) ? ER_OUT_OF_RESOURCES : 0;
}


int Show_instances::write_data(st_net *net)
{
  my_bool err_status= FALSE;

  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  instance_map->guardian->lock();
  instance_map->lock();

  while ((instance= iterator.next()))
  {
    Buffer send_buf;  /* buffer for packets */
    uint pos= 0;

    const char *instance_name= instance->options.instance_name.str;
    const char *state_name= instance_map->get_instance_state_name(instance);

    if (store_to_protocol_packet(&send_buf, instance_name, &pos) ||
        store_to_protocol_packet(&send_buf, state_name, &pos) ||
        my_net_write(net, send_buf.buffer, pos))
    {
      err_status= TRUE;
      break;
    }
  }

  instance_map->unlock();
  instance_map->guardian->unlock();

  return err_status ? ER_OUT_OF_RESOURCES : 0;
}


/**************************************************************************
 Implementation of Flush_instances.
**************************************************************************/

/*
  Implementation of FLUSH INSTANCES statement.

  Possible error codes:
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
    ER_THERE_IS_ACTIVE_INSTACE  If there is an active instance
*/

int Flush_instances::execute(st_net *net, ulong connection_id)
{
  instance_map->guardian->lock();
  instance_map->lock();

  if (instance_map->is_there_active_instance())
  {
    instance_map->unlock();
    instance_map->guardian->unlock();
    return ER_THERE_IS_ACTIVE_INSTACE;
  }

  if (instance_map->flush_instances())
  {
    instance_map->unlock();
    instance_map->guardian->unlock();
    return ER_OUT_OF_RESOURCES;
  }

  instance_map->unlock();
  instance_map->guardian->unlock();

  return net_send_ok(net, connection_id, NULL) ? ER_OUT_OF_RESOURCES : 0;
}


/**************************************************************************
 Implementation of Abstract_instance_cmd.
**************************************************************************/

Abstract_instance_cmd::Abstract_instance_cmd(
  Instance_map *instance_map_arg, const LEX_STRING *instance_name_arg)
  :Command(instance_map_arg),
  instance_name(instance_name_arg)
{
  /*
    MT-NOTE: we can not make a search for Instance object here,
    because it can dissappear after releasing the lock.
  */
}


int Abstract_instance_cmd::execute(st_net *net, ulong connection_id)
{
  int err_code;

  instance_map->lock();

  {
    Instance *instance= instance_map->find(get_instance_name());

    if (!instance)
    {
      instance_map->unlock();
      return ER_BAD_INSTANCE_NAME;
    }

    err_code= execute_impl(net, instance);
  }

  instance_map->unlock();

  if (!err_code)
    err_code= send_ok_response(net, connection_id);

  return err_code;
}


/**************************************************************************
 Implementation of Show_instance_status.
**************************************************************************/

Show_instance_status::Show_instance_status(Instance_map *instance_map_arg,
                                           const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of SHOW INSTANCE STATUS statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Show_instance_status::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if ((err_code= write_header(net)) ||
      (err_code= write_data(net, instance)))
    return err_code;

  return 0;
}


int Show_instance_status::send_ok_response(st_net *net, ulong connection_id)
{
  if (send_eof(net) || net_flush(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


int Show_instance_status::write_header(st_net *net)
{
  LIST name, state, version, version_number, mysqld_compatible;
  LIST *field_list;
  LEX_STRING name_field, state_field, version_field,
                   version_number_field, mysqld_compatible_field;

  /* Create list of the fileds to be passed to send_fields(). */

  name_field.str= (char *) "instance_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;

  state_field.str= (char *) "state";
  state_field.length= DEFAULT_FIELD_LENGTH;
  state.data= &state_field;

  version_field.str= (char *) "version";
  version_field.length= MAX_VERSION_LENGTH;
  version.data= &version_field;

  version_number_field.str= (char *) "version_number";
  version_number_field.length= MAX_VERSION_LENGTH;
  version_number.data= &version_number_field;

  mysqld_compatible_field.str= (char *) "mysqld_compatible";
  mysqld_compatible_field.length= DEFAULT_FIELD_LENGTH;
  mysqld_compatible.data= &mysqld_compatible_field;

  field_list= list_add(NULL, &mysqld_compatible);
  field_list= list_add(field_list, &version);
  field_list= list_add(field_list, &version_number);
  field_list= list_add(field_list, &state);
  field_list= list_add(field_list, &name);

  return send_fields(net, field_list) ? ER_OUT_OF_RESOURCES : 0;
}


int Show_instance_status::write_data(st_net *net, Instance *instance)
{
  Buffer send_buf;  /* buffer for packets */
  char version_num_buf[MAX_VERSION_LENGTH];
  uint pos= 0;

  const char *state_name;
  const char *version_tag= "unknown";
  const char *version_num= "unknown";
  const char *mysqld_compatible_status;

  instance_map->guardian->lock();
  state_name= instance_map->get_instance_state_name(instance);
  mysqld_compatible_status= instance->is_mysqld_compatible() ? "yes" : "no";
  instance_map->guardian->unlock();

  if (instance->options.mysqld_version)
  {

    if (parse_version_number(instance->options.mysqld_version, version_num_buf,
                             sizeof(version_num_buf)))
      return ER_OUT_OF_RESOURCES;

    version_num= version_num_buf;
    version_tag= instance->options.mysqld_version;
  }

  if (store_to_protocol_packet(&send_buf, get_instance_name()->str, &pos) ||
      store_to_protocol_packet(&send_buf, state_name, &pos) ||
      store_to_protocol_packet(&send_buf, version_num, &pos) ||
      store_to_protocol_packet(&send_buf, version_tag, &pos) ||
      store_to_protocol_packet(&send_buf, mysqld_compatible_status, &pos) ||
      my_net_write(net, send_buf.buffer, (uint) pos))
  {
    return ER_OUT_OF_RESOURCES;
  }

  return 0;
}


/**************************************************************************
 Implementation of Show_instance_options.
**************************************************************************/

Show_instance_options::Show_instance_options(
  Instance_map *instance_map_arg, const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of SHOW INSTANCE OPTIONS statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Show_instance_options::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if ((err_code= write_header(net)) ||
      (err_code= write_data(net, instance)))
    return err_code;

  return 0;
}


int Show_instance_options::send_ok_response(st_net *net, ulong connection_id)
{
  if (send_eof(net) || net_flush(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


int Show_instance_options::write_header(st_net *net)
{
  LIST name, option;
  LIST *field_list;
  LEX_STRING name_field, option_field;

  /* Create list of the fileds to be passed to send_fields(). */

  name_field.str= (char *) "option_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;

  option_field.str= (char *) "value";
  option_field.length= DEFAULT_FIELD_LENGTH;
  option.data= &option_field;

  field_list= list_add(NULL, &option);
  field_list= list_add(field_list, &name);

  return send_fields(net, field_list) ? ER_OUT_OF_RESOURCES : 0;
}


int Show_instance_options::write_data(st_net *net, Instance *instance)
{
  Buffer send_buff;  /* buffer for packets */
  uint pos= 0;

  if (store_to_protocol_packet(&send_buff, "instance_name", &pos) ||
      store_to_protocol_packet(&send_buff, get_instance_name()->str, &pos) ||
      my_net_write(net, send_buff.buffer, pos))
  {
    return ER_OUT_OF_RESOURCES;
  }

  /* Loop through the options. */

  for (int i= 0; i < instance->options.get_num_options(); i++)
  {
    Named_value option= instance->options.get_option(i);
    const char *option_value= option.get_value()[0] ? option.get_value() : "";

    pos= 0;

    if (store_to_protocol_packet(&send_buff, option.get_name(), &pos) ||
        store_to_protocol_packet(&send_buff, option_value, &pos) ||
        my_net_write(net, send_buff.buffer, pos))
    {
      return ER_OUT_OF_RESOURCES;
    }
  }

  return 0;
}


/**************************************************************************
 Implementation of Start_instance.
**************************************************************************/

Start_instance::Start_instance(Instance_map *instance_map_arg,
                               const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of START INSTANCE statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Start_instance::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if ((err_code= instance->start()))
    return err_code;

  if (!(instance->options.nonguarded))
    instance_map->guardian->guard(instance);

  return 0;
}


int Start_instance::send_ok_response(st_net *net, ulong connection_id)
{
  if (net_send_ok(net, connection_id, "Instance started"))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/**************************************************************************
 Implementation of Stop_instance.
**************************************************************************/

Stop_instance::Stop_instance(Instance_map *instance_map_arg,
                             const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of STOP INSTANCE statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Stop_instance::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if (!(instance->options.nonguarded))
    instance_map->guardian->stop_guard(instance);

  if ((err_code= instance->stop()))
    return err_code;

  return 0;
}


int Stop_instance::send_ok_response(st_net *net, ulong connection_id)
{
  if (net_send_ok(net, connection_id, NULL))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/**************************************************************************
 Implementation for Create_instance.
**************************************************************************/

Create_instance::Create_instance(Instance_map *instance_map_arg,
                                 const LEX_STRING *instance_name_arg)
  :Command(instance_map_arg),
  instance_name(instance_name_arg)
{
}


/*
  This operation initializes Create_instance object.

  SYNOPSYS
    text            [IN/OUT] a pointer to the text containing instance options.

  RETURN
    FALSE           On success.
    TRUE            On error.
*/

bool Create_instance::init(const char **text)
{
  return options.init() || parse_args(text);
}


/*
  This operation parses CREATE INSTANCE options.

  SYNOPSYS
    text            [IN/OUT] a pointer to the text containing instance options.

  RETURN
    FALSE           On success.
    TRUE            On syntax error.
*/

bool Create_instance::parse_args(const char **text)
{
  uint len;

  /* Check if we have something (and trim leading spaces). */

  get_word(text, &len, NONSPACE);

  if (len == 0)
    return FALSE; /* OK: no option. */

  /* Main parsing loop. */

  while (TRUE)
  {
    LEX_STRING option_name;
    char *option_name_str;
    char *option_value_str= NULL;

    /* Looking for option name. */

    get_word(text, &option_name.length, OPTION_NAME);

    if (option_name.length == 0)
      return TRUE; /* Syntax error: option name expected. */

    option_name.str= (char *) *text;
    *text+= option_name.length;

    /* Looking for equal sign. */

    skip_spaces(text);

    if (**text == '=')
    {
      ++(*text); /* Skip an equal sign. */

      /* Looking for option value. */

      skip_spaces(text);

      if (!**text)
        return TRUE; /* Syntax error: EOS when option value expected. */

      if (**text != '\'' && **text != '"')
      {
        /* Option value is a simple token. */

        LEX_STRING option_value;

        get_word(text, &option_value.length, ALPHANUM);

        if (option_value.length == 0)
          return TRUE; /* internal parser error. */

        option_value.str= (char *) *text;
        *text+= option_value.length;

        if (!(option_value_str= Named_value::alloc_str(&option_value)))
          return TRUE; /* out of memory during parsing. */
      }
      else
      {
        /* Option value is a string. */

        if (parse_option_value(*text, &len, &option_value_str))
          return TRUE; /* Syntax error: invalid string specification. */

        *text+= len;
      }
    }

    if (!option_value_str)
    {
      LEX_STRING empty_str= { C_STRING_WITH_LEN("") };

      if (!(option_value_str= Named_value::alloc_str(&empty_str)))
        return TRUE; /* out of memory during parsing. */
    }

    if (!(option_name_str= Named_value::alloc_str(&option_name)))
    {
      Named_value::free_str(&option_value_str);
      return TRUE; /* out of memory during parsing. */
    }

    {
      Named_value option(option_name_str, option_value_str);

      if (options.add_element(&option))
      {
        option.free();
        return TRUE; /* out of memory during parsing. */
      }
    }

    skip_spaces(text);

    if (!**text)
      return FALSE; /* OK: end of options. */

    if (**text != ',')
      return TRUE; /* Syntax error: comma expected. */

    ++(*text);
  }
}


/*
  Implementation of CREATE INSTANCE statement.

  Possible error codes:
    ER_MALFORMED_INSTANCE_NAME  Instance name is malformed
    ER_CREATE_EXISTING_INSTANCE There is an instance with the given name
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Create_instance::execute(st_net *net, ulong connection_id)
{
  int err_code;

  /* Check that the name is valid and there is no instance with such name. */

  if (!Instance::is_name_valid(get_instance_name()))
    return ER_MALFORMED_INSTANCE_NAME;

  /*
    NOTE: In order to prevent race condition, we should perform all operations
    on under acquired lock.
  */

  instance_map->lock();

  if (instance_map->find(get_instance_name()))
  {
    instance_map->unlock();
    return ER_CREATE_EXISTING_INSTANCE;
  }

  if ((err_code= instance_map->create_instance(get_instance_name(), &options)))
  {
    instance_map->unlock();
    return err_code;
  }

  if ((err_code= create_instance_in_file(get_instance_name(), &options)))
  {
    Instance *instance= instance_map->find(get_instance_name());

    if (instance)
      instance_map->remove_instance(instance); /* instance is deleted here. */

    instance_map->unlock();
    return err_code;
  }

  /* That's all. */

  instance_map->unlock();

  /* Send the result. */

  if (net_send_ok(net, connection_id, NULL))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/**************************************************************************
 Implementation for Drop_instance.
**************************************************************************/

Drop_instance::Drop_instance(Instance_map *instance_map_arg,
                             const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of DROP INSTANCE statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_DROP_ACTIVE_INSTANCE     The specified instance is active
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Drop_instance::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  /* Check that the instance is offline. */

  if (instance_map->guardian->is_active(instance))
    return ER_DROP_ACTIVE_INSTANCE;

  err_code= modify_defaults_file(Options::Main::config_file, NULL, NULL,
                                 get_instance_name()->str, MY_REMOVE_SECTION);
  DBUG_ASSERT(err_code >= 0 && err_code <= 2);

  if (err_code)
  {
    log_error("Can not remove instance '%s' from defaults file (%s). "
              "Original error code: %d.",
              (const char *) get_instance_name()->str,
              (const char *) Options::Main::config_file,
              (int) err_code);
  }

  if (err_code)
    return modify_defaults_to_im_error[err_code];

  /* Remove instance from the instance map hash and Guardian's list. */

  if (!instance->options.nonguarded)
    instance_map->guardian->stop_guard(instance);

  if ((err_code= instance->stop()))
    return err_code;

  instance_map->remove_instance(instance);

  return 0;
}


int Drop_instance::send_ok_response(st_net *net, ulong connection_id)
{
  if (net_send_ok(net, connection_id, "Instance dropped"))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/**************************************************************************
 Implementation for Show_instance_log.
**************************************************************************/

Show_instance_log::Show_instance_log(Instance_map *instance_map_arg,
                                     const LEX_STRING *instance_name_arg,
                                     Log_type log_type_arg,
                                     uint size_arg, uint offset_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg),
  log_type(log_type_arg),
  size(size_arg),
  offset(offset_arg)
{
}


/*
  Implementation of SHOW INSTANCE LOG statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OFFSET_ERROR             We were requested to read negative number of
                                bytes from the log
    ER_NO_SUCH_LOG              The specified type of log is not available for
                                the given instance
    ER_GUESS_LOGFILE            IM wasn't able to figure out the log
                                placement, while it is enabled. Probably user
                                should specify the path to the logfile
                                explicitly.
    ER_OPEN_LOGFILE             Cannot open the logfile
    ER_READ_FILE                Cannot read the logfile
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Show_instance_log::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if ((err_code= check_params(instance)))
    return err_code;

  if ((err_code= write_header(net)) ||
      (err_code= write_data(net, instance)))
    return err_code;

  return 0;
}


int Show_instance_log::send_ok_response(st_net *net, ulong connection_id)
{
  if (send_eof(net) || net_flush(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


int Show_instance_log::check_params(Instance *instance)
{
  const char *logpath= instance->options.logs[log_type];

  /* Cannot read negative number of bytes. */

  if (offset > size)
    return ER_OFFSET_ERROR;

  /* Instance has no such log. */

  if (logpath == NULL)
    return ER_NO_SUCH_LOG;

  if (*logpath == '\0')
    return ER_GUESS_LOGFILE;

  return 0;
}


int Show_instance_log::write_header(st_net *net)
{
  LIST name;
  LIST *field_list;
  LEX_STRING name_field;

  /* Create list of the fields to be passed to send_fields(). */

  name_field.str= (char *) "Log";
  name_field.length= DEFAULT_FIELD_LENGTH;

  name.data= &name_field;

  field_list= list_add(NULL, &name);

  return send_fields(net, field_list) ? ER_OUT_OF_RESOURCES : 0;
}


int Show_instance_log::write_data(st_net *net, Instance *instance)
{
  Buffer send_buff;  /* buffer for packets */
  uint pos= 0;

  const char *logpath= instance->options.logs[log_type];
  File fd;

  size_t buff_size;
  int read_len;

  MY_STAT file_stat;
  Buffer read_buff;

  if ((fd= my_open(logpath, O_RDONLY | O_BINARY,  MYF(MY_WME))) <= 0)
    return ER_OPEN_LOGFILE;

  /* my_fstat doesn't use the flag parameter */
  if (my_fstat(fd, &file_stat, MYF(0)))
  {
    close(fd);
    return ER_OUT_OF_RESOURCES;
  }

  /* calculate buffer size */
  buff_size= (size - offset);

  read_buff.reserve(0, buff_size);

  /* read in one chunk */
  read_len= (int)my_seek(fd, file_stat.st_size - size, MY_SEEK_SET, MYF(0));

  if ((read_len= my_read(fd, (byte*) read_buff.buffer,
                         buff_size, MYF(0))) < 0)
  {
    close(fd);
    return ER_READ_FILE;
  }

  close(fd);

  if (store_to_protocol_packet(&send_buff, read_buff.buffer, &pos, read_len) ||
      my_net_write(net, send_buff.buffer, pos))
  {
    return ER_OUT_OF_RESOURCES;
  }

  return 0;
}


/**************************************************************************
 Implementation of Show_instance_log_files.
**************************************************************************/

Show_instance_log_files::Show_instance_log_files
              (Instance_map *instance_map_arg,
               const LEX_STRING *instance_name_arg)
  :Abstract_instance_cmd(instance_map_arg, instance_name_arg)
{
}


/*
  Implementation of SHOW INSTANCE LOG FILES statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Show_instance_log_files::execute_impl(st_net *net, Instance *instance)
{
  int err_code;

  if ((err_code= write_header(net)) ||
      (err_code= write_data(net, instance)))
    return err_code;

  return 0;
}


int Show_instance_log_files::send_ok_response(st_net *net, ulong connection_id)
{
  if (send_eof(net) || net_flush(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


int Show_instance_log_files::write_header(st_net *net)
{
  LIST name, path, size;
  LIST *field_list;
  LEX_STRING name_field, path_field, size_field;

  /* Create list of the fileds to be passed to send_fields(). */

  name_field.str= (char *) "Logfile";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;

  path_field.str= (char *) "Path";
  path_field.length= DEFAULT_FIELD_LENGTH;
  path.data= &path_field;

  size_field.str= (char *) "File size";
  size_field.length= DEFAULT_FIELD_LENGTH;
  size.data= &size_field;

  field_list= list_add(NULL, &size);
  field_list= list_add(field_list, &path);
  field_list= list_add(field_list, &name);

  return send_fields(net, field_list) ? ER_OUT_OF_RESOURCES : 0;
}


int Show_instance_log_files::write_data(st_net *net, Instance *instance)
{
  Buffer send_buff;  /* buffer for packets */

  /*
    We have alike structure in instance_options.cc. We use such to be able
    to loop through the options, which we need to handle in some common way.
  */
  struct log_files_st
  {
    const char *name;
    const char *value;
  } logs[]=
  {
    {"ERROR LOG", instance->options.logs[IM_LOG_ERROR]},
    {"GENERAL LOG", instance->options.logs[IM_LOG_GENERAL]},
    {"SLOW LOG", instance->options.logs[IM_LOG_SLOW]},
    {NULL, NULL}
  };
  struct log_files_st *log_files;

  for (log_files= logs; log_files->name; log_files++)
  {
    if (!log_files->value)
      continue;

    struct stat file_stat;
    /*
      Save some more space for the log file names. In fact all
      we need is strlen("GENERAL_LOG") + 1
    */
    enum { LOG_NAME_BUFFER_SIZE= 20 };
    char buff[LOG_NAME_BUFFER_SIZE];

    uint pos= 0;

    const char *log_path= "";
    const char *log_size= "0";

    if (!stat(log_files->value, &file_stat) &&
        MY_S_ISREG(file_stat.st_mode))
    {
      int10_to_str(file_stat.st_size, buff, 10);

      log_path= log_files->value;
      log_size= buff;
    }

    if (store_to_protocol_packet(&send_buff, log_files->name, &pos) ||
        store_to_protocol_packet(&send_buff, log_path, &pos) ||
        store_to_protocol_packet(&send_buff, log_size, &pos) ||
        my_net_write(net, send_buff.buffer, pos))
      return ER_OUT_OF_RESOURCES;
  }

  return 0;
}


/**************************************************************************
 Implementation of Abstract_option_cmd.
**************************************************************************/

/*
  Instance_options_list -- a data class representing a list of options for
  some instance.
*/

class Instance_options_list
{
public:
  Instance_options_list(const LEX_STRING *instance_name_arg);

public:
  bool init();

  const LEX_STRING *get_instance_name() const
  {
    return instance_name.get_str();
  }

public:
  /*
    This member is set and used only in Abstract_option_cmd::execute_impl().
    Normally it is not used (and should not).

    The problem is that construction and execution of commands are made not
    in one transaction (not under one lock session). So, we can not initialize
    instance in constructor and use it in execution.
  */
  Instance *instance;

  Named_value_arr options;

private:
  Instance_name instance_name;
};


/**************************************************************************/

Instance_options_list::Instance_options_list(
  const LEX_STRING *instance_name_arg)
  :instance(NULL),
  instance_name(instance_name_arg)
{
}


bool Instance_options_list::init()
{
  return options.init();
}


/**************************************************************************/

C_MODE_START

static byte* get_item_key(const byte* item, uint* len,
                          my_bool __attribute__((unused)) t)
{
  const Instance_options_list *lst= (const Instance_options_list *) item;
  *len= lst->get_instance_name()->length;
  return (byte *) lst->get_instance_name()->str;
}

static void delete_item(void *item)
{
  delete (Instance_options_list *) item;
}

C_MODE_END


/**************************************************************************/

Abstract_option_cmd::Abstract_option_cmd(Instance_map *instance_map_arg)
  :Command(instance_map_arg),
  initialized(FALSE)
{
}


Abstract_option_cmd::~Abstract_option_cmd()
{
  if (initialized)
    hash_free(&instance_options_map);
}


bool Abstract_option_cmd::add_option(const LEX_STRING *instance_name,
                                     Named_value *option)
{
  Instance_options_list *lst= get_instance_options_list(instance_name);

  if (!lst)
    return TRUE;

  lst->options.add_element(option);

  return FALSE;
}


bool Abstract_option_cmd::init(const char **text)
{
  static const int INITIAL_HASH_SIZE= 16;

  if (hash_init(&instance_options_map, default_charset_info,
                INITIAL_HASH_SIZE, 0, 0, get_item_key, delete_item, 0))
    return TRUE;

  if (parse_args(text))
    return TRUE;

  initialized= TRUE;

  return FALSE;
}


/*
  Correct the option file. The "skip" option is used to remove the found
  option.

  SYNOPSYS
  Abstract_option_cmd::correct_file()
    skip     Skip the option, being searched while writing the result file.
             That is, to delete it.

  RETURN
    0                       Success
    ER_OUT_OF_RESOURCES     Not enough resources to complete the operation
    ER_ACCESS_OPTION_FILE   Cannot access the option file
*/

int Abstract_option_cmd::correct_file(Instance *instance, Named_value *option,
                                      bool skip)
{
  int err_code= modify_defaults_file(Options::Main::config_file,
                                     option->get_name(),
                                     option->get_value(),
                                     instance->get_name()->str,
                                     skip);

  DBUG_ASSERT(err_code >= 0 && err_code <= 2);

  if (err_code)
  {
    log_error("Can not modify option (%s) in defaults file (%s). "
              "Original error code: %d.",
              (const char *) option->get_name(),
              (const char *) Options::Main::config_file,
              (int) err_code);
  }

  return modify_defaults_to_im_error[err_code];
}


/*
  Implementation of SET statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance with the given name does not exist
    ER_INCOMPATIBLE_OPTION      The specified option can not be set for
                                mysqld-compatible instance
    ER_INSTANCE_IS_ACTIVE       The specified instance is active
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Abstract_option_cmd::execute(st_net *net, ulong connection_id)
{
  int err_code;

  instance_map->lock();

  err_code= execute_impl(net, connection_id);

  instance_map->unlock();

  return err_code;
}


Instance_options_list *
Abstract_option_cmd::get_instance_options_list(const LEX_STRING *instance_name)
{
  Instance_options_list *lst=
    (Instance_options_list *) hash_search(&instance_options_map,
                                        (byte *) instance_name->str,
                                        instance_name->length);

  if (!lst)
  {
    lst= new Instance_options_list(instance_name);

    if (!lst)
      return NULL;

    if (lst->init() || my_hash_insert(&instance_options_map, (byte *) lst))
    {
      delete lst;
      return NULL;
    }
  }

  return lst;
}


int Abstract_option_cmd::execute_impl(st_net *net, ulong connection_id)
{
  int err_code;

  /* Check that all the specified instances exist and are offline. */

  for (uint i= 0; i < instance_options_map.records; ++i)
  {
    Instance_options_list *lst=
      (Instance_options_list *) hash_element(&instance_options_map, i);

    lst->instance= instance_map->find(lst->get_instance_name());

    if (!lst->instance)
      return ER_BAD_INSTANCE_NAME;

    if (instance_map->guardian->is_active(lst->instance))
      return ER_INSTANCE_IS_ACTIVE;
  }

  /* Perform command-specific (SET/UNSET) actions. */

  for (uint i= 0; i < instance_options_map.records; ++i)
  {
    Instance_options_list *lst=
      (Instance_options_list *) hash_element(&instance_options_map, i);

    for (int j= 0; j < lst->options.get_size(); ++j)
    {
      Named_value option= lst->options.get_element(j);
      err_code= process_option(lst->instance, &option);

      if (err_code)
        break;
    }

    if (err_code)
      break;
  }

  if (err_code == 0)
    net_send_ok(net, connection_id, NULL);

  return err_code;
}


/**************************************************************************
 Implementation of Set_option.
**************************************************************************/

Set_option::Set_option(Instance_map *instance_map_arg)
  :Abstract_option_cmd(instance_map_arg)
{
}


/*
  This operation parses SET options.

  SYNOPSYS
    text            [IN/OUT] a pointer to the text containing options.

  RETURN
    FALSE           On success.
    TRUE            On syntax error.
*/

bool Set_option::parse_args(const char **text)
{
  uint len;

  /* Check if we have something (and trim leading spaces). */

  get_word(text, &len, NONSPACE);

  if (len == 0)
    return TRUE; /* Syntax error: no option. */

  /* Main parsing loop. */

  while (TRUE)
  {
    LEX_STRING instance_name;
    LEX_STRING option_name;
    char *option_name_str;
    char *option_value_str= NULL;

    /* Looking for instance name. */

    get_word(text, &instance_name.length, ALPHANUM);

    if (instance_name.length == 0)
      return TRUE; /* Syntax error: instance name expected. */

    instance_name.str= (char *) *text;
    *text+= instance_name.length;

    skip_spaces(text);

    /* Check the the delimiter is a dot. */

    if (**text != '.')
      return TRUE; /* Syntax error: dot expected. */

    ++(*text);

    /* Looking for option name. */

    get_word(text, &option_name.length, OPTION_NAME);

    if (option_name.length == 0)
      return TRUE; /* Syntax error: option name expected. */

    option_name.str= (char *) *text;
    *text+= option_name.length;

    /* Looking for equal sign. */

    skip_spaces(text);

    if (**text == '=')
    {
      ++(*text); /* Skip an equal sign. */

      /* Looking for option value. */

      skip_spaces(text);

      if (!**text)
        return TRUE; /* Syntax error: EOS when option value expected. */

      if (**text != '\'' && **text != '"')
      {
        /* Option value is a simple token. */

        LEX_STRING option_value;

        get_word(text, &option_value.length, ALPHANUM);

        if (option_value.length == 0)
          return TRUE; /* internal parser error. */

        option_value.str= (char *) *text;
        *text+= option_value.length;

        if (!(option_value_str= Named_value::alloc_str(&option_value)))
          return TRUE; /* out of memory during parsing. */
      }
      else
      {
        /* Option value is a string. */

        if (parse_option_value(*text, &len, &option_value_str))
          return TRUE; /* Syntax error: invalid string specification. */

        *text+= len;
      }
    }

    if (!option_value_str)
    {
      LEX_STRING empty_str= { C_STRING_WITH_LEN("") };

      if (!(option_value_str= Named_value::alloc_str(&empty_str)))
        return TRUE; /* out of memory during parsing. */
    }

    if (!(option_name_str= Named_value::alloc_str(&option_name)))
    {
      Named_value::free_str(&option_name_str);
      return TRUE; /* out of memory during parsing. */
    }

    {
      Named_value option(option_name_str, option_value_str);

      if (add_option(&instance_name, &option))
      {
        option.free();
        return TRUE; /* out of memory during parsing. */
      }
    }

    skip_spaces(text);

    if (!**text)
      return FALSE; /* OK: end of options. */

    if (**text != ',')
      return TRUE; /* Syntax error: comma expected. */

    ++(*text); /* Skip a comma. */
  }
}


int Set_option::process_option(Instance *instance, Named_value *option)
{
  /* Check that the option is valid. */

  if (instance->is_mysqld_compatible() &&
      Instance_options::is_option_im_specific(option->get_name()))
  {
      log_error("Error: IM-option (%s) can not be used "
                "in the configuration of mysqld-compatible instance (%s).",
                (const char *) option->get_name(),
                (const char *) instance->get_name()->str);
      return ER_INCOMPATIBLE_OPTION;
  }

  /* Update the configuration file. */

  int err_code= correct_file(instance, option, FALSE);

  if (err_code)
    return err_code;

  /* Update the internal cache. */

  if (instance->options.set_option(option))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/**************************************************************************
 Implementation of Unset_option.
**************************************************************************/

Unset_option::Unset_option(Instance_map *instance_map_arg)
  :Abstract_option_cmd(instance_map_arg)
{
}


/*
  This operation parses UNSET options.

  SYNOPSYS
    text            [IN/OUT] a pointer to the text containing options.

  RETURN
    FALSE           On success.
    TRUE            On syntax error.
*/

bool Unset_option::parse_args(const char **text)
{
  uint len;

  /* Check if we have something (and trim leading spaces). */

  get_word(text, &len, NONSPACE);

  if (len == 0)
    return TRUE; /* Syntax error: no option. */

  /* Main parsing loop. */

  while (TRUE)
  {
    LEX_STRING instance_name;
    LEX_STRING option_name;
    char *option_name_str;
    char *option_value_str;

    /* Looking for instance name. */

    get_word(text, &instance_name.length, ALPHANUM);

    if (instance_name.length == 0)
      return TRUE; /* Syntax error: instance name expected. */

    instance_name.str= (char *) *text;
    *text+= instance_name.length;

    skip_spaces(text);

    /* Check the the delimiter is a dot. */

    if (**text != '.')
      return TRUE; /* Syntax error: dot expected. */

    ++(*text); /* Skip a dot. */

    /* Looking for option name. */

    get_word(text, &option_name.length, OPTION_NAME);

    if (option_name.length == 0)
      return TRUE; /* Syntax error: option name expected. */

    option_name.str= (char *) *text;
    *text+= option_name.length;

    if (!(option_name_str= Named_value::alloc_str(&option_name)))
      return TRUE; /* out of memory during parsing. */

    {
      LEX_STRING empty_str= { C_STRING_WITH_LEN("") };

      if (!(option_value_str= Named_value::alloc_str(&empty_str)))
      {
        Named_value::free_str(&option_name_str);
        return TRUE;
      }
    }

    {
      Named_value option(option_name_str, option_value_str);

      if (add_option(&instance_name, &option))
      {
        option.free();
        return TRUE; /* out of memory during parsing. */
      }
    }

    skip_spaces(text);

    if (!**text)
      return FALSE; /* OK: end of options. */

    if (**text != ',')
      return TRUE; /* Syntax error: comma expected. */

    ++(*text); /* Skip a comma. */
  }
}


/*
  Implementation of UNSET statement.

  Possible error codes:
    ER_BAD_INSTANCE_NAME        The instance name specified is not valid
    ER_INSTANCE_IS_ACTIVE       The specified instance is active
    ER_OUT_OF_RESOURCES         Not enough resources to complete the operation
*/

int Unset_option::process_option(Instance *instance, Named_value *option)
{
  /* Update the configuration file. */

  int err_code= correct_file(instance, option, TRUE);

  if (err_code)
    return err_code;

  /* Update the internal cache. */

  instance->options.unset_option(option->get_name());

  return 0;
}


/**************************************************************************
 Implementation of Syntax_error.
**************************************************************************/

int Syntax_error::execute(st_net *net, ulong connection_id)
{
  return ER_SYNTAX_ERROR;
}
