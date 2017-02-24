/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_CAPABILITIES_HANDLER_H_
#define _NGS_CAPABILITIES_HANDLER_H_

#include "ngs_common/smart_ptr.h"
#include <string>

#include "ngs_common/protocol_protobuf.h"


namespace ngs
{
  class Client_interface;


  class Capability_handler
  {
  public:
    virtual ~Capability_handler() { }

    virtual const std::string name() const = 0;

    virtual bool is_supported() const = 0;

    virtual void get(::Mysqlx::Datatypes::Any &any) = 0;
    virtual bool set(const ::Mysqlx::Datatypes::Any &any)  = 0;

    virtual void commit() = 0;
  };


  typedef ngs::shared_ptr<Capability_handler> Capability_handler_ptr;


} // namespace ngs


#endif
