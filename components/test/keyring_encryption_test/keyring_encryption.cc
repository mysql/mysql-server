/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

/* <openssl/applink.c> is included in client library */

#include <my_dbug.h> /* DEBUG macros */
#include <my_sys.h>  /* MY_INIT */
#include <scope_guard.h>

#include "components.h"
#include "options.h"

using components::AES_encryption_keyring_services;
using components::deinit_components_subsystem;
using components::init_components_subsystem;
using components::Keyring_component_load;
using components::Keyring_encryption_test;

using options::Options;
using options::process_options;

class Keyring_encryption_test_setup {
 public:
  explicit Keyring_encryption_test_setup(char *progname) {
    MY_INIT(progname);
    init_components_subsystem();
  }

  ~Keyring_encryption_test_setup() {
    deinit_components_subsystem();
    my_end(0);
  }
};

int main(int argc, char **argv) {
  /* Initialization */
  Keyring_encryption_test_setup keyring_encryption_test_setup(argv[0]);
  DBUG_TRACE;
  DBUG_PROCESS(argv[0]);
  constexpr int exit_status = EXIT_FAILURE;

  int exit_code;
  if (process_options(&argc, &argv, exit_code) == false) {
    std::cerr << "Error processing options" << std::endl;
    return exit_status;
  }

  {
    Keyring_component_load keyring_component_load(Options::s_keyring);

    if (!keyring_component_load.ok()) {
      std::cerr << "Error loading keyring component '" << Options::s_keyring
                << "'" << std::endl;
      return exit_status;
    }

    AES_encryption_keyring_services keyring_service(Options::s_keyring);
    if (!keyring_service.ok()) {
      std::cerr << "Error acquiring required services from component '"
                << Options::s_keyring << "'" << std::endl;
      return exit_status;
    }

    Keyring_encryption_test keyring_encryption_test(keyring_service);
    if (!keyring_encryption_test.ok()) {
      std::cerr << "Error initializing test driver" << std::endl;
      return exit_status;
    }

    if (!keyring_encryption_test.test_aes()) {
      std::cerr << "Failed AES tests" << std::endl;
      return exit_status;
    }
    std::cout << "Successfully tested AES functionality" << std::endl;
  }
  return 0;
}
