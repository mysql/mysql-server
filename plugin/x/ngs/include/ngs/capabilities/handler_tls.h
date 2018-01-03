/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#ifndef _NGS_CAPABILITIE_TLS_H_
#define _NGS_CAPABILITIE_TLS_H_


#include "plugin/x/ngs/include/ngs/capabilities/handler.h"


namespace ngs
{

  class Client_interface;


  class Capability_tls : public Capability_handler
  {
  public:
    Capability_tls(Client_interface& client) : m_client(client), tls_should_be_enabled(false)
    {
    }

    virtual const std::string name() const { return "tls"; }
    virtual bool is_supported() const;

    virtual void get(::Mysqlx::Datatypes::Any &any);
    virtual bool set(const ::Mysqlx::Datatypes::Any &any);

    virtual void commit();

  private:
    Client_interface& m_client;
    bool    tls_should_be_enabled;
  };


} // namespace ngs


#endif // _NGS_CAPABILITIE_TLS_H_
