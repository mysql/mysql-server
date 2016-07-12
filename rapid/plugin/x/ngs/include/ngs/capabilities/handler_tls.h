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


#ifndef _NGS_CAPABILITIE_TLS_H_
#define _NGS_CAPABILITIE_TLS_H_


#include "ngs/capabilities/handler.h"


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
