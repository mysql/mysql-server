/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_AUTHENTICATION_REGISTRATION_H
#define MYSQL_AUTHENTICATION_REGISTRATION_H

#include <mysql/components/service.h>

/**
  @ingroup group_components_services_inventory

  A service to do registration of fido device.
*/
BEGIN_SERVICE_DEFINITION(mysql_authentication_registration)

/**
  This method performs initiate registration step.

  @param[out] outbuf       Buffer to hold challenge
  @param[in] outbuflen    Length of buffer

  @return
    @retval FALSE Succeeded.
    @retval TRUE  Failed.
*/
DECLARE_BOOL_METHOD(init, (unsigned char **outbuf, unsigned int outbuflen));

/**
  This method performs finish registration step.

  @param [in] buf         Buffer holding signed challenge
  @param [in] buflen      Length of signed challenge
  @param [in] challenge       Buffer to hold random challenge
  @param [in] challenge_length   Length of random challenge
  @param [out] challenge_response       Buffer to hold challenge response
  @param [out] challenge_response_length    Length of challenge response

  @return
    @retval FALSE Succeeded.
    @retval TRUE  Failed.
*/
DECLARE_BOOL_METHOD(finish, (unsigned char *buf, unsigned int buflen,
                             const unsigned char *challenge,
                             unsigned int challenge_length,
                             unsigned char *challenge_response,
                             unsigned int *challenge_response_length));

/**
  This method calculates length of challenge required for server to allocate
  buffer which needs to be passed to init() to extract challenge.

  @param [out] outbuflen       Buffer to hold length of challenge
*/
DECLARE_METHOD(void, get_challenge_length, (unsigned int *outbuflen));

END_SERVICE_DEFINITION(mysql_authentication_registration)

#endif /* MYSQL_AUTHENTICATION_REGISTRATION_H */