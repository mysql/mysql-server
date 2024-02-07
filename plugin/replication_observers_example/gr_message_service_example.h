/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GR_MESSAGE_SERVICE_EXAMPLE_H
#define GR_MESSAGE_SERVICE_EXAMPLE_H

#include <mysql/udf_registration_types.h>

/**
  @class GR_message_service_send_example

  An example implementation of a module that uses GR send service.
 */

class GR_message_service_send_example {
 public:
  /**
   UDF that will be called to send data to Group Replication send service.

   @return false success, true on failure.
   */
  static char *udf(UDF_INIT *, UDF_ARGS *args, char *result,
                   unsigned long *length, unsigned char *, unsigned char *);

  /**
   UDF initialization procedure.

   @return false success, true on failure.
   */
  static bool udf_init(UDF_INIT *init_id, UDF_ARGS *args, char *message);

  /**
    Register send method to send service message from GR.

    @return false success, true on failure.
  */
  bool register_example();

  /**
    Unregister send method, will not allow send service message from GR.

    @return false success, true on failure.
  */
  bool unregister_example();
};

/**
  This function register examples that uses services recv and send of Group
  replication.

  @return false success, true on failure.
 */

bool gr_service_message_example_init();

/**
  This function unregister examples that uses services recv and send of Group
  replication.

  @return false success, true on failure.
 */

bool gr_service_message_example_deinit();

#endif /* GR_MESSAGE_SERVICE_EXAMPLE_H */
