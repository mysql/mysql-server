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
    {"--mysqld_path=", 14, &mysqld_path, SAVE_VALUE},
    {"--admin_user=", 13, &mysqld_user, SAVE_VALUE},
    {"--admin_password=", 17, &mysqld_password, SAVE_VALUE},
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

