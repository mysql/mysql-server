/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_PROTOCOL_PLUGIN_CHAIN_FILE_OUTPUT_H_
#define PLUGIN_X_PROTOCOL_PLUGIN_CHAIN_FILE_OUTPUT_H_

#include <string>

#include "plugin/x/protocol/plugin/file_output.h"

class Chain_file_output : public File_output {
 public:
  explicit Chain_file_output(const std::string &name) : File_output(name) {}

  void write_header(Context *context) override {
    write_to_context(context, "#ifndef PLUGIN_X_GENERATED_XPROTOCOL_TAGS_H");
    write_to_context(context, "#define PLUGIN_X_GENERATED_XPROTOCOL_TAGS_H");
    write_to_context(context, "");
    write_to_context(context, "#include <set>");
    write_to_context(context, "#include <string>");
    write_to_context(context, "#include <cstring>");
    write_to_context(context, "");
    write_to_context(context, "");
    write_to_context(context, "class XProtocol_tags {");
    write_to_context(context, " public:");
    write_to_context(context,
                     "  bool is_chain_acceptable(const std::string &chain) {");
    write_to_context(
        context,
        "    auto iterator = m_allowed_tag_chains.lower_bound(chain);");
    write_to_context(context,
                     "    if (m_allowed_tag_chains.end() == iterator)");
    write_to_context(context, "      return false;");
    write_to_context(context, "    const auto to_match = (*iterator).c_str();");
    write_to_context(context,
                     "    return strstr(to_match, chain.c_str()) == to_match;");
    write_to_context(context, "  }");
    write_to_context(context, "");
    write_to_context(context, " private:");
    write_to_context(context, "  std::set<std::string> m_allowed_tag_chains;");
    write_to_context(context, " public:");
    write_to_context(context, "  XProtocol_tags() {");
    write_to_context(context, "    // Workaround for crash at FreeBSD 11");
    write_to_context(context,
                     "    // It crashes when using std::set<std::string> and "
                     "initialization list");
    write_to_context(context, "    const char *v[] = {");
  }

  void write_footer(Context *context) override {
    write_to_context(context, "    };");
    write_to_context(context, "");
    write_to_context(
        context,
        "    for(unsigned int i = 0; i < sizeof(v)/sizeof(v[0]); ++i)");
    write_to_context(context, "      m_allowed_tag_chains.insert(v[i]);");
    write_to_context(context, "  }");
    write_to_context(context, "};");
    write_to_context(context, "");
    write_to_context(context, "#endif  // PLUGIN_X_GENERATED_XPROTOCOL_TAGS_H");
  }

  void append_chain(Context *context, const std::string &chain) {
    write_to_context(context, "      \"", chain, "\",");
  }
};

#endif  // PLUGIN_X_PROTOCOL_PLUGIN_CHAIN_FILE_OUTPUT_H_
