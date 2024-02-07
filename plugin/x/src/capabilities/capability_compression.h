/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_CAPABILITIES_CAPABILITY_COMPRESSION_H_
#define PLUGIN_X_SRC_CAPABILITIES_CAPABILITY_COMPRESSION_H_

#include <map>
#include <memory>
#include <string>

#include "plugin/x/src/capabilities/handler.h"
#include "plugin/x/src/capabilities/set_variable_adaptor.h"
#include "plugin/x/src/helper/optional_value.h"
#include "plugin/x/src/ngs/compression_types.h"
#include "plugin/x/src/variables/system_variables.h"

namespace xpl {
namespace iface {

class Client;

}  // namespace iface

class Capability_compression : public Capability_handler {
 public:
  explicit Capability_compression(iface::Client *client) : m_client(client) {}

  std::string name() const override { return "compression"; }
  bool is_settable() const override { return true; }
  bool is_gettable() const override { return true; }

  void commit() override;

 private:
  bool is_supported_impl() const override { return true; }
  void get_impl(::Mysqlx::Datatypes::Any *any) override;
  ngs::Error_code set_impl(const ::Mysqlx::Datatypes::Any &any) override;

  iface::Client *m_client;
  ngs::Compression_algorithm m_algorithm{ngs::Compression_algorithm::k_none};
  int64_t m_max_messages{-1};
  bool m_combine_messages{true};
  Optional_value<int64_t> m_level;

  const Set_variable_adaptor<ngs::Compression_algorithm> m_algorithms_variable{
      Plugin_system_variables::m_compression_algorithms,
      {ngs::Compression_algorithm::k_deflate, ngs::Compression_algorithm::k_lz4,
       ngs::Compression_algorithm::k_zstd}};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_CAPABILITIES_CAPABILITY_COMPRESSION_H_
