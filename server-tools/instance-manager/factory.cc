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

#include "factory.h"


Show_instances *Command_factory::new_Show_instances()
{
  return new Show_instances(&instance_map);
}


Flush_instances *Command_factory::new_Flush_instances()
{
  return new Flush_instances(&instance_map);
}


Show_instance_status *Command_factory::
                      new_Show_instance_status(const char *name, uint len)
{
  return new Show_instance_status(&instance_map, name, len);
}


Show_instance_options *Command_factory::
                       new_Show_instance_options(const char *name, uint len)
{
  return new Show_instance_options(&instance_map, name, len);
}


Start_instance *Command_factory::
                new_Start_instance(const char *name, uint len)
{
  return new Start_instance(&instance_map, name, len);
}


Stop_instance *Command_factory::new_Stop_instance(const char *name, uint len)
{
  return new Stop_instance(&instance_map, name, len);
}


Syntax_error *Command_factory::new_Syntax_error()
{
  return new Syntax_error();
}


Set_option *Command_factory::
            new_Set_option(const char* name, uint len,
                           const char *option_arg, uint option_len,
                           const char *option_value_arg, uint option_value_len)
{
  return new Set_option(&instance_map, name, len, option_arg,
                        option_len, option_value_arg, option_value_len);
}


Unset_option *Command_factory::
            new_Unset_option(const char* name, uint len,
                             const char *option_arg, uint option_len,
                             const char *option_value_arg, uint option_value_len)
{
  return new Unset_option(&instance_map, name, len, option_arg,
                          option_len, option_value_arg, option_value_len);
}


Show_instance_log *Command_factory::
                   new_Show_instance_log(const char *name, uint len,
                                         Log_type log_type_arg,
                                         const char *size, const char *offset)
{
  return new Show_instance_log(&instance_map, name, len,
                               log_type_arg, size, offset);
}


Show_instance_log_files *Command_factory::
                       new_Show_instance_log_files(const char *name, uint len)
{
  return new Show_instance_log_files(&instance_map, name, len);
}

