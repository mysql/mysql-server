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
#include <my_sys.h>
#include <mysql.h>
#include <signal.h>
#include <m_string.h>

int Instance_options::complete_initialization(const char *default_path,
                                              const char *default_user,
                                              const char *default_password)
{
  /* we need to reserve space for the final zero + possible default options */
  if (!(argv= (char**) alloc_root(&alloc, (options_array.elements + 1
                       + MAX_NUMBER_OF_DEFAULT_OPTIONS) * sizeof(char*))))
  goto err;


  if (mysqld_path == NULL)
  {
    if (!(mysqld_path= strdup_root(&alloc, default_path)))
      goto err;
  }

  /* this option must be first in the argv */
  if (add_to_argv(mysqld_path))
    goto err;

  /* the following options are not for argv */
  if (mysqld_user == NULL)
  {
    if (!(mysqld_user= strdup_root(&alloc, default_user)))
      goto err;
  }

  if (mysqld_password == NULL)
  {
    if (!(mysqld_password= strdup_root(&alloc, default_password)))
      goto err;
  }

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
  uint elements_count=0;
  static const char socket[]= "--socket=";
  static const char port[]= "--port=";
  static const char datadir[]= "--datadir=";
  static const char language[]= "--bind-address=";
  static const char pid_file[]= "--pid-file=";
  static const char path[]= "--mysqld_path=";
  static const char user[]= "--admin_user=";
  static const char password[]= "--admin_password=";
  static const char guarded[]= "--guarded";
  char *tmp;

  if (!(tmp= strdup_root(&alloc, option)))
    goto err;

 /* To get rid the final zero in a string we subtract 1 from sizeof value */
  if (strncmp(tmp, socket, sizeof socket - 1) == 0)
  {
    mysqld_socket= tmp;
    goto add_options;
  }

  if (strncmp(tmp, port, sizeof port - 1) == 0)
  {
    mysqld_port= tmp;
    goto add_options;
  }

  if (strncmp(tmp, datadir, sizeof datadir - 1) == 0)
  {
    mysqld_datadir= tmp;
    goto add_options;
  }

  if (strncmp(tmp, language, sizeof language - 1) == 0)
  {
    mysqld_bind_address= tmp;
    goto add_options;
  }

  if (strncmp(tmp, pid_file, sizeof pid_file - 1) == 0)
  {
    mysqld_pid_file= tmp;
    goto add_options;
  }

  /*
    We don't need a prefix in the next three optios.
    We also don't need to add them to argv array =>
    return instead of goto.
  */

  if (strncmp(tmp, path, sizeof path - 1) == 0)
  {
    mysqld_path= strchr(tmp, '=') + 1;
    return 0;
  }

  if (strncmp(tmp, user, sizeof user - 1) == 0)
  {
    mysqld_user= strchr(tmp, '=') + 1;
    return 0;
  }

  if (strncmp(tmp, password, sizeof password - 1) == 0)
  {
    mysqld_password= strchr(tmp, '=') + 1;
    return 0;
  }

  if (strncmp(tmp, guarded, sizeof guarded - 1) == 0)
  {
    is_guarded= tmp;
    return 0;
  }

add_options:
  insert_dynamic(&options_array,(gptr) &tmp);
  return 0;

err:
  return 1;
}

int Instance_options::add_to_argv(const char* option)
{
  DBUG_ASSERT(filled_default_options < (MAX_NUMBER_OF_DEFAULT_OPTIONS + 1));

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

  my_init_dynamic_array(&options_array, sizeof(char *), 0, 32);

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

