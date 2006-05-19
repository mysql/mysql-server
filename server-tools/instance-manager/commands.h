#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H
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

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <hash.h>

#include "command.h"
#include "instance.h"
#include "parse.h"

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif


/*
  Print all instances of this instance manager.
  Grammar: SHOW ISTANCES
*/

class Show_instances : public Command
{
public:
  Show_instances(Instance_map *instance_map_arg): Command(instance_map_arg)
  {}

  int execute(st_net *net, ulong connection_id);

private:
  int write_header(st_net *net);
  int write_data(st_net *net);
};


/*
  Reread configuration file and refresh internal cache.
  Grammar: FLUSH INSTANCES
*/

class Flush_instances : public Command
{
public:
  Flush_instances(Instance_map *instance_map_arg): Command(instance_map_arg)
  {}

  int execute(st_net *net, ulong connection_id);
};


/*
  Abstract class for Instance-specific commands.
*/

class Abstract_instance_cmd : public Command
{
public:
  Abstract_instance_cmd(Instance_map *instance_map_arg,
                        const LEX_STRING *instance_name_arg);

public:
  virtual int execute(st_net *net, ulong connection_id);

protected:
  /* MT-NOTE: this operation is called under acquired Instance_map's lock. */
  virtual int execute_impl(st_net *net, Instance *instance) = 0;

  /*
    This operation is invoked on successful return of execute_impl() and is
    intended to send closing data.

    MT-NOTE: this operation is called under released Instance_map's lock.
  */
  virtual int send_ok_response(st_net *net, ulong connection_id) = 0;

protected:
  inline const LEX_STRING *get_instance_name() const
  {
    return instance_name.get_str();
  }

private:
  Instance_name instance_name;
};


/*
  Print status of an instance.
  Grammar: SHOW ISTANCE STATUS <instance_name>
*/

class Show_instance_status : public Abstract_instance_cmd
{
public:
  Show_instance_status(Instance_map *instance_map_arg,
                       const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);

private:
  int write_header(st_net *net);
  int write_data(st_net *net, Instance *instance);
};


/*
  Print options of chosen instance.
  Grammar: SHOW INSTANCE OPTIONS <instance_name>
*/

class Show_instance_options : public Abstract_instance_cmd
{
public:
  Show_instance_options(Instance_map *instance_map_arg,
                        const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);

private:
  int write_header(st_net *net);
  int write_data(st_net *net, Instance *instance);
};


/*
  Start an instance.
  Grammar: START INSTANCE <instance_name>
*/

class Start_instance : public Abstract_instance_cmd
{
public:
  Start_instance(Instance_map *instance_map_arg,
                 const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);
};


/*
  Stop an instance.
  Grammar: STOP INSTANCE <instance_name>
*/

class Stop_instance : public Abstract_instance_cmd
{
public:
  Stop_instance(Instance_map *instance_map_arg,
                const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);
};


/*
  Create an instance.
  Grammar: CREATE INSTANCE <instance_name> [<options>]
*/

class Create_instance : public Command
{
public:
  Create_instance(Instance_map *instance_map_arg,
                  const LEX_STRING *instance_name_arg);

public:
  bool init(const char **text);

protected:
  virtual int execute(st_net *net, ulong connection_id);

  inline const LEX_STRING *get_instance_name() const
  {
    return instance_name.get_str();
  }

private:
  bool parse_args(const char **text);

private:
  Instance_name instance_name;

  Named_value_arr options;
};


/*
  Drop an instance.
  Grammar: DROP INSTANCE <instance_name>

  Operation is permitted only if the instance is stopped. On successful
  completion the instance section is removed from config file and the instance
  is removed from the instance map.
*/

class Drop_instance : public Abstract_instance_cmd
{
public:
  Drop_instance(Instance_map *instance_map_arg,
                const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);
};


/*
  Print requested part of the log.
  Grammar:
    SHOW <instance_name> LOG {ERROR | SLOW | GENERAL} size[, offset_from_end]
*/

class Show_instance_log : public Abstract_instance_cmd
{
public:
  Show_instance_log(Instance_map *instance_map_arg,
                    const LEX_STRING *instance_name_arg,
                    Log_type log_type_arg, uint size_arg, uint offset_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);

private:
  int check_params(Instance *instance);
  int write_header(st_net *net);
  int write_data(st_net *net, Instance *instance);

private:
  Log_type log_type;
  uint size;
  uint offset;
};


/*
  Shows the list of the log files, used by an instance.
  Grammar: SHOW <instance_name> LOG FILES
*/

class Show_instance_log_files : public Abstract_instance_cmd
{
public:
  Show_instance_log_files(Instance_map *instance_map_arg,
                          const LEX_STRING *instance_name_arg);

protected:
  virtual int execute_impl(st_net *net, Instance *instance);
  virtual int send_ok_response(st_net *net, ulong connection_id);

private:
  int write_header(st_net *net);
  int write_data(st_net *net, Instance *instance);
};


/*
  Abstract class for option-management commands.
*/

class Instance_options_list;

class Abstract_option_cmd : public Command
{
public:
  ~Abstract_option_cmd();

public:
  bool add_option(const LEX_STRING *instance_name, Named_value *option);

public:
  bool init(const char **text);

  virtual int execute(st_net *net, ulong connection_id);

protected:
  Abstract_option_cmd(Instance_map *instance_map_arg);

  int correct_file(Instance *instance, Named_value *option, bool skip);

protected:
  virtual bool parse_args(const char **text) = 0;
  virtual int process_option(Instance *instance, Named_value *option) = 0;

private:
  Instance_options_list *
  get_instance_options_list(const LEX_STRING *instance_name);

  int execute_impl(st_net *net, ulong connection_id);

private:
  HASH instance_options_map;
  bool initialized;
};


/*
  Set an option for the instance.
  Grammar: SET instance_name.option[=option_value][, ...]
*/

class Set_option : public Abstract_option_cmd
{
public:
  Set_option(Instance_map *instance_map_arg);

protected:
  virtual bool parse_args(const char **text);
  virtual int process_option(Instance *instance, Named_value *option);
};


/*
  Remove option of the instance.
  Grammar: UNSET instance_name.option[, ...]
*/

class Unset_option: public Abstract_option_cmd
{
public:
  Unset_option(Instance_map *instance_map_arg);

protected:
  virtual bool parse_args(const char **text);
  virtual int process_option(Instance *instance, Named_value *option);
};


/*
  Syntax error command.

  This command is issued if parser reported a syntax error. We need it to
  distinguish between syntax error and internal parser error.  E.g. parsing
  failed because we hadn't had enought memory. In the latter case the parser
  just returns NULL.
*/

class Syntax_error : public Command
{
public:
  /* This is just to avoid compiler warning. */
  Syntax_error() :Command(NULL)
  {}

public:
  int execute(st_net *net, ulong connection_id);
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_COMMANDS_H */
