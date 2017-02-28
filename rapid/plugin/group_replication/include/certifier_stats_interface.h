/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef CERTIFIER_STATS_INTERFACE_INCLUDE
#define CERTIFIER_STATS_INTERFACE_INCLUDE

#include <mysql/group_replication_priv.h>

class Certifier_stats
{
public:
  virtual ~Certifier_stats() {}
  virtual ulonglong get_positive_certified()= 0;
  virtual ulonglong get_negative_certified()= 0;
  virtual ulonglong get_certification_info_size()= 0;
  virtual int get_group_stable_transactions_set_string(char **buffer, size_t *length)= 0;
  virtual void get_last_conflict_free_transaction(std::string* value)= 0;
};

#endif /* CERTIFIER_STATS_INTERFACE_INCLUDE */
