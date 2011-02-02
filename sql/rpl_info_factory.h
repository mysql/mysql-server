/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_INFO_FACTORY_H
#define RPL_INFO_FACTORY_H

#include "rpl_info.h"
#include "rpl_mi.h"
#include "rpl_rli.h"
#include "rpl_info_file.h"
#include "rpl_info_table.h"
#include "rpl_info_handler.h"

enum enum_mi_repository
{
  MI_REPOSITORY_FILE= 0,
  MI_REPOSITORY_TABLE= 1
};
extern ulong opt_mi_repository_id;

enum enum_rli_repository
{
  RLI_REPOSITORY_FILE= 0,
  RLI_REPOSITORY_TABLE= 1
};
extern ulong opt_rli_repository_id;

#define MI_FIELD_ID 0

#define MI_SCHEMA "mysql"
#define MI_TABLE  "slave_master_info"

#define RLI_FIELD_ID 0

#define RLI_SCHEMA "mysql"
#define RLI_TABLE  "slave_relay_log_info"

class Rpl_info_factory
{
  public:

  static bool create(uint mi_option, Master_info **mi,
                     uint rli_option, Relay_log_info **rli);
  static Master_info *create_mi(uint rli_option);
  static Relay_log_info *create_rli(uint rli_option, bool is_slave_recovery);
  static bool decide_repository(Rpl_info *info, Rpl_info_table **table,
                                Rpl_info_file **file, bool is_table,
                                const char **msg);
  static bool change_engine(Rpl_info_table *table, const char *engine,
                            const char **msg);
};
#endif
