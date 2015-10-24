/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef OBJECT_FILTER_INCLUDED
#define OBJECT_FILTER_INCLUDED

#include "base/composite_options_provider.h"
#include "base/abstract_program.h"
#include "abstract_data_object.h"
#include "nullable.h"
#include <string>
#include <vector>

namespace Mysql{
namespace Tools{
namespace Dump{

class Object_filter
  : public Mysql::Tools::Base::Options::Composite_options_provider
{
public:
  Object_filter(Mysql::Tools::Base::Abstract_program* program);

  void create_options();

  bool is_object_included_in_dump(Abstract_data_object* object);

  std::vector<std::pair<std::string, std::string> > m_databases_excluded;
  std::vector<std::pair<std::string, std::string> > m_databases_included;
  std::vector<std::pair<std::string, std::string> > m_tables_excluded;
  std::vector<std::pair<std::string, std::string> > m_tables_included;

private:
  bool is_object_included_by_lists(
    const std::string& schema_name,
    const std::string& object_name,
    std::vector<std::pair<std::string, std::string> >* include_list,
    std::vector<std::pair<std::string, std::string> >* exclude_list);
  void include_databases_callback(char*);
  void exclude_databases_callback(char*);
  void include_tables_callback(char*);
  void exclude_tables_callback(char*);
  void include_routines_callback(char*);
  void exclude_routines_callback(char*);
  void include_triggers_callback(char*);
  void exclude_triggers_callback(char*);
  void include_events_callback(char*);
  void exclude_events_callback(char*);
  void include_users_callback(char*);
  void exclude_users_callback(char*);

  bool is_user_included_by_lists(
    const std::string& object_name,
    std::vector<std::pair<std::string, std::string> >* include_list,
    std::vector<std::pair<std::string, std::string> >* exclude_list);

  void process_object_inclusion_string(
    std::vector<std::pair<std::string, std::string> >& list,
    bool allow_schema= true, bool is_user_object= false);

  std::vector<std::pair<std::string, std::string> > m_routines_excluded;
  std::vector<std::pair<std::string, std::string> > m_routines_included;
  std::vector<std::pair<std::string, std::string> > m_triggers_excluded;
  std::vector<std::pair<std::string, std::string> > m_triggers_included;
  std::vector<std::pair<std::string, std::string> > m_events_excluded;
  std::vector<std::pair<std::string, std::string> > m_events_included;
  std::vector<std::pair<std::string, std::string> > m_users_included;
  std::vector<std::pair<std::string, std::string> > m_users_excluded;


  bool m_dump_routines;
  bool m_dump_triggers;
  bool m_dump_events;
  bool m_dump_users;
  Mysql::Nullable<std::string> m_include_tmp_string;
  Mysql::Tools::Base::Abstract_program* m_program;
};

}
}
}

#endif
