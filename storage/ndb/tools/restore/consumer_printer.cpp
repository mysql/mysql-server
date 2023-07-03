/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#include "consumer_printer.hpp"
extern FilteredNdbOut info;
extern bool ga_dont_ignore_systab_0;
extern NdbRecordPrintFormat g_ndbrecord_print_format;
extern const char *tab_path;

bool
BackupPrinter::table(const TableS & tab)
{
  if (m_print || m_print_meta) 
  {
    m_ndbout << tab;
    info.setLevel(254);
    info << "Successfully printed table: ", tab.m_dictTable->getName();
  }
  return true;
}

bool
BackupPrinter::tuple(const TupleS & tup, Uint32 fragId)
{
  m_dataCount++;
  if (m_print || m_print_data)
  {
    if (m_ndbout.m_out == info.m_out)
    {
      info.setLevel(254);
      info << tup.getTable()->getTableName() << "; ";
    }
    const TableS * table = tup.getTable();
    if ((!ga_dont_ignore_systab_0) &&  table->isSYSTAB_0())
      return true;
    m_ndbout << tup << g_ndbrecord_print_format.lines_terminated_by;  
  }
  return true;
}

bool
BackupPrinter::logEntry(const LogEntry & logE)
{
  if (m_print || m_print_log)
    m_ndbout << logE << endl;
  else if(m_print_sql_log)
  {
    logE.printSqlLog();
    ndbout << endl;
  }
  m_logCount++;
  return true;
}

void
BackupPrinter::endOfLogEntrys()
{
  if (m_print || m_print_log || m_print_sql_log)
  {
    info.setLevel(254);
    info << "Printed " << m_dataCount << " tuples and "
         << m_logCount << " log entries" 
         << " to stdout." << endl;
  }
}
bool
BackupPrinter::update_apply_status(const RestoreMetaData &metaData, bool snapshotstart)
{
  if (m_print)
  {
  }
  return true;
}

bool
BackupPrinter::delete_epoch_tuple()
{
  if (m_print)
  {
  }
  return true;
}
