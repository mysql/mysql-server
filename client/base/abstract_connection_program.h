/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ABSTRACT_CONNECTION_PROGRAM_INCLUDED
#define ABSTRACT_CONNECTION_PROGRAM_INCLUDED

#include "client_priv.h"
#include <string>

#include "i_options_provider.h"
#include "composite_options_provider.h"
#include "mysql_connection_options.h"
#include "i_connection_factory.h"
#include "abstract_program.h"

namespace Mysql{
namespace Tools{
namespace Base{

/**
  Base class for all programs that use connection to MySQL database server.
 */
class Abstract_connection_program
  : public Abstract_program, public I_connection_factory
{
public:
  /**
    Provides new connection to MySQL database server based on option values.
    Implementation of I_connection_factory interface.
   */
  virtual MYSQL* create_connection();

  /**
    Retrieves charset that will be used in new MySQL connections. Can be NULL
    if none was set explicitly.
   */
  CHARSET_INFO* get_current_charset() const;

  /**
    Sets charset that will be used in new MySQL connections.
   */
  void set_current_charset(CHARSET_INFO* charset);

protected:
  Abstract_connection_program();

private:
  Options::Mysql_connection_options m_connection_options;
};

}
}
}

#endif
