/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CONSUMER_PRINTER_HPP
#define CONSUMER_PRINTER_HPP

#include "consumer.hpp"

class BackupPrinter : public BackupConsumer 
{
  NdbOut & m_ndbout;
public:
  BackupPrinter(NODE_GROUP_MAP *ng_map,
                uint ng_map_len,
                NdbOut & out = ndbout) : m_ndbout(out) 
  {
    m_nodegroup_map = ng_map;
    m_nodegroup_map_len= ng_map_len;
    m_print = false;
    m_print_log = false;
    m_print_data = false;
    m_print_meta = false;
    m_logCount = 0;
    m_dataCount = 0;  
  }
  
  virtual bool table(const TableS &);
#ifdef USE_MYSQL
  virtual bool table(const TableS &, MYSQL* mysqlp);
#endif
  virtual void tuple(const TupleS &, Uint32 fragId);
  virtual void logEntry(const LogEntry &);
  virtual void endOfTuples() {};
  virtual void endOfLogEntrys();
  virtual bool update_apply_status(const RestoreMetaData &metaData);
  bool m_print;
  bool m_print_log;
  bool m_print_data;
  bool m_print_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;  
};

#endif
