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

#include "commands.h"

#include "instance_map.h"
#include "messages.h"
#include "mysqld_error.h"
#include "mysql_manager_error.h"
#include "protocol.h"
#include "buffer.h"
#include "options.h"

#include <m_string.h>
#include <mysql.h>
#include <my_dir.h>


/*
  Add a string to a buffer

  SYNOPSYS
    put_to_buff()
    buff              buffer to add the string
    str               string to add
    uint              offset in the buff to add a string

  DESCRIPTION

  Function to add a string to the buffer. It is different from
  store_to_protocol_packet, which is used in the protocol.cc. The last
  one also stores the length of the string in a special way.
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


/* implementation for Show_instances: */


/*
  The method sends a list of instances in the instance map to the client.

  SYNOPSYS
    Show_instances::execute()
    net                    The network connection to the client.
    connection_id          Client connection ID

  RETURN
    0 - ok
    1 - error occured
*/

int Show_instances::execute(struct st_net *net, ulong connection_id)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name, status;
  NAME_WITH_LENGTH name_field, status_field;
  LIST *field_list;
  uint position=0;

  name_field.name= (char*) "instance_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;
  status_field.name= (char*) "status";
  status_field.length= DEFAULT_FIELD_LENGTH;
  status.data= &status_field;
  field_list= list_add(NULL, &status);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;
    Instance_map::Iterator iterator(instance_map);

    instance_map->lock();
    while ((instance= iterator.next()))
    {
      position= 0;
      store_to_protocol_packet(&send_buff, instance->options.instance_name,
                               &position);
      if (instance->is_running())
        store_to_protocol_packet(&send_buff, (char*) "online", &position);
      else
        store_to_protocol_packet(&send_buff, (char*) "offline", &position);
      if (my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }
    instance_map->unlock();
  }
  if (send_eof(net))
    goto err;
  if (net_flush(net))
    goto err;

  return 0;
err:
  return ER_OUT_OF_RESOURCES;
}


/* implementation for Flush_instances: */

int Flush_instances::execute(struct st_net *net, ulong connection_id)
{
  if (instance_map->flush_instances() ||
      net_send_ok(net, connection_id, NULL))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/* implementation for Show_instance_status: */

Show_instance_status::Show_instance_status(Instance_map *instance_map_arg,
                                           const char *name, uint len)
  :Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
  else
    instance_name= NULL;
}


/*
  The method sends a table with a status of requested instance to the client.

  SYNOPSYS
    Show_instance_status::do_command()
    net               The network connection to the client.
    instance_name     The name of the instance.

  RETURN
    0 - ok
    1 - error occured
*/


int Show_instance_status::execute(struct st_net *net,
                                  ulong connection_id)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, status, version;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, status_field, version_field;
  uint position=0;

  if (!instance_name)
    return ER_BAD_INSTANCE_NAME;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char*) "instance_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;
  status_field.name= (char*) "status";
  status_field.length= DEFAULT_FIELD_LENGTH;
  status.data= &status_field;
  version_field.name= (char*) "version";
  version_field.length= MAX_VERSION_LENGTH;
  version.data= &version_field;
  field_list= list_add(NULL, &version);
  field_list= list_add(field_list, &status);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    store_to_protocol_packet(&send_buff, (char*) instance_name, &position);
    if (!(instance= instance_map->find(instance_name, strlen(instance_name))))
      goto err;
    if (instance->is_running())
      store_to_protocol_packet(&send_buff, (char*) "online", &position);
    else
      store_to_protocol_packet(&send_buff, (char*) "offline", &position);

    if (instance->options.mysqld_version)
      store_to_protocol_packet(&send_buff, instance->options.mysqld_version,
                               &position);
    else
      store_to_protocol_packet(&send_buff, (char*) "unknown", &position);


    if (send_buff.is_error() ||
        my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
  }

  if (send_eof(net) || net_flush(net))
    goto err;

  return 0;

err:
  return ER_OUT_OF_RESOURCES;
}


/* Implementation for Show_instance_options */

Show_instance_options::Show_instance_options(Instance_map *instance_map_arg,
                                             const char *name, uint len):
  Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
  else
    instance_name= NULL;
}


int Show_instance_options::execute(struct st_net *net, ulong connection_id)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name, option;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, option_field;
  uint position=0;

  if (!instance_name)
    return ER_BAD_INSTANCE_NAME;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char*) "option_name";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;
  option_field.name= (char*) "value";
  option_field.length= DEFAULT_FIELD_LENGTH;
  option.data= &option_field;
  field_list= list_add(NULL, &option);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    if (!(instance= instance_map->find(instance_name, strlen(instance_name))))
      goto err;
    store_to_protocol_packet(&send_buff, (char*) "instance_name", &position);
    store_to_protocol_packet(&send_buff, (char*) instance_name, &position);
    if (my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
    if ((instance->options.mysqld_path))
    {
      position= 0;
      store_to_protocol_packet(&send_buff, (char*) "mysqld-path", &position);
      store_to_protocol_packet(&send_buff,
                               (char*) instance->options.mysqld_path,
                               &position);
      if (send_buff.is_error() ||
          my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }

    if ((instance->options.nonguarded))
    {
      position= 0;
      store_to_protocol_packet(&send_buff, (char*) "nonguarded", &position);
      store_to_protocol_packet(&send_buff, "", &position);
      if (send_buff.is_error() ||
          my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }

    /* loop through the options stored in DYNAMIC_ARRAY */
    for (uint i= 0; i < instance->options.options_array.elements; i++)
    {
      char *tmp_option, *option_value;
      get_dynamic(&(instance->options.options_array), (gptr) &tmp_option, i);
      option_value= strchr(tmp_option, '=');
      /* split the option string into two parts if it has a value */

      position= 0;
      if (option_value != NULL)
      {
        *option_value= 0;
        store_to_protocol_packet(&send_buff, tmp_option + 2, &position);
        store_to_protocol_packet(&send_buff, option_value + 1, &position);
        /* join name and the value into the same option again */
        *option_value= '=';
      }
      else
      {
        store_to_protocol_packet(&send_buff, tmp_option + 2, &position);
        store_to_protocol_packet(&send_buff, "", &position);
      }

      if (send_buff.is_error() ||
          my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }
  }

  if (send_eof(net) || net_flush(net))
    goto err;

  return 0;

err:
  return ER_OUT_OF_RESOURCES;
}


/* Implementation for Start_instance */

Start_instance::Start_instance(Instance_map *instance_map_arg,
                               const char *name, uint len)
  :Command(instance_map_arg)
{
  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
}


int Start_instance::execute(struct st_net *net, ulong connection_id)
{
  uint err_code;
  if (instance == 0)
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */
  else
  {
    if ((err_code= instance->start()))
      return err_code;

    if (!(instance->options.nonguarded))
        instance_map->guardian->guard(instance);

    net_send_ok(net, connection_id, "Instance started");
    return 0;
  }
}


/* implementation for Show_instance_log: */

Show_instance_log::Show_instance_log(Instance_map *instance_map_arg,
                                     const char *name, uint len,
                                     Log_type log_type_arg,
                                     const char *size_arg,
                                     const char *offset_arg)
  :Command(instance_map_arg)
{
  Instance *instance;

  if (offset_arg != NULL)
    offset= atoi(offset_arg);
  else
    offset= 0;
  size= atoi(size_arg);
  log_type= log_type_arg;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
  else
    instance_name= NULL;
}



/*
  Open the logfile, read requested part of the log and send the info
  to the client.

  SYNOPSYS
    Show_instance_log::execute()
    net                 The network connection to the client.
    connection_id       Client connection ID

  DESCRIPTION

    Send a table with the content of the log requested. The function also
    deals with errro handling, to be verbose.

  RETURN
   ER_OFFSET_ERROR      We were requested to read negative number of bytes
                        from the log
   ER_NO_SUCH_LOG       The kind log being read is not enabled in the instance
   ER_GUESS_LOGFILE     IM wasn't able to figure out the log placement, while
                        it is enabled. Probably user should specify the path
                        to the logfile explicitly.
   ER_OPEN_LOGFILE      Cannot open the logfile
   ER_READ_FILE         Cannot read the logfile
   ER_OUT_OF_RESOURCES  We weren't able to allocate some resources
*/

int Show_instance_log::execute(struct st_net *net, ulong connection_id)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name;
  LIST *field_list;
  NAME_WITH_LENGTH name_field;
  uint position= 0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char*) "Log";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;
  field_list= list_add(NULL, &name);

  if (!instance_name)
    return ER_BAD_INSTANCE_NAME;

  /* cannot read negative number of bytes */
  if (offset > size)
    return ER_OFFSET_ERROR;

  send_fields(net, field_list);

  {
    Instance *instance;
    const char *logpath;
    File fd;

    if ((instance= instance_map->find(instance_name,
                                      strlen(instance_name))) == NULL)
      goto err;

    logpath= instance->options.logs[log_type];

    /* Instance has no such log */
    if (logpath == NULL)
      return ER_NO_SUCH_LOG;

    if (*logpath == '\0')
      return ER_GUESS_LOGFILE;


    if ((fd= my_open(logpath, O_RDONLY | O_BINARY,  MYF(MY_WME))) >= 0)
    {
      size_t buff_size;
      int read_len;
      /* calculate buffer size */
      MY_STAT file_stat;
      Buffer read_buff;

      /* my_fstat doesn't use the flag parameter */
      if (my_fstat(fd, &file_stat, MYF(0)))
        goto err;

      buff_size= (size - offset);

      read_buff.reserve(0, buff_size);

      /* read in one chunk */
      read_len= my_seek(fd, file_stat.st_size - size, MY_SEEK_SET, MYF(0));

      if ((read_len= my_read(fd, (byte*) read_buff.buffer,
                             buff_size, MYF(0))) < 0)
        return ER_READ_FILE;
      store_to_protocol_packet(&send_buff, read_buff.buffer,
                               &position, read_len);
      close(fd);
    }
    else
      return ER_OPEN_LOGFILE;

    if (my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
  }

  if (send_eof(net) ||  net_flush(net))
    goto err;

  return 0;

err:
  return ER_OUT_OF_RESOURCES;
}


/* implementation for Show_instance_log_files: */

Show_instance_log_files::Show_instance_log_files
              (Instance_map *instance_map_arg, const char *name, uint len)
  :Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
  else
    instance_name= NULL;
}


/*
  The method sends a table with a list of log files
  used by the instance.

  SYNOPSYS
    Show_instance_log_files::execute()
    net               The network connection to the client.
    connection_id     The ID of the client connection

  RETURN
    ER_BAD_INSTANCE_NAME  The instance name specified is not valid
    ER_OUT_OF_RESOURCES   some error occured
    0 - ok
*/

int Show_instance_log_files::execute(struct st_net *net, ulong connection_id)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name, path, size;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, path_field, size_field;
  uint position= 0;

  if (!instance_name)
    return ER_BAD_INSTANCE_NAME;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char*) "Logfile";
  name_field.length= DEFAULT_FIELD_LENGTH;
  name.data= &name_field;
  path_field.name= (char*) "Path";
  path_field.length= DEFAULT_FIELD_LENGTH;
  path.data= &path_field;
  size_field.name= (char*) "File size";
  size_field.length= DEFAULT_FIELD_LENGTH;
  size.data= &size_field;
  field_list= list_add(NULL, &size);
  field_list= list_add(field_list, &path);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  Instance *instance;

  if ((instance= instance_map->
                 find(instance_name, strlen(instance_name))) == NULL)
    goto err;

  {
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
      if (log_files->value != NULL)
      {
        struct stat file_stat;
        /*
          Save some more space for the log file names. In fact all
          we need is srtlen("GENERAL_LOG") + 1
        */
        enum { LOG_NAME_BUFFER_SIZE= 20 };
        char buff[LOG_NAME_BUFFER_SIZE];

        position= 0;
        /* store the type of the log in the send buffer */
        store_to_protocol_packet(&send_buff, log_files->name, &position);
        if (stat(log_files->value, &file_stat))
        {
          store_to_protocol_packet(&send_buff, "", &position);
          store_to_protocol_packet(&send_buff, (char*) "0", &position);
        }
        else if (MY_S_ISREG(file_stat.st_mode))
        {
          store_to_protocol_packet(&send_buff,
                                   (char*) log_files->value,
                                   &position);
          int10_to_str(file_stat.st_size, buff, 10);
          store_to_protocol_packet(&send_buff, (char*) buff, &position);
        }

        if (my_net_write(net, send_buff.buffer, (uint) position))
          goto err;
      }
    }
  }

  if (send_eof(net) || net_flush(net))
    goto err;

  return 0;

err:
  return ER_OUT_OF_RESOURCES;
}


/* implementation for SET instance_name.option=option_value: */

Set_option::Set_option(Instance_map *instance_map_arg,
                       const char *name, uint len,
                       const char *option_arg, uint option_len_arg,
                       const char *option_value_arg, uint option_value_len_arg)
  :Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
  {
    instance_name= instance->options.instance_name;

     /* add prefix for add_option */
    if ((option_len_arg < MAX_OPTION_LEN - 1) ||
        (option_value_len_arg < MAX_OPTION_LEN - 1))
    {
      strmake(option, option_arg, option_len_arg);
      strmake(option_value, option_value_arg, option_value_len_arg);
    }
    else
    {
      option[0]= 0;
      option_value[0]= 0;
    }
    instance_name_len= len;
  }
  else
  {
    instance_name= NULL;
    instance_name_len= 0;
  }
}


/*
  The method sends a table with a list of log files
  used by the instance.

  SYNOPSYS
    Set_option::correct_file()
    skip     Skip the option, being searched while writing the result file.
             That is, to delete it.

  DESCRIPTION

  Correct the option file. The "skip" option is used to remove the found
  option.

  RETURN
    ER_OUT_OF_RESOURCES     out of resources
    ER_ACCESS_OPTION_FILE   Cannot access the option file
    0 - ok
*/

int Set_option::correct_file(int skip)
{
  static const int mysys_to_im_error[]= { 0, ER_OUT_OF_RESOURCES,
                                             ER_ACCESS_OPTION_FILE };
  int error;

  error= modify_defaults_file(Options::config_file, option,
                              option_value, instance_name, skip);
  DBUG_ASSERT(error >= 0 && error <= 2);

  return mysys_to_im_error[error];
}


/*
  The method sets an option in the the default config file (/etc/my.cnf).

  SYNOPSYS
    Set_option::do_command()
    net               The network connection to the client.

  RETURN
    0 - ok
    1 - error occured
*/

int Set_option::do_command(struct st_net *net)
{
  int error;

  /* we must hold the instance_map mutex while changing config file */
  instance_map->lock();
  error= correct_file(FALSE);
  instance_map->unlock();

  return error;
}


int Set_option::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    int val;

    val= do_command(net);

    if (val == 0)
      net_send_ok(net, connection_id, NULL);

    return val;
  }

  return ER_BAD_INSTANCE_NAME;
}


/* the only function from Unset_option we need to Implement */

int Unset_option::do_command(struct st_net *net)
{
  return correct_file(TRUE);
}


/* Implementation for Stop_instance: */

Stop_instance::Stop_instance(Instance_map *instance_map_arg,
                               const char *name, uint len)
  :Command(instance_map_arg)
{
  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
    instance_name= instance->options.instance_name;
}


int Stop_instance::execute(struct st_net *net, ulong connection_id)
{
  uint err_code;

  if (instance == 0)
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */

  if (!(instance->options.nonguarded))
    instance_map->guardian->stop_guard(instance);

  if ((err_code= instance->stop()))
    return err_code;

  net_send_ok(net, connection_id, NULL);
  return 0;
}


int Syntax_error::execute(struct st_net *net, ulong connection_id)
{
  return ER_SYNTAX_ERROR;
}
