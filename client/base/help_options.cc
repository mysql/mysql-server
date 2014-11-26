/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "client_priv.h"
#include "my_default.h"
#include <sstream>
#include "help_options.h"
#include "abstract_program.h"

#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

using namespace Mysql::Tools::Base::Options;
using Mysql::Tools::Base::Abstract_program;

extern const char *load_default_groups[];

Help_options::Help_options(Abstract_program *program)
  : m_program(program)
{}

void Help_options::create_options()
{
  this->create_new_option("help", "Display this help message and exit.")
    ->set_short_character('?')
    ->add_callback(new Instance_callback<void, char*, Help_options>(
    this, &Help_options::help_callback));
}

void Help_options::help_callback(char* argument __attribute__((unused)))
{
  this->print_usage();
  exit(0);
}

void Mysql::Tools::Base::Options::Help_options::print_usage()
{
  std::string first_year_str=
    (static_cast<std::ostringstream*>(&(
      std::ostringstream() << this->m_program->get_first_release_year()))
    ->str());
  string copyright;

  if (first_year_str == COPYRIGHT_NOTICE_CURRENT_YEAR)
  {
    copyright= ORACLE_WELCOME_COPYRIGHT_NOTICE(COPYRIGHT_NOTICE_CURRENT_YEAR);
  }
  else
  {
#define FIRST_YEAR_CONSTANT "$first_year$"
    string first_year_constant_str= FIRST_YEAR_CONSTANT;

    copyright= ORACLE_WELCOME_COPYRIGHT_NOTICE(FIRST_YEAR_CONSTANT);
    copyright= copyright.replace(copyright.find(first_year_constant_str),
      first_year_constant_str.length(), first_year_str);
  }

  printf("%s  Ver %s Distrib %s, for %s (%s)\n%s\n%s\n",
    this->m_program->get_name().c_str(),
    this->m_program->get_version().c_str(),
    MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE,
    copyright.c_str(),
    this->m_program->get_description().c_str());
  my_print_help(this->m_program->get_options_array());
  print_defaults("my", load_default_groups);
  my_print_variables(this->m_program->get_options_array());
}
