/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlx.pb.h"
#include "mysqlx_connection.pb.h"
#include "mysqlx_error.h"
#include "mysqlx_notice.pb.h"
#include "mysqlx_resultset.pb.h"
#include "mysqlx_session.pb.h"
#include "mysqlx_sql.pb.h"

// export one symbol to trigger the .lib/.dll build on windows.
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
    void _mysqlrouter_mysqlxmessages_exports() {
  // use enough symbols to let the linker pull all in.
  { Mysqlx::Ok msg; }
  { Mysqlx::Connection::CapabilitiesSet msg; }
  { Mysqlx::Notice::Warning msg; }
  { Mysqlx::Session::Reset msg; }
  { Mysqlx::Sql::StmtExecute msg; }
  { Mysqlx::Resultset::Row msg; }
}
}
