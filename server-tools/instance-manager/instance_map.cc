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

#include "instance_map.h"

#include <my_global.h>
#include <m_ctype.h>
#include <mysql_com.h>

#include "buffer.h"
#include "guardian.h"
#include "instance.h"
#include "log.h"
#include "manager.h"
#include "mysqld_error.h"
#include "mysql_manager_error.h"
#include "options.h"
#include "priv.h"

/*
  Note:  As we are going to suppost different types of connections,
  we shouldn't have connection-specific functions. To avoid it we could
  put such functions to the Command-derived class instead.
  The command could be easily constructed for a specific connection if
  we would provide a special factory for each connection.
*/

C_MODE_START

/* Procedure needed for HASH initialization */

static byte* get_instance_key(const byte* u, uint* len,
                          my_bool __attribute__((unused)) t)
{
  const Instance *instance= (const Instance *) u;
  *len= instance->options.instance_name.length;
  return (byte *) instance->options.instance_name.str;
}

static void delete_instance(void *u)
{
  Instance *instance= (Instance *) u;
  delete instance;
}

/*
  The option handler to pass to the process_default_option_files finction.

  SYNOPSYS
    process_option()
    ctx               Handler context. Here it is an instance_map structure.
    group_name        The name of the group the option belongs to.
    option            The very option to be processed. It is already
                      prepared to be used in argv (has -- prefix)

  DESCRIPTION

    This handler checks whether a group is an instance group and adds
    an option to the appropriate instance class. If this is the first
    occurence of an instance name, we'll also create the instance
    with such name and add it to the instance map.

  RETURN
    0 - ok
    1 - error occured
*/

static int process_option(void *ctx, const char *group, const char *option)
{
  Instance_map *map= (Instance_map*) ctx;
  LEX_STRING group_str;

  group_str.str= (char *) group;
  group_str.length= strlen(group);

  return map->process_one_option(&group_str, option);
}

C_MODE_END


/*
   Parse option string.

  SYNOPSIS
    parse_option()
      option_str        [IN] option string (e.g. "--name=value")
      option_name_buf   [OUT] parsed name of the option.
                        Must be of (MAX_OPTION_LEN + 1) size.
      option_value_buf  [OUT] parsed value of the option.
                        Must be of (MAX_OPTION_LEN + 1) size.

  DESCRIPTION
    This is an auxiliary function and should not be used externally. It is
    intended to parse whole option string into option name and option value.
*/

static void parse_option(const char *option_str,
                         char *option_name_buf,
                         char *option_value_buf)
{
  const char *eq_pos;
  const char *ptr= option_str;

  while (*ptr == '-')
    ++ptr;

  strmake(option_name_buf, ptr, MAX_OPTION_LEN + 1);

  eq_pos= strchr(ptr, '=');
  if (eq_pos)
  {
    option_name_buf[eq_pos - ptr]= 0;
    strmake(option_value_buf, eq_pos + 1, MAX_OPTION_LEN + 1);
  }
  else
  {
    option_value_buf[0]= 0;
  }
}


/*
  Process one option from the configuration file.

  SYNOPSIS
    Instance_map::process_one_option()
      group         group name
      option        option string (e.g. "--name=value")

  DESCRIPTION
    This is an auxiliary function and should not be used externally.
    It is used only by flush_instances(), which pass it to
    process_option(). The caller ensures proper locking
    of the instance map object.
*/

int Instance_map::process_one_option(const LEX_STRING *group,
                                     const char *option)
{
  Instance *instance= NULL;

  if (!Instance::is_name_valid(group))
  {
    /*
      Current section name is not a valid instance name.
      We should skip it w/o error.
    */
    return 0;
  }

  if (!(instance= (Instance *) hash_search(&hash, (byte *) group->str,
                                           group->length)))
  {
    if (!(instance= new Instance(thread_registry)))
      return 1;

    if (instance->init(group) || add_instance(instance))
    {
      delete instance;
      return 1;
    }

    if (instance->is_mysqld_compatible())
      log_info("Warning: instance name '%s' is mysqld-compatible.",
               (const char *) group->str);

    log_info("mysqld instance '%s' has been added successfully.",
             (const char *) group->str);
  }

  if (option)
  {
    char option_name[MAX_OPTION_LEN + 1];
    char option_value[MAX_OPTION_LEN + 1];

    parse_option(option, option_name, option_value);

    if (instance->is_mysqld_compatible() &&
        Instance_options::is_option_im_specific(option_name))
    {
      log_info("Warning: configuration of mysqld-compatible instance '%s' "
               "contains IM-specific option '%s'. "
               "This breaks backward compatibility for the configuration file.",
               (const char *) group->str,
               (const char *) option_name);
    }

    Named_value option(option_name, option_value);

    if (instance->options.set_option(&option))
      return 1;   /* the instance'll be deleted when we destroy the map */
  }

  return 0;
}


Instance_map::Instance_map(const char *default_mysqld_path_arg,
                           Thread_registry &thread_registry_arg):
  mysqld_path(default_mysqld_path_arg),
  thread_registry(thread_registry_arg)
{
  pthread_mutex_init(&LOCK_instance_map, 0);
}


int Instance_map::init()
{
  return hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
                   get_instance_key, delete_instance, 0);
}

Instance_map::~Instance_map()
{
  pthread_mutex_lock(&LOCK_instance_map);
  hash_free(&hash);
  pthread_mutex_unlock(&LOCK_instance_map);
  pthread_mutex_destroy(&LOCK_instance_map);
}


void Instance_map::lock()
{
  pthread_mutex_lock(&LOCK_instance_map);
}


void Instance_map::unlock()
{
  pthread_mutex_unlock(&LOCK_instance_map);
}

/*
  Re-read instance configuration file.

  SYNOPSIS
    Instance_map::flush_instances()

  DESCRIPTION
    This function will:
     - clear the current list of instances. This removes both
       running and stopped instances.
     - load a new instance configuration from the file.
     - pass on the new map to the guardian thread: it will start
       all instances that are marked `guarded' and not yet started.
    Note, as the check whether an instance is started is currently
    very simple (returns TRUE if there is a MySQL server running
    at the given port), this function has some peculiar
    side-effects:
     * if the port number of a running instance was changed, the
       old instance is forgotten, even if it was running. The new
       instance will be started at the new port.
     * if the configuration was changed in a way that two
       instances swapped their port numbers, the guardian thread
       will not notice that and simply report that both instances
       are configured successfully and running.
    In order to avoid such side effects one should never call
    FLUSH INSTANCES without prior stop of all running instances.

  NOTE: The operation should be invoked with the following locks acquired:
    - Guardian_thread;
    - Instance_map;
*/

int Instance_map::flush_instances()
{
  int rc;

  /*
    Guardian thread relies on the instance map repository for guarding
    instances. This is why refreshing instance map, we need (1) to stop
    guardian (2) reload the instance map (3) reinitialize the guardian
    with new instances.
  */
  hash_free(&hash);
  hash_init(&hash, default_charset_info, START_HASH_SIZE, 0, 0,
            get_instance_key, delete_instance, 0);

  rc= load();
  /* don't init guardian if we failed to load instances */
  if (!rc)
    guardian->init(); // TODO: check error status.
  return rc;
}


bool Instance_map::is_there_active_instance()
{
  Instance *instance;
  Iterator iterator(this);

  while ((instance= iterator.next()))
  {
    if (guardian->find_instance_node(instance) != NULL ||
        instance->is_running())
    {
      return TRUE;
    }
  }

  return FALSE;
}


int Instance_map::add_instance(Instance *instance)
{
  return my_hash_insert(&hash, (byte *) instance);
}


int Instance_map::remove_instance(Instance *instance)
{
  return hash_delete(&hash, (byte *) instance);
}


int Instance_map::create_instance(const LEX_STRING *instance_name,
                                  const Named_value_arr *options)
{
  Instance *instance= new Instance(thread_registry);

  if (!instance)
  {
    log_error("Error: can not initialize (name: '%s').",
              (const char *) instance_name->str);
    return ER_OUT_OF_RESOURCES;
  }

  if (instance->init(instance_name))
  {
    log_error("Error: can not initialize (name: '%s').",
              (const char *) instance_name->str);
    delete instance;
    return ER_OUT_OF_RESOURCES;
  }

  for (int i= 0; options && i < options->get_size(); ++i)
  {
    Named_value option= options->get_element(i);

    if (instance->is_mysqld_compatible() &&
        Instance_options::is_option_im_specific(option.get_name()))
    {
      log_error("Error: IM-option (%s) can not be used "
                "in configuration of mysqld-compatible instance (%s).",
                (const char *) option.get_name(),
                (const char *) instance_name->str);
      delete instance;
      return ER_INCOMPATIBLE_OPTION;
    }

    instance->options.set_option(&option);
  }

  if (instance->is_mysqld_compatible())
    log_info("Warning: instance name '%s' is mysqld-compatible.",
             (const char *) instance_name->str);

  if (instance->complete_initialization(this, mysqld_path))
  {
    log_error("Error: can not complete initialization of instance (name: '%s').",
              (const char *) instance_name->str);
    delete instance;
    return ER_OUT_OF_RESOURCES;
    /* TODO: return more appropriate error code in this case. */
  }

  if (add_instance(instance))
  {
    log_error("Error: can not register instance (name: '%s').",
              (const char *) instance_name->str);
    delete instance;
    return ER_OUT_OF_RESOURCES;
  }

  return 0;
}


Instance * Instance_map::find(const LEX_STRING *name)
{
  return (Instance *) hash_search(&hash, (byte *) name->str, name->length);
}


bool Instance_map::complete_initialization()
{
  bool mysqld_found;

  /* Complete initialization of all registered instances. */

  for (uint i= 0; i < hash.records; ++i)
  {
    Instance *instance= (Instance *) hash_element(&hash, i);

    if (instance->complete_initialization(this, mysqld_path))
      return TRUE;
  }

  /* That's all if we are runnning in an ordinary mode. */

  if (!Options::Main::mysqld_safe_compatible)
    return FALSE;

  /* In mysqld-compatible mode we must ensure that there 'mysqld' instance. */

  mysqld_found= find(&Instance::DFLT_INSTANCE_NAME) != NULL;

  if (mysqld_found)
    return FALSE;

  if (create_instance(&Instance::DFLT_INSTANCE_NAME, NULL))
  {
    log_error("Error: could not create default instance.");
    return TRUE;
  }

  switch (create_instance_in_file(&Instance::DFLT_INSTANCE_NAME, NULL))
  {
  case 0:
  case ER_CONF_FILE_DOES_NOT_EXIST:
    /*
      Continue if the instance has been added to the config file
      successfully, or the config file just does not exist.
    */
    break;

  default:
    log_error("Error: could not add default instance to the config file.");

    Instance *instance= find(&Instance::DFLT_INSTANCE_NAME);

    if (instance)
      remove_instance(instance); /* instance is deleted here. */

    return TRUE;
  }

  return FALSE;
}


/* load options from config files and create appropriate instance structures */

int Instance_map::load()
{
  int argc= 1;
  /* this is a dummy variable for search_option_files() */
  uint args_used= 0;
  const char *argv_options[3];
  char **argv= (char **) &argv_options;
  char defaults_file_arg[FN_REFLEN];

  /* the name of the program may be orbitrary here in fact */
  argv_options[0]= "mysqlmanager";

  /*
    If the option file was forced by the user when starting
    the IM with --defaults-file=xxxx, make sure it is also
    passed as --defaults-file, not only as Options::config_file.
    This is important for option files given with relative path:
    e.g. --defaults-file=my.cnf.
    Otherwise my_search_option_files will treat "my.cnf" as a group
    name and start looking for files named "my.cnf.cnf" in all
    default dirs. Which is not what we want.
  */
  if (Options::Main::is_forced_default_file)
  {
    snprintf(defaults_file_arg, FN_REFLEN, "--defaults-file=%s",
             Options::Main::config_file);

    argv_options[1]= defaults_file_arg;
    argv_options[2]= '\0';

    argc= 2;
  }
  else
    argv_options[1]= '\0';

  /*
    If the routine failed, we'll simply fallback to defaults in
    complete_initialization().
  */
  if (my_search_option_files(Options::Main::config_file, &argc,
                             (char ***) &argv, &args_used,
                             process_option, (void*) this))
    log_info("Falling back to compiled-in defaults");

  return complete_initialization();
}


/*--- Implementaton of the Instance map iterator class  ---*/


void Instance_map::Iterator::go_to_first()
{
  current_instance=0;
}


Instance *Instance_map::Iterator::next()
{
  if (current_instance < instance_map->hash.records)
    return (Instance *) hash_element(&instance_map->hash, current_instance++);

  return NULL;
}


const char *Instance_map::get_instance_state_name(Instance *instance)
{
  LIST *instance_node;

  if (!instance->is_configured())
    return "misconfigured";

  if ((instance_node= guardian->find_instance_node(instance)) != NULL)
  {
    /* The instance is managed by Guardian: we can report precise state. */

    return Guardian_thread::get_instance_state_name(
      guardian->get_instance_state(instance_node));
  }

  /* The instance is not managed by Guardian: we can report status only.  */

  return instance->is_running() ? "online" : "offline";
}


/*
  Create a new configuration section for mysqld-instance in the config file.

  SYNOPSYS
    create_instance_in_file()
    instance_name       mysqld-instance name
    options             options for the new mysqld-instance

  RETURN
    0     On success
    ER_CONF_FILE_DOES_NOT_EXIST     If config file does not exist
    ER_ACCESS_OPTION_FILE           If config file is not writable or some I/O
                                    error ocurred during writing configuration
*/

int create_instance_in_file(const LEX_STRING *instance_name,
                            const Named_value_arr *options)
{
  File cnf_file;

  if (my_access(Options::Main::config_file, W_OK))
  {
    log_error("Error: configuration file (%s) does not exist.",
              (const char *) Options::Main::config_file);
    return ER_CONF_FILE_DOES_NOT_EXIST;
  }

  cnf_file= my_open(Options::Main::config_file, O_WRONLY | O_APPEND, MYF(0));

  if (cnf_file <= 0)
  {
    log_error("Error: can not open configuration file (%s): %s.",
              (const char *) Options::Main::config_file,
              (const char *) strerror(errno));
    return ER_ACCESS_OPTION_FILE;
  }

  if (my_write(cnf_file, (byte*)NEWLINE, NEWLINE_LEN, MYF(MY_NABP)) ||
      my_write(cnf_file, (byte*)"[", 1, MYF(MY_NABP)) ||
      my_write(cnf_file, (byte*)instance_name->str, instance_name->length,
               MYF(MY_NABP)) ||
      my_write(cnf_file, (byte*)"]", 1,   MYF(MY_NABP)) ||
      my_write(cnf_file, (byte*)NEWLINE, NEWLINE_LEN, MYF(MY_NABP)))
  {
    log_error("Error: can not write to configuration file (%s): %s.",
              (const char *) Options::Main::config_file,
              (const char *) strerror(errno));
    my_close(cnf_file, MYF(0));
    return ER_ACCESS_OPTION_FILE;
  }

  for (int i= 0; options && i < options->get_size(); ++i)
  {
    char option_str[MAX_OPTION_STR_LEN];
    char *ptr;
    int option_str_len;
    Named_value option= options->get_element(i);

    ptr= strxnmov(option_str, MAX_OPTION_LEN + 1, option.get_name(), NullS);

    if (option.get_value()[0])
      ptr= strxnmov(ptr, MAX_OPTION_LEN + 2, "=", option.get_value(), NullS);

    option_str_len= ptr - option_str;

    if (my_write(cnf_file, (byte*)option_str, option_str_len, MYF(MY_NABP)) ||
        my_write(cnf_file, (byte*)NEWLINE, NEWLINE_LEN, MYF(MY_NABP)))
    {
      log_error("Error: can not write to configuration file (%s): %s.",
                (const char *) Options::Main::config_file,
                (const char *) strerror(errno));
      my_close(cnf_file, MYF(0));
      return ER_ACCESS_OPTION_FILE;
    }
  }

  my_close(cnf_file, MYF(0));

  return 0;
}
