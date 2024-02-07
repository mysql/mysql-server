/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_BINLOG_EVENT_CODECS_FACTORY_H
#define MYSQL_BINLOG_EVENT_CODECS_FACTORY_H

#include <memory>
#include "mysql/binlog/event/binary_log.h"
#include "mysql/binlog/event/codecs/base.h"

namespace mysql::binlog::event::codecs {

class Factory {
 public:
  static std::unique_ptr<Codec> build_codec(Log_event_type type);
};

}  // namespace mysql::binlog::event::codecs

#endif  // MYSQL_BINLOG_EVENT_CODECS_FACTORY_H
