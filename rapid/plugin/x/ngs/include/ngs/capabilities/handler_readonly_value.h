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


#ifndef _NGS_CAPABILITIE_READONLY_H_
#define _NGS_CAPABILITIE_READONLY_H_


#include "ngs/capabilities/handler.h"
#include "ngs/mysqlx/setter_any.h"


namespace ngs
{


  class Capability_readonly_value : public Capability_handler
  {
  public:
    template<typename ValueType>
    Capability_readonly_value(const std::string &cap_name, const ValueType& value)
    : m_name(cap_name)
    {
      Setter_any::set_scalar(m_value, value);
    }

    virtual const std::string name() const { return m_name; }
    virtual bool is_supported() const { return true; }

    virtual void get(::Mysqlx::Datatypes::Any &any) {any.CopyFrom(m_value); }
    virtual bool set(const ::Mysqlx::Datatypes::Any &any) { return false; };

    virtual void commit() {}

  private:
    const std::string        m_name;
    ::Mysqlx::Datatypes::Any m_value;
  };


} // namespace ngs


#endif // _NGS_CAPABILITIE_READONLY_H_
