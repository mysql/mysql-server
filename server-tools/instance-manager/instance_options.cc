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

#ifdef __GNUC__
#pragma implementation
#endif

#include "instance_options.h"

#include "parse_output.h"
#include "parse.h"
#include "buffer.h"

#include <my_sys.h>
#include <signal.h>
#include <m_string.h>


/*
  Get compiled-in value of default_option

  SYNOPSYS
    get_default_option()
    result            buffer to put found value
    result_len        buffer size
    oprion_name       the name of the option, prefixed with "--"

  DESCRIPTION

   Get compile-in value of requested option from server

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::get_default_option(char *result, size_t result_len,
                                         const char *option_name)
{
  int position= 0;
  int rc= 1;
  char verbose_option[]= " --no-defaults --verbose --help";

  Buffer cmd(strlen(mysqld_path) + sizeof(verbose_option) + 1);
  if (cmd.get_size()) /* malloc succeeded */
  {
    cmd.append(position, mysqld_path, strlen(mysqld_path));
    position+=  strlen(mysqld_path);
    cmd.append(position, verbose_option, sizeof(verbose_option) - 1);
    position+= sizeof(verbose_option) - 1;
    cmd.append(position, "\0", 1);

    if (cmd.is_error())
      goto err;
    /* get the value from "mysqld --help --verbose" */
    rc= parse_output_and_get_value(cmd.buffer, option_name + 2,
                                   result, result_len);
  }

  return rc;
err:
  return 1;
}


int Instance_options::fill_log_options()
{
  /* array for the log option for mysqld */
  enum { MAX_LOG_OPTIONS= 8 };
  enum { MAX_LOG_OPTION_LENGTH= 256 };
  /* the last option must be '\0', so we reserve space for it */
  char log_options[MAX_LOG_OPTIONS + 1][MAX_LOG_OPTION_LENGTH];
  Buffer buff;
  uint position= 0;
  char **tmp_argv= argv;
  char datadir[MAX_LOG_OPTION_LENGTH];
  char hostname[MAX_LOG_OPTION_LENGTH];
  uint hostname_length;
  struct log_files_st
  {
    const char *name;
    uint length;
    const char **value;
    const char *default_suffix;
  } logs[]=
  {
    {"--log-error", 11, &error_log, ".err"},
    {"--log", 5, &query_log, ".log"},
    {"--log-slow-queries", 18, &slow_log, "-slow.log"},
    {NULL, 0, NULL, NULL}
  };
  struct log_files_st *log_files;

  /* clean the buffer before usage */
  bzero(log_options, sizeof(log_options));

  /* create a "mysqld <argv_options>" command in the buffer */
  buff.append(position, mysqld_path, strlen(mysqld_path));
  position=  strlen(mysqld_path);

  /* skip the first option */
  tmp_argv++;

  while (*tmp_argv != 0)
  {
    buff.append(position, " ", 1);
    position++;
    buff.append(position, *tmp_argv, strlen(*tmp_argv));
    position+= strlen(*tmp_argv);
    tmp_argv++;
  }

  buff.append(position, "\0", 1);
  position++;

  /* get options and parse them */
  if (parse_arguments(buff.buffer, "--log", (char *) log_options,
                      MAX_LOG_OPTIONS + 1, MAX_LOG_OPTION_LENGTH))
    goto err;
  /* compute hostname and datadir for the instance */
  if (mysqld_datadir == NULL)
  {
    if (get_default_option(datadir,
                           MAX_LOG_OPTION_LENGTH, "--datadir"))
      goto err;
  }
  else           /* below is safe, as --datadir always has a value */
    strncpy(datadir, strchr(mysqld_datadir, '=') + 1,
            MAX_LOG_OPTION_LENGTH);

  if (gethostname(hostname,sizeof(hostname)-1) < 0)
    strmov(hostname, "mysql");

  hostname[MAX_LOG_OPTION_LENGTH - 1]= 0; /* Safety */
  hostname_length= strlen(hostname);


  for (log_files= logs; log_files->name; log_files++)
  {
    for (int i=0; (i < MAX_LOG_OPTIONS) && (log_options[i][0] != '\0'); i++)
    {
      if (!strncmp(log_options[i], log_files->name, log_files->length))
      {
        /*
          This is really log_files->name option if and only if it is followed
          by '=', '\0' or space character. This way we can distinguish such
          options as '--log' and '--log-bin'. This is checked in the following
          two statements.
        */
        if (log_options[i][log_files->length] == '\0' ||
            my_isspace(default_charset_info, log_options[i][log_files->length]))
        {
          char full_name[MAX_LOG_OPTION_LENGTH];

          fn_format(full_name, hostname, datadir, "",
                    MY_UNPACK_FILENAME | MY_SAFE_PATH);


          if ((MAX_LOG_OPTION_LENGTH - strlen(full_name)) >
              strlen(log_files->default_suffix))
          {
            strcpy(full_name + strlen(full_name),
                   log_files->default_suffix);
          }
          else
            goto err;

          *(log_files->value)= strdup_root(&alloc, datadir);
        }

        if (log_options[i][log_files->length] == '=')
        {
          char full_name[MAX_LOG_OPTION_LENGTH];

          fn_format(full_name, log_options[i] +log_files->length + 1,
                    datadir, "", MY_UNPACK_FILENAME | MY_SAFE_PATH);

          if (!(*(log_files->value)=
                strdup_root(&alloc, full_name)))
            goto err;
        }

      }
    }
  }

  return 0;

err:
  return 1;

}


int Instance_options::get_pid_filename(char *result)
{
  const char *pid_file= mysqld_pid_file;
  char datadir[MAX_PATH_LEN];

  if (!(mysqld_datadir))
  {
    /* we might get an error here if we have wrong path to the mysqld binary */
    if (get_default_option(datadir, sizeof(datadir), "--datadir"))
      return 1;
  }
  else
    strxnmov(datadir, MAX_PATH_LEN - 1, strchr(mysqld_datadir, '=') + 1,
             "/", NullS);

  DBUG_ASSERT(mysqld_pid_file);
  pid_file= strchr(pid_file, '=') + 1;

  /* get the full path to the pidfile */
  my_load_path(result, pid_file, datadir);
  return 0;
}


int Instance_options::unlink_pidfile()
{
  return unlink(pid_file_with_path);
}


pid_t Instance_options::get_pid()
{
  FILE *pid_file_stream;

  /* get the pid */
  if ((pid_file_stream= my_fopen(pid_file_with_path,
                                O_RDONLY | O_BINARY, MYF(0))) != NULL)
  {
    pid_t pid;

    fscanf(pid_file_stream, "%i", &pid);
    my_fclose(pid_file_stream, MYF(0));
    return pid;
  }
  else
    return 0;
}


int Instance_options::complete_initialization(const char *default_path,
                                              int only_instance)
{
  const char *tmp;

  if (!(mysqld_path))
  {
    if (!(mysqld_path= strdup_root(&alloc, default_path)))
      goto err;
  }

  if (mysqld_port)
    mysqld_port_val= atoi(strchr(mysqld_port, '=') + 1);

  if (shutdown_delay)
    shutdown_delay_val= atoi(shutdown_delay);

  if (!(tmp= strdup_root(&alloc, "--no-defaults")))
    goto err;

  if (!(mysqld_pid_file))
  {
    char pidfilename[MAX_PATH_LEN];
    char hostname[MAX_PATH_LEN];

    /*
      If we created only one istance [mysqld], because no config. files were
      found, we would like to model mysqld pid file values.
    */
    if (!gethostname(hostname, sizeof(hostname) - 1))
      (only_instance == 0) ?
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", instance_name, "-",
               hostname, ".pid", NullS):
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", hostname,
               ".pid", NullS);

    else
      (only_instance == 0) ?
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", instance_name,
               ".pid", NullS):
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", "mysql",
               ".pid", NullS);

    add_option(pidfilename);
  }

  if (get_pid_filename(pid_file_with_path))
    goto err;

  /* we need to reserve space for the final zero + possible default options */
  if (!(argv= (char**) alloc_root(&alloc, (options_array.elements + 1
                       + MAX_NUMBER_OF_DEFAULT_OPTIONS) * sizeof(char*))))
  goto err;

  /* the path must be first in the argv */
  if (add_to_argv(mysqld_path))
    goto err;

  if (add_to_argv(tmp))
    goto err;

  memcpy((gptr) (argv + filled_default_options), options_array.buffer,
         options_array.elements*sizeof(char*));
  argv[filled_default_options + options_array.elements]= 0;

  fill_log_options();

  return 0;

err:
  return 1;
}


/*
  Assigns given value to the appropriate option from the class.

  SYNOPSYS
    add_option()
    option            string with the option prefixed by --

  DESCRIPTION

    The method is called from the option handling routine.

  RETURN
    0 - ok
    1 - error occured
*/

int Instance_options::add_option(const char* option)
{
  char *tmp;
  enum { SAVE_VALUE= 1, SAVE_WHOLE, SAVE_WHOLE_AND_ADD };
  struct selected_options_st
  {
    const char *name;
    uint length;
    const char **value;
    uint type;
  } options[]=
  {
    {"--socket=", 9, &mysqld_socket, SAVE_WHOLE_AND_ADD},
    {"--port=", 7, &mysqld_port, SAVE_WHOLE_AND_ADD},
    {"--datadir=", 10, &mysqld_datadir, SAVE_WHOLE_AND_ADD},
    {"--bind-address=", 15, &mysqld_bind_address, SAVE_WHOLE_AND_ADD},
    {"--pid-file=", 11, &mysqld_pid_file, SAVE_WHOLE_AND_ADD},
    {"--mysqld-path=", 14, &mysqld_path, SAVE_VALUE},
    {"--nonguarded", 9, &nonguarded, SAVE_WHOLE},
    {"--shutdown_delay", 9, &shutdown_delay, SAVE_VALUE},
    {NULL, 0, NULL, 0}
  };
  struct selected_options_st *selected_options;

  if (!(tmp= strdup_root(&alloc, option)))
    goto err;

   for (selected_options= options; selected_options->name; selected_options++)
   {
     if (!strncmp(tmp, selected_options->name, selected_options->length))
       switch(selected_options->type){
       case SAVE_WHOLE_AND_ADD:
         *(selected_options->value)= tmp;
         insert_dynamic(&options_array,(gptr) &tmp);
         return 0;
       case SAVE_VALUE:
         *(selected_options->value)= strchr(tmp, '=') + 1;
         return 0;
       case SAVE_WHOLE:
         *(selected_options->value)= tmp;
         return 0;
       defaut:
         break;
       }
   }

  /* if we haven't returned earlier we should just save the option */
  insert_dynamic(&options_array,(gptr) &tmp);

  return 0;

err:
  return 1;
}


int Instance_options::add_to_argv(const char* option)
{
  DBUG_ASSERT(filled_default_options < MAX_NUMBER_OF_DEFAULT_OPTIONS);

  if ((option))
    argv[filled_default_options++]= (char *) option;
  return 0;
}


/* function for debug purposes */
void Instance_options::print_argv()
{
  int i;
  printf("printing out an instance %s argv:\n", instance_name);
  for (i=0; argv[i] != NULL; i++)
  {
    printf("argv: %s\n", argv[i]);
  }
}


/*
  We execute this function to initialize some options.
  Return value: 0 - ok. 1 - unable to allocate memory.
*/

int Instance_options::init(const char *instance_name_arg)
{
  instance_name_len= strlen(instance_name_arg);

  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);

  if (my_init_dynamic_array(&options_array, sizeof(char *), 0, 32))
      goto err;

  if (!(instance_name= strmake_root(&alloc, (char *) instance_name_arg,
                                  instance_name_len)))
    goto err;

  return 0;

err:
  return 1;
}


Instance_options::~Instance_options()
{
  free_root(&alloc, MYF(0));
  delete_dynamic(&options_array);
}

