/*
   Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef ABSTRACT_PROGRAM_INCLUDED
#define ABSTRACT_PROGRAM_INCLUDED

#include <string>

#include "i_options_provider.h"
#include "composite_options_provider.h"
#include "debug_options.h"
#include "help_options.h"
#include "message_data.h"

namespace Mysql{
namespace Tools{
namespace Base{

/**
  Base class for all MySQL client tools.
 */
class Abstract_program : public Options::Composite_options_provider
{
public:
  virtual ~Abstract_program();

  /**
    Returns null-terminated string with name of current program.
   */
  const std::string get_name();

  /**
    Returns pointer to array of options for usage in handle_options.
    Array has empty sentinel at the end, as handle_options expects.
   */
  my_option* get_options_array();

  /**
    Does all initialization and exit work, calls execute().
   */
  void run(int argc, char **argv);

  /**
    Returns string describing current version of this program.
   */
  virtual std::string get_version()= 0;

  /**
    Returns year of first release of this program.
   */
  virtual int get_first_release_year()= 0;

  /**
    Returns string describing shortly current program
   */
  virtual std::string get_description()= 0;

  /**
    Handles general errors.
   */
  virtual void error(const Message_data& message)= 0;

  /**
    Runs main program code.
   */
  virtual int execute(std::vector<std::string> positional_options)= 0;

  /**
   Prints program invocation message.
  */
  virtual void short_usage()= 0;
  /**
   Return error code
  */
  virtual int get_error_code()= 0;

protected:
  Abstract_program();

private:
  /**
    Initializes program name.
   */
  void init_name(char *name_from_cmd_line);
  /**
    Gathers all options from option providers and save in array to be used in
    my_getopt functionality.
   */
  void aggregate_options();

  /**
    Compares option structures by long name. Keeps --help first.
   */
  static bool options_by_name_comparer(const my_option& a, const my_option& b);

  /*
    Redirects call to option_parsed of main Abstract_program instance.
    If we have anonymous functions or binding this should be removed.
  */
  static my_bool callback_option_parsed(int optid,
    const struct my_option *opt MY_ATTRIBUTE((unused)),
    char *argument);

  Options::Debug_options m_debug_options;
  Options::Help_options m_help_options;
  std::vector<my_option> m_options;
  char **m_defaults_argv;
  std::string m_name;

  friend class Abstract_connection_program;
};

}
}
}

#endif
