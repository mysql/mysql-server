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

#include <m_string.h>
#include <mysql.h>


/* some useful functions */

static int put_to_buff(Buffer *buff, const char *str, uint *position)
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
    Show_instances::do_command()
    net               The network connection to the client.

  RETURN
    0 - ok
    1 - error occured
*/

int Show_instances::do_command(struct st_net *net)
{
  Buffer send_buff;  /* buffer for packets */
  LIST name, status;
  NAME_WITH_LENGTH name_field, status_field;
  LIST *field_list;
  uint position=0;

  name_field.name= (char *) "instance_name";
  name_field.length= 20;
  name.data= &name_field;
  status_field.name= (char *) "status";
  status_field.length= 20;
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
      store_to_string(&send_buff, instance->options.instance_name, &position);
      if (instance->is_running())
        store_to_string(&send_buff, (char *) "online", &position);
      else
        store_to_string(&send_buff, (char *) "offline", &position);
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
  return 1;
}


int Show_instances::execute(struct st_net *net, ulong connection_id)
{
  if (do_command(net))
    return ER_OUT_OF_RESOURCES;

  return 0;
}


/* implementation for Flush_instances: */

int Flush_instances::execute(struct st_net *net, ulong connection_id)
{
  if (instance_map->flush_instances())
    return ER_OUT_OF_RESOURCES;

  net_send_ok(net, connection_id, NULL);
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
  {
    instance_name= instance->options.instance_name;
  }
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


int Show_instance_status::do_command(struct st_net *net,
                                     const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, status, version;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, status_field, version_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "instance_name";
  name_field.length= 20;
  name.data= &name_field;
  status_field.name= (char *) "status";
  status_field.length= 20;
  status.data= &status_field;
  version_field.name= (char *) "version";
  version_field.length= MAX_VERSION_LENGTH;
  version.data= &version_field;
  field_list= list_add(NULL, &version);
  field_list= list_add(field_list, &status);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    store_to_string(&send_buff, (char *) instance_name, &position);
    if (!(instance= instance_map->find(instance_name, strlen(instance_name))))
      goto err;
    if (instance->is_running())
    {
      store_to_string(&send_buff, (char *) "online", &position);
      store_to_string(&send_buff, "unknown", &position);
    }
    else
    {
      store_to_string(&send_buff, (char *) "offline", &position);
      store_to_string(&send_buff, (char *) "unknown", &position);
    }


    if (send_buff.is_error() ||
        my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
  }

  send_eof(net);
  net_flush(net);

  return 0;

err:
  return 1;
}


int Show_instance_status::execute(struct st_net *net, ulong connection_id)
{
  if ((instance_name))
  {
    if (do_command(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* Implementation for Show_instance_options */

Show_instance_options::Show_instance_options(Instance_map *instance_map_arg,
                                             const char *name, uint len):
  Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


int Show_instance_options::do_command(struct st_net *net,
                                      const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, option;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, option_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "option_name";
  name_field.length= 20;
  name.data= &name_field;
  option_field.name= (char *) "value";
  option_field.length= 20;
  option.data= &option_field;
  field_list= list_add(NULL, &option);
  field_list= list_add(field_list, &name);

  send_fields(net, field_list);

  {
    Instance *instance;

    if (!(instance= instance_map->find(instance_name, strlen(instance_name))))
      goto err;
    store_to_string(&send_buff, (char *) "instance_name", &position);
    store_to_string(&send_buff, (char *) instance_name, &position);
    if (my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
    if ((instance->options.mysqld_path))
    {
      position= 0;
      store_to_string(&send_buff, (char *) "mysqld-path", &position);
      store_to_string(&send_buff,
                     (char *) instance->options.mysqld_path,
                     &position);
      if (send_buff.is_error() ||
          my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }

    if ((instance->options.nonguarded))
    {
      position= 0;
      store_to_string(&send_buff, (char *) "nonguarded", &position);
      store_to_string(&send_buff, "", &position);
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
        store_to_string(&send_buff, tmp_option + 2, &position);
        store_to_string(&send_buff, option_value + 1, &position);
        /* join name and the value into the same option again */
        *option_value= '=';
      }
      else store_to_string(&send_buff, tmp_option + 2, &position);
      if (send_buff.is_error() ||
          my_net_write(net, send_buff.buffer, (uint) position))
        goto err;
    }
  }

  send_eof(net);
  net_flush(net);

  return 0;

err:
  return 1;
}


int Show_instance_options::execute(struct st_net *net, ulong connection_id)
{
  if ((instance_name))
  {
    if (do_command(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
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
  {
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */
  }
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
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


int Show_instance_log::do_command(struct st_net *net,
                                  const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name;
  LIST *field_list;
  NAME_WITH_LENGTH name_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "Log";
  name_field.length= 20;
  name.data= &name_field;
  field_list= list_add(NULL, &name);

  /* cannot read negative number of bytes */
  if (offset > size)
    return ER_SYNTAX_ERROR;

  send_fields(net, field_list);

  {
    Instance *instance;
    const char *logpath;
    File fd;

    if ((instance= instance_map->find(instance_name, strlen(instance_name))) == NULL)
      goto err;

    switch (log_type)
    {
    case LOG_ERROR:
      logpath= instance->options.error_log;
      break;
    case LOG_GENERAL:
      logpath= instance->options.query_log;
      break;
    case LOG_SLOW:
      logpath= instance->options.slow_log;
      break;
    default:
      logpath= NULL;
    }

    /* Instance has no such log */
    if (logpath == NULL)
    {
      return ER_NO_SUCH_LOG;
    }
    else if (*logpath == '\0')
    {
      return ER_GUESS_LOGFILE;
    }


    if ((fd= open(logpath, O_RDONLY)))
    {
      size_t buff_size;
      int read_len;
      /* calculate buffer size */
      struct stat file_stat;

      if(fstat(fd, &file_stat))
        goto err;

      buff_size= (size - offset);

      /* read in one chunk */
      read_len= my_seek(fd, file_stat.st_size - size, MY_SEEK_SET, MYF(0));

      char *bf= (char *) malloc(sizeof(char)*buff_size);
      read_len= my_read(fd, bf, buff_size, MYF(0));
      store_to_string(&send_buff, (char *) bf, &position, read_len);
      close(fd);
    }
    else
    {
      return ER_OPEN_LOGFILE;
    }

    if (my_net_write(net, send_buff.buffer, (uint) position))
      goto err;
  }

  send_eof(net);
  net_flush(net);

  return 0;

err:
  return ER_OUT_OF_RESOURCES;
}


int Show_instance_log::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    return do_command(net, instance_name);
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}



/* implementation for Show_instance_log_files: */

Show_instance_log_files::Show_instance_log_files
              (Instance_map *instance_map_arg, const char *name, uint len)
  :Command(instance_map_arg)
{
  Instance *instance;

  /* we make a search here, since we don't want to store the name */
  if ((instance= instance_map->find(name, len)))
  {
    instance_name= instance->options.instance_name;
  }
  else
    instance_name= NULL;
}


/*
  The method sends a table with a the list of the log files
  used by the instance.

  SYNOPSYS
    Show_instance_log_files::do_command()
    net               The network connection to the client.
    instance_name     The name of the instance.

  RETURN
    0 - ok
    1 - error occured
*/


int Show_instance_log_files::do_command(struct st_net *net,
                                        const char *instance_name)
{
  enum { MAX_VERSION_LENGTH= 40 };
  Buffer send_buff;  /* buffer for packets */
  LIST name, path, size;
  LIST *field_list;
  NAME_WITH_LENGTH name_field, path_field, size_field;
  uint position=0;

  /* create list of the fileds to be passed to send_fields */
  name_field.name= (char *) "Logfile";
  name_field.length= 20;
  name.data= &name_field;
  path_field.name= (char *) "Path";
  path_field.length= 20;
  path.data= &path_field;
  size_field.name= (char *) "Filesize";
  size_field.length= 20;
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
      to loop througt the options, which we need to handle in some common way.
    */
    struct log_files_st
    {
      const char *name;
      const char *value;
    } logs[]=
    {
      {"ERROR LOG", instance->options.error_log},
      {"GENERAL LOG", instance->options.query_log},
      {"SLOW LOG", instance->options.slow_log},
      {NULL, NULL}
    };
    struct log_files_st *log_files;


    instance->options.print_argv();
    for (log_files= logs; log_files->name; log_files++)
    {
      if (log_files->value != NULL)
      {
        struct stat file_stat;
        char buff[20];

        position= 0;
        /* store the type of the log in the send buffer */
        store_to_string(&send_buff, log_files->name, &position);
        switch (stat(log_files->value, &file_stat)) {
        case 0:
          if (S_ISREG(file_stat.st_mode))
          {
            store_to_string(&send_buff,
                            (char *) log_files->value,
                            &position);
            int10_to_str(file_stat.st_size, buff, 10);
            store_to_string(&send_buff, (char *) buff, &position);
            break;
          }
        default:
          store_to_string(&send_buff,
                          "",
                          &position);
          store_to_string(&send_buff, (char *) "0", &position);
        }
        if (my_net_write(net, send_buff.buffer, (uint) position))
          goto err;
      }
    }
  }

  send_eof(net);
  net_flush(net);

  return 0;

err:
  return 1;
}
int Show_instance_log_files::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    if (do_command(net, instance_name))
      return ER_OUT_OF_RESOURCES;
    return 0;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* implementation for SET nstance_name.option=option_value: */

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
      strncpy(option, option_arg, option_len_arg);
      option[option_len_arg]= 0;
      strncpy(option_value, option_value_arg, option_value_len_arg);
      option_value[option_value_len_arg]= 0;
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
  Correct the file. skip option could be used in future if we don't want to
  let user change the options file (E.g. he lacks permissions to do that)
*/
int Set_option::correct_file(bool skip)
{
  FILE *cnf_file;
  const char *default_location="/etc/my.cnf";
  char linebuff[4096], *ptr;
  uint optlen;
  Buffer file_buffer;
  uint position= 0;
  bool isfound= false;

  optlen= strlen(option);

  if (!(cnf_file= my_fopen(default_location, O_RDONLY, MYF(0))))
    goto err_fopen;

  while (fgets(linebuff, sizeof(linebuff), cnf_file))
  {
    /* if the section is found traverse it */
    if (isfound)
    {
        /* skip the old value of the option we are changing */
      if (strncmp(linebuff, option, optlen))
      {
        /* copy all other lines line */
        put_to_buff(&file_buffer, linebuff, &position);
      }
    }
    else
      put_to_buff(&file_buffer, linebuff, &position);

    /* looking for appropriate instance section */
    for (ptr= linebuff ; my_isspace(&my_charset_latin1,*ptr) ; ptr++);
    if (*ptr == '[')
    {
      /* copy the line to the buffer */
      if (!strncmp(++ptr, instance_name, instance_name_len))
      {
        isfound= true;
        /* add option */
        if (!skip)
        {
          put_to_buff(&file_buffer, option, &position);
          if (option_value[0] != 0)
          {
           put_to_buff(&file_buffer, "=", &position);
           put_to_buff(&file_buffer, option_value, &position);
          }
          /* add a newline */
          put_to_buff(&file_buffer, "\n", &position);
        }
      }
      else
        isfound= false; /* mark that this section is of no interest to us */
    }

  }

  if (my_fclose(cnf_file, MYF(0)))
    goto err;

  /* we must hold an instance_map mutex while changing config file */
  instance_map->lock();

  if (!(cnf_file= my_fopen(default_location, O_WRONLY|O_TRUNC, MYF(0))))
    goto err;
  if (my_fwrite(cnf_file, file_buffer.buffer, position, MYF(MY_NABP)))
    goto err;

  if (my_fclose(cnf_file, MYF(0)))
    goto err;

  instance_map->unlock();

  return 0;

err:
  my_fclose(cnf_file, MYF(0));
  return ER_OUT_OF_RESOURCES;
err_fopen:
  return ER_ACCESS_OPTION_FILE;
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
  return correct_file(false);
}


int Set_option::execute(struct st_net *net, ulong connection_id)
{
  if (instance_name != NULL)
  {
    int val;

    val= do_command(net);
    if (val == 0)
    {
      net_send_ok(net, connection_id, NULL);
      return 0;
    }

    return val;
  }
  else
  {
    return ER_BAD_INSTANCE_NAME;
  }
}


/* the only function from Unset_option we need to Implement */

int Unset_option::do_command(struct st_net *net)
{
  return correct_file(true);
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
  {
    return ER_BAD_INSTANCE_NAME; /* haven't found an instance */
  }
  else
  {
    if (!(instance->options.nonguarded))
        instance_map->guardian->
               stop_guard(instance);
    if ((err_code= instance->stop()))
      return err_code;
    net_send_ok(net, connection_id, NULL);
    return 0;
  }
}


int Syntax_error::execute(struct st_net *net, ulong connection_id)
{
  return ER_SYNTAX_ERROR;
}
