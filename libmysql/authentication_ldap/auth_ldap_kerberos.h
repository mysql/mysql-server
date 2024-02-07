/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef AUTH_LDAP_KERBEROS_H_
#define AUTH_LDAP_KERBEROS_H_

#include <assert.h>
#include <krb5/krb5.h>

#include <string>

#include "krb5_interface.h"

namespace auth_ldap_sasl_client {

/**
  Kerberos class is built around kerberos library.
  This class should/can be used for different part of code as standalone
  class.
  This class performs following operations:
  1. Authentication with kerberos server and store the credentials in cache.
  2. Get the default configured kerberos user in the OS, from default principal.

  Credentials:
  A ticket plus the secret session key necessary to use that ticket successfully
  in an authentication exchange.

  Principal:
  A named client or server entity that participates in a network communication,
  with one name that is considered canonical

  Credential cache:
  A credential cache (or ccache) holds Kerberos credentials while they
  remain valid and, generally, while the user's session lasts, so that
  authenticating to a service multiple times (e.g., connecting to a web or mail
  server more than once) doesn't require contacting the KDC every time.
*/
class Kerberos {
 public:
  /**
   Constructor.
  */
  Kerberos();
  /**
   Destructor.
  */
  ~Kerberos();
  /**
   Set user and password member variables.

   @param user [in] user name
   @param password [in]  password
  */
  void set_user_and_password(const char *user, const char *password) {
    assert(user);
    assert(password);
    m_user = user;
    m_password = password;
  }
  /**
    1. This function authenticates with kerberos server.
    2. If TGT destroy is false, this function stores the TGT in Kerberos cache
    for subsequent usage.
    3. If user credentials already exist in the cache, it doesn't attempt to get
    it again.

    @retval true Successfully able to obtain and store credentials.
    @retval false Failed to obtain and store credentials.
  */
  bool obtain_store_credentials();
  /**
    This function retrieves default principle from kerberos configuration and
    parses the user name from it. If user name has not been provided in the
    MySQL client, This method can be used to get the user name  and use for
    authentication.
    @retval true Successfully able to get user name.
    @retval false Failed to get user name.
  */
  bool get_default_principal_name(std::string &name);
  /**
   Check if the cache contains valid credentials

   @retval true valid credentials exist
   @retval false valid credentials not exist or an error ocurred
  */
  bool credentials_valid();
  /**
   Destroys existing credentials (remove them from the cache).
  */
  void destroy_credentials();
  /**
    This function gets LDAP host from krb5.conf file.
  */
  void get_ldap_host(std::string &host);

 private:
  /**
    This function creates kerberos context, initializes credentials cache and
    user principal.
    @retval true All the required kerberos objects like context,
    credentials cache and user principal are initialized correctly.
    @retval false Required kerberos objects failed to initialized.
  */
  bool initialize();
  /**
    This function frees kerberos context, credentials, credentials cache and
    user principal.
  */
  void cleanup();

  /** is the object initialized */
  bool m_initialized;
  /** user name */
  std::string m_user;
  /** user password */
  std::string m_password;
  /** LDAP host */
  std::string m_ldap_server_host;
  /** shall be the credentials destroyed on cleanup */
  bool m_destroy_tgt;
  /** Kerberos context */
  krb5_context m_context;
  /** Kerberos cache */
  krb5_ccache m_krb_credentials_cache;
  /** Kerberos credentials */
  krb5_creds m_credentials;
  /** were the credentials created by the object */
  bool m_credentials_created;
  /** interface to kerberos functions */
  Krb5_interface krb5;

  /**
   Log a Kerberos error, the message is taken from the Kerberos based on the
   error code.

   @param error_code [in] Kerberos error code
  */
  void log(int error_code);
  /**
  This method gets kerberos profile settings from krb5.conf file.

  @retval true success
  @retval false failure

  @details
  Sample krb5.conf file format may be like this:

  [realms]
  MEM.LOCAL = {
    kdc = VIKING67.MEM.LOCAL
    admin_server = VIKING67.MEM.LOCAL
    default_domain = MEM.LOCAL
    }

  # This portion is optional
  [appdefaults]
  mysql = {
    ldap_server_host = ldap_host.oracle.com
    ldap_destroy_tgt = true
  }

  kdc:
  The name or address of a host running a KDC for that realm.
  An optional port number, separated from the hostname by a colon, may
  be included. If the name or address contains colons (for example, if it is
  an IPv6 address), enclose it in square brackets to distinguish the colon
  from a port separator.

  For example:
  kdchost.example.com:88
  [2001:db8:3333:4444:5555:6666:7777:8888]:88

  Details from:
  https://web.mit.edu/kerberos/krb5-latest/doc/admin/conf_files/krb5_conf.html

  Host information is used by LDAP SASL client API while initialization.
  LDAP SASL API doesn't need port information and port is not used any where.
  */
  bool get_kerberos_config();
  /**
   Opens default Kerberos cache.

   @retval true success
   @retval false failure
   */
  bool open_default_cache();

  /**
   Closes default Kerberos cache.
  */
  void close_default_cache();
};
}  // namespace auth_ldap_sasl_client
#endif  // AUTH_LDAP_KERBEROS_H_
