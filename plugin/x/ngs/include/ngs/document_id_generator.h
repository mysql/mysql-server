/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_DOCUMENT_ID_GENERATOR_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_DOCUMENT_ID_GENERATOR_H_

#include <string>
#include "plugin/x/ngs/include/ngs/interface/document_id_generator_interface.h"
#include "plugin/x/ngs/include/ngs/thread.h"

namespace ngs {

class Document_id_generator : public ngs::Document_id_generator_interface {
 public:
  Document_id_generator();
  Document_id_generator(const uint64_t timestamp, const uint64_t serial);

  std::string generate(const Variables &vars) override;

 private:
  uint64_t m_timestamp;
  uint64_t m_serial;
  Mutex m_generate_mutex;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_DOCUMENT_ID_GENERATOR_H_
