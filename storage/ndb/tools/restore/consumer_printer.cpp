/*
   Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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

void
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
      return;
    m_ndbout << tup << g_ndbrecord_print_format.lines_terminated_by;  
  }
}

void
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
BackupPrinter::update_apply_status(const RestoreMetaData &metaData)
{
  if (m_print)
  {
  }
  return true;
}
