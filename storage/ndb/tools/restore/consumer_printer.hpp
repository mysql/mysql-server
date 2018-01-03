/*
   Copyright (c) 2004, 2016, Oracle and/or its affiliates. All rights reserved.

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
    m_print_sql_log = false;
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
  bool m_print_sql_log;
  bool m_print_data;
  bool m_print_meta;
  Uint32 m_logCount;
  Uint32 m_dataCount;  
};

#endif
