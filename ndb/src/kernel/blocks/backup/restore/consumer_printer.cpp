/* Copyright (C) 2003 MySQL AB

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

#include "consumer_printer.hpp"

bool
BackupPrinter::table(const TableS & tab)
{
  if (m_print || m_print_meta) 
  {
    m_ndbout << tab;
    ndbout_c("Successfully printed table: %s", tab.m_dictTable->getName());
  }
  return true;
}

void
BackupPrinter::tuple(const TupleS & tup)
{
  m_dataCount++;
  if (m_print || m_print_data)
    m_ndbout << tup << endl;  
}

void
BackupPrinter::logEntry(const LogEntry & logE)
{
  if (m_print || m_print_log)
    m_ndbout << logE << endl;
  m_logCount++;
}

void
BackupPrinter::endOfLogEntrys()
{
  if (m_print || m_print_log) 
  {
    ndbout << "Printed " << m_dataCount << " tuples and "
	   << m_logCount << " log entries" 
	   << " to stdout." << endl;
  }
}
