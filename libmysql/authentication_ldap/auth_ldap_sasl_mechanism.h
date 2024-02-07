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

#ifndef AUTH_LDAP_SASL_MECHANISM_H_
#define AUTH_LDAP_SASL_MECHANISM_H_

#include "my_config.h"

#ifdef HAVE_SASL_SASL_H
#include <sys/types.h>
#endif
#include <sasl/sasl.h>

#include <string>

#if defined(KERBEROS_LIB_CONFIGURED)
#include "auth_ldap_kerberos.h"
#endif

namespace auth_ldap_sasl_client {

const int SASL_ERROR_INVALID_METHOD = -2;

/**
 Base class representing SASL mechanism. The child classes are used to perform
 all mechanism specific SASL operations.
*/
class Sasl_mechanism {
 public:
  /** GSSAPI string */
  static const char SASL_GSSAPI[];
  /**  SCRAM-SHA-1 string */
  static const char SASL_SCRAM_SHA1[];
  /** SCRAM-SHA-256 string */
  static const char SASL_SCRAM_SHA256[];

  /**
   Destructor.
  */
  virtual ~Sasl_mechanism() = default;
  /**
   Preauthentication step, e.g. obtaining Kerberos ticket. Not needed by most
   methods, so the default implementation just returns success.

   @param user [in] user mname
   @param password [in] user password

   @return true -success
  */
  bool virtual preauthenticate([[maybe_unused]] const char *user,
                               [[maybe_unused]] const char *password) {
    return true;
  }
  /**
   Get LDAP host. Not needed by most methods, return nullptr by default.

   @return LDAP host URL or nullptr on failure
  */
  virtual const char *get_ldap_host() { return nullptr; }

  /**
   Get default user name. Called if no user name was provided as parameter to
   the client. Most methods don't provide default user name.

   @param name [out] default user name
   @return false -failure
  */
  virtual bool get_default_user([[maybe_unused]] std::string &name) {
    return false;
  }

  /**
   Get list of supported SASL callbacks.

   @return List of callbacks.
  */
  virtual const sasl_callback_t *get_callbacks() { return nullptr; }

  /**
   Gets constans string describing mechanism name.

   @return mechanism name
  */
  const char *get_mechanism_name() { return m_mechanism_name; }

  /**
   Check if the authentication method requires conclusion message from the
   server. Most authentication mechanisms don't require to be concluded by MySQL
   server, so the base class implementation always returns false.

   @return false
  */
  virtual bool require_conclude_by_server() { return false; }

  /**
   SASL mechanism factory function. Creates mechanism object based on mechanism
   name.

   @param mechanism_name [in] name of the mechanism
   @param mechanism [out] created mechanism object

   @retval true success
   @retval false failure
  */
  static bool create_sasl_mechanism(const char *mechanism_name,
                                    Sasl_mechanism *&mechanism);

 protected:
  /**
   Constructor. Made protected to avoid creating direct objects of this class.

   @param mechanism_name [in] name of the mechanism
  */
  Sasl_mechanism(const char *mechanism_name)
      : m_mechanism_name(mechanism_name) {}

 private:
  /** array of SASL callbacks */
  static const sasl_callback_t callbacks[];
  /** name of the mechanism */
  const char *m_mechanism_name;
};

#if defined(KERBEROS_LIB_CONFIGURED)
/**
 Class representing GSSAPI/Kerberos mechanism
*/
class Sasl_mechanism_kerberos : public Sasl_mechanism {
 public:
  /**
   Constructor.
  */
  Sasl_mechanism_kerberos() : Sasl_mechanism(SASL_GSSAPI) {}
  /**
   Destructor.
  */
  ~Sasl_mechanism_kerberos() override = default;
  /**
    Preauthentication step. Obtains Kerberos ticket.

    @param user [in] user mname
    @param password [in] user password

    @retval true success
    @retval false failure
   */
  bool preauthenticate(const char *user, const char *password) override;
  /**
   Get LDAP host.

   @return LDAP host URL or nullptr on failure
  */
  const char *get_ldap_host() override;

  /**
   Get default user name. Called if no user name was provided as parameter to
   the client. The name is the default principal.

   @param name [out] default user name

    @retval true success
    @retval false failure
  */
  bool get_default_user(std::string &name) override;
  /**
   Gets array of SASL callbacks supported by the mechanism.

   @return array of callbacks
  */
  const sasl_callback_t *get_callbacks() override { return callbacks; }
  /**
    GSSAPI authentication must be concluded by MySQL server.

    @return true
  */
  bool require_conclude_by_server() override { return true; }

 private:
  /** URL of the LDAP server */
  std::string m_ldap_server_host;
  /** Kerberos object used to perform Kerberos operations */
  Kerberos m_kerberos;
  /** Array of SASL callbacks supported by this mechanism */
  static const sasl_callback_t callbacks[];
};
#endif

#if defined(SCRAM_LIB_CONFIGURED)
/**
  Class representing SCRAM family of SASL mechanisms (currently SCRAM-SHA-1 and
  SCRAM-SHA-256).
*/
class Sasl_mechanism_scram : public Sasl_mechanism {
 public:
  /**
   Constructor.

   @param mechanism_name [in] mame of the mechanism
  */
  Sasl_mechanism_scram(const char *mechanism_name)
      : Sasl_mechanism(mechanism_name) {}
  /**
   Destructor.
  */
  ~Sasl_mechanism_scram() override = default;
  /**
   Gets array of SASL callbacks supported by the mechanism.

   @return array of callbacks
  */
  const sasl_callback_t *get_callbacks() override { return callbacks; }

 private:
  /** Array of SASL callbacks supported by this mechanism */
  static const sasl_callback_t callbacks[];
};
#endif
}  // namespace auth_ldap_sasl_client
#endif  // AUTH_LDAP_SASL_MECHANISM_H_
