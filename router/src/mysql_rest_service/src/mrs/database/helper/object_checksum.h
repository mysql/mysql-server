/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <map>
#include <memory>
#include <string>
#include "mrs/database/duality_view/select.h"
#include "mrs/database/entry/object.h"
#include "router/src/http/src/digest.h"

namespace mrs {
namespace database {

class IDigester {
 public:
  virtual ~IDigester() = default;
  virtual void update(std::string_view data) = 0;
  virtual std::string finalize() = 0;
};

class Sha256Digest : public IDigester {
 public:
  Sha256Digest();
  void update(std::string_view data) override;
  std::string finalize() override;

 private:
  std::string all;
  Digest digest_;
};

void digest_object(std::shared_ptr<entry::Object> object, std::string_view doc,
                   IDigester *digest);

// std::string compute_checksum(std::shared_ptr<entry::Object> object,
//                              std::string_view doc);

std::string post_process_json(
    std::shared_ptr<entry::Object> view, const dv::ObjectFieldFilter &filter,
    const std::map<std::string, std::string> &metadata, std::string_view doc,
    bool compute_checksum = true);

}  // namespace database
}  // namespace mrs
