/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This source file is compiled into the shared ndbclient
  in order to tell the linker which functions to export.

  NOTE! _ndbclient_exports() is not intended to be called
*/

#include "ndbapi/NdbApi.hpp"
#include "ndbapi/NdbInfo.hpp"
#include "portlib/NdbDir.hpp"
#include "util/ndb_opts.h"
#include "util/ndb_rand.h"
#include "util/random.h"

#ifdef NDB_WITH_NDBJTIE
extern "C" void _ndbjtie_exports(void);
#endif

extern "C"
#ifdef _MSC_VER
/*
  Make at least one symbol defined in ndbclient in order to force
  generation of export lib 
*/
__declspec(dllexport)
#endif
void
_ndbclient_exports(void)
{
  (void)ndb_init();
  Ndb_cluster_connection cluster_connection;
  NdbScanFilter scan_filter((NdbOperation*)0);
  NdbIndexStat index_stat;
  NdbInfo info(&cluster_connection, "");
  drop_instance(); // NdbPool
#ifdef NDB_WITH_NDBJTIE
  _ndbjtie_exports();
#endif
  ndb_std_print_version();
  myRandom48Init(0);
  ndb_rand();
  (void)NdbDir::chdir("");
  (void)BitmaskImpl::setField(0, 0, 0, 37, (Uint32*)0);
  ndb_end(0);
}
