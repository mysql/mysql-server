/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef RPL_CHANNEL_CREDENTIALS_INCLUDE
#define RPL_CHANNEL_CREDENTIALS_INCLUDE

#include <map>
#include <string>

typedef std::pair<bool, std::string> String_set;

class Rpl_channel_credentials {
  struct Channel_cred_param {
    String_set username;
    String_set password;
    String_set plugin_auth;

    Channel_cred_param(char *username_arg, char *password_arg,
                       char *plugin_auth_arg) {
      if ((username.first = (username_arg != nullptr)))
        username.second.assign(username_arg);
      if ((password.first = (password_arg != nullptr)))
        password.second.assign(password_arg);
      if ((plugin_auth.first = (plugin_auth_arg != nullptr)))
        plugin_auth.second.assign(plugin_auth_arg);
    }
  };

 private:
  typedef std::pair<std::string, Channel_cred_param> channel_credential_pair;
  std::map<std::string, Channel_cred_param> m_credential_set;

  /**
    Constructor
  */
  Rpl_channel_credentials() = default;

  /**
    Destructor
  */
  virtual ~Rpl_channel_credentials() = default;

 public:
  /**
    Returns object

    @return instance
  */
  static Rpl_channel_credentials &get_instance();

 public:
  /**
    Delete all stored credentials and delete instance.
  */
  void reset();

  /**
    Number of channels stored.

    @return number of channels
  */
  int number_of_channels();

  /**
   Method to get channel credentials.

    @param[in]   channel_name  The channel.
    @param[out]  user          Username of channel.
    @param[out]  pass          Password of channel.
    @param[out]  auth          Authentication plugin.

    @return the operation status
      @retval 0   OK
      @retval 1   Credentials do not exist.
  */
  int get_credentials(const char *channel_name, String_set &user,
                      String_set &pass, String_set &auth);

  /**
    Method to store credentials in map.

    @param[in]  channel_name  The channel name to store.
    @param[in]  username      Username for channel.
    @param[in]  password      Password for channel.
    @param[in]  plugin_auth   Authentication plugin.

    @return the operation status
      @retval 0   OK
      @retval 1   Error, credentials already exists
  */
  int store_credentials(const char *channel_name, char *username,
                        char *password, char *plugin_auth);

  /**
   Method to delete channel credentials.

    @param[in]  channel_name  The channel.

    @return the operation status
      @retval 0   OK
      @retval 1   Credentials do not exist.
  */
  int delete_credentials(const char *channel_name);
};
#endif  // RPL_CHANNEL_CREDENTIALS_INCLUDE
