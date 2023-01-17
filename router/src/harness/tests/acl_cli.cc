/*
Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <bitset>
#include <cstdlib>
#include <iostream>

#include "mysql/harness/access_rights.h"
#include "mysql/harness/filesystem.h"

int main(int argc, char **argv) {
  (void)argc;

  std::string filename = argv[0];

  auto rights_res = mysql_harness::access_rights_get(filename);
  if (!rights_res) {
    auto ec = rights_res.error();

    std::cerr << "ERROR: " << ec << "\n";

    return EXIT_FAILURE;
  }

  std::cout << filename << "\n";

#ifdef _WIN32
  auto sec_desc_raw = std::move(rights_res.value());

  namespace acl = mysql_harness::win32::access_rights;

  acl::SecurityDescriptor sec_desc(sec_desc_raw.get());

  std::cout << "- desc: " << sec_desc.to_string() << "\n";

  auto optional_dacl_res = sec_desc.dacl();
  if (!optional_dacl_res) {
    auto ec = optional_dacl_res.error();

    std::cerr << "ERROR: " << ec << "\n";

    return EXIT_FAILURE;
  }

  auto optional_dacl = std::move(optional_dacl_res.value());

  if (!optional_dacl) {
    std::cout << "all access\n";

    return EXIT_SUCCESS;
  }

  for (const auto &ace : acl::Acl(optional_dacl.value())) {
    std::cout << "- type: " << (int)ace.type() << " (size: " << (int)ace.size()
              << ")\n";

    if (ace.type() == ACCESS_ALLOWED_ACE_TYPE) {
      acl::AccessAllowedAce access_ace(
          static_cast<ACCESS_ALLOWED_ACE *>(ace.data()));

      char Name[256]{};
      char Domain[256]{};
      DWORD name_sz = sizeof(Name);
      DWORD domain_sz = sizeof(Domain);
      SID_NAME_USE sid_type;

      LookupAccountSidA(NULL, access_ace.sid().native(), Name, &name_sz, Domain,
                        &domain_sz, &sid_type);

      std::cout << "  - sid: " << access_ace.sid().to_string() << " (" << Domain
                << "\\" << Name << ")\n";
      std::cout << "  - mask: " << std::bitset<32>(access_ace.mask()) << "\n";
    }
  }

#endif

  return EXIT_SUCCESS;
}
