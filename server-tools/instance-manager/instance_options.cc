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
#include "buffer.h"
#include <my_sys.h>
#include <mysql.h>
#include <signal.h>
#include <m_string.h>


/* option_name should be prefixed with "--" */
int Instance_options::get_default_option(char *result, const char *option_name,
                                         size_t result_len)
{
  int position= 0;
  char verbose_option[]= " --no-defaults --verbose --help";
  Buffer cmd;

  cmd.append(position, mysqld_path, strlen(mysqld_path));
  position+=  strlen(mysqld_path);
  cmd.append(position, verbose_option, sizeof(verbose_option) - 1);
  position+= sizeof(verbose_option) - 1;
  cmd.append(position, "\0", 1);
  /* get the value from "mysqld --help --verbose" */
  if (parse_output_and_get_value(cmd.buffer, option_name + 2,
                                 result, result_len))
    return 1;

  return 0;
}


void Instance_options::get_pid_filename(char *result)
{
  const char *pid_file= mysqld_pid_file;
  char datadir[MAX_PATH_LEN];

  if (mysqld_datadir == NULL)
  {
    get_default_option(datadir, "--datadir", MAX_PATH_LEN);
  }
  else
    strxnmov(datadir, MAX_PATH_LEN - 1, strchr(mysqld_datadir, '=') + 1,
             "/", NullS);

  /* well, we should never get it */
  if (mysqld_pid_file != NULL)
    pid_file= strchr(pid_file, '=') + 1;
  else
    DBUG_ASSERT(0);

  /* get the full path to the pidfile */
  my_load_path(result, pid_file, datadir);

}


int Instance_options::unlink_pidfile()
{
  char pid_file_path[MAX_PATH_LEN];

  /*
    This works as we know that pid_file_path is of
    MAX_PATH_LEN == FN_REFLEN length
  */
  get_pid_filename((char *)&pid_file_path);

  return unlink(pid_file_path);
}


pid_t Instance_options::get_pid()
{
  char pid_file_path[MAX_PATH_LEN];

  /*
    This works as we know that pid_file_path is of
    MAX_PATH_LEN == FN_REFLEN length
  */
  get_pid_filename((char *)&pid_file_path);

  /* get the pid */
  if (FILE *pid_file_stream= my_fopen(pid_file_path,
                                      O_RDONLY | O_BINARY, MYF(0)))
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
                                              const char *default_user,
                                              const char *default_password)
{
  const char *tmp;

  if (mysqld_path == NULL)
  {
    if (!(mysqld_path= strdup_root(&alloc, default_path)))
      goto err;
  }

  if (!(tmp= strdup_root(&alloc, "--no-defaults")))
    goto err;

  if (mysqld_pid_file == NULL)
  {
    char pidfilename[MAX_PATH_LEN];
    char hostname[MAX_PATH_LEN];
    if (!gethostname(hostname, sizeof(hostname) - 1))
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", hostname, "-",
               instance_name, ".pid", NullS);
    else
      strxnmov(pidfilename, MAX_PATH_LEN - 1, "--pid-file=", instance_name,
               ".pid", NullS);

    add_option(pidfilename);
  }

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
    {"--guarded", 9, &is_guarded, SAVE_WHOLE},
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

  return 0;

err:
  return 1;
}


int Instance_options::add_to_argv(const char* option)
{
  DBUG_ASSERT(filled_default_options < MAX_NUMBER_OF_DEFAULT_OPTIONS);

  if (option != NULL)
    argv[filled_default_options++]= (char *) option;
  return 0;
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

