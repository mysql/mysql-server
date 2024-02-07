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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <gtest/gtest.h>

#include "sql/rpl_channel_credentials.h"

namespace credential_struct_unittest {

class CredentialStructTesting : public ::testing::Test {
 protected:
  CredentialStructTesting() = default;

  void SetUp() override {
    user[0] = 0;
    pass[0] = 0;
    auth[0] = 0;
    strcpy(user, "username");
    strcpy(pass, "password");
    strcpy(auth, "authentication");
  }

  void TearDown() override {
    user[0] = 0;
    pass[0] = 0;
    auth[0] = 0;
  }

  char user[1024];
  char pass[1024];
  char auth[1024];
};

TEST_F(CredentialStructTesting, AssertAddition) {
  /* Assert credential set is empty. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().number_of_channels(), 0);

  /* Create channel channel_1 */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_1", this->user, this->pass, this->auth),
            0);
  /* Assert credentials of channel are added. */
  ASSERT_EQ(1,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Create channel channel_2 */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_2", this->user, this->pass, this->auth),
            0);
  /* Assert credentials of channel are added. */
  ASSERT_EQ(2,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* channel_2 already exist so it should fail, re-use store pointer. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_2", this->user, this->pass, this->auth),
            1);
  /* Assert credentials of channel are not added. */
  ASSERT_EQ(2,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Create channel channel_3, reuse store pointer. */
  this->user[0] = 0;
  this->pass[0] = 0;
  this->auth[0] = 0;
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_3", this->user, this->pass, this->auth),
            0);
  /* Assert credentials of channel are not added. */
  ASSERT_EQ(3,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Create channel channel_4. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_4", this->user, this->pass, this->auth),
            0);

  ASSERT_EQ(4,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Cleanup */
  Rpl_channel_credentials::get_instance().reset();
  ASSERT_EQ(true,
            Rpl_channel_credentials::get_instance().number_of_channels() == 0);
}

TEST_F(CredentialStructTesting, AssertDeletion) {
  /* Assert credential set is empty. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().number_of_channels(), 0);

  /* Store 3 channels. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_1", this->user, this->pass, this->auth),
            0);
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_2", this->user, this->pass, this->auth),
            0);
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_3", this->user, this->pass, this->auth),
            0);

  /* Assert 3 channels are present. */
  ASSERT_EQ(3,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Delete channel_3. */
  ASSERT_EQ(
      Rpl_channel_credentials::get_instance().delete_credentials("channel_3"),
      0);
  /* Assert channel is deleted. */
  ASSERT_EQ(2,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Delete channel_3 throws error now. */
  ASSERT_EQ(
      Rpl_channel_credentials::get_instance().delete_credentials("channel_3"),
      1);
  ASSERT_EQ(2,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Check error is thrown for non-existing channel. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().delete_credentials(
                "channel_does_not_exist"),
            1);
  ASSERT_EQ(2,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Delete channel_2. */
  ASSERT_EQ(
      Rpl_channel_credentials::get_instance().delete_credentials("channel_2"),
      0);
  /* Assert channel is deleted. */
  ASSERT_EQ(1,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Delete channel_1. */
  ASSERT_EQ(
      Rpl_channel_credentials::get_instance().delete_credentials("channel_1"),
      0);

  /* Assert instance is automatically deleted. */
  ASSERT_EQ(true,
            Rpl_channel_credentials::get_instance().number_of_channels() == 0);

  /* Assert delete gives error when channel list is empty. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().delete_credentials(
                "channel_list_empty"),
            1);
  ASSERT_EQ(true,
            Rpl_channel_credentials::get_instance().number_of_channels() == 0);
}

TEST_F(CredentialStructTesting, AssertCleanup) {
  /* Assert credential set is empty. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().number_of_channels(), 0);
  Rpl_channel_credentials::get_instance().reset();
  ASSERT_EQ(Rpl_channel_credentials::get_instance().number_of_channels(), 0);

  /* Store 3 channels. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_1", this->user, this->pass, this->auth),
            0);
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_2", this->user, this->pass, this->auth),
            0);
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_3", this->user, this->pass, this->auth),
            0);
  ASSERT_EQ(3,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Delete all channels. */
  Rpl_channel_credentials::get_instance().reset();
  ASSERT_EQ(true,
            Rpl_channel_credentials::get_instance().number_of_channels() == 0);
}

TEST_F(CredentialStructTesting, AssertGet) {
  String_set user, pass, auth;

  /* Assert credential set is empty. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().number_of_channels(), 0);
  ASSERT_EQ(Rpl_channel_credentials::get_instance().get_credentials(
                "channel_does_not_exist", user, pass, auth),
            1);

  /* Store 3 channels. */
  /* Channel_1 credentials are username, password, authentication. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_1", this->user, this->pass, this->auth),
            0);
  /* Channel_2 credentials are username, password, NULL, NULL. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_2", this->user, this->pass, nullptr),
            0);
  /* Channel_3 credentials are username, NULL, NULL, NULL. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_3", this->user, nullptr, nullptr),
            0);
  /* Channel_4 credentials are "", "", "", "". */
  this->user[0] = 0;
  this->pass[0] = 0;
  this->auth[0] = 0;
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_4", this->user, this->pass, this->auth),
            0);
  /* Channel_5 credentials are NULL, NULL, NULL, NULL. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().store_credentials(
                "channel_5", nullptr, nullptr, nullptr),
            0);

  ASSERT_EQ(5,
            (int)Rpl_channel_credentials::get_instance().number_of_channels());

  /* Assert credentials are correct for channel_1. */
  /* Channel_1 credentials are username, password, authentication. */
  Rpl_channel_credentials::get_instance().get_credentials("channel_1", user,
                                                          pass, auth);
  ASSERT_EQ(std::string("username"), user.second);
  ASSERT_EQ(std::string("password"), pass.second);
  ASSERT_EQ(std::string("authentication"), auth.second);
  ASSERT_EQ(user.first, true);
  ASSERT_EQ(pass.first, true);
  ASSERT_EQ(auth.first, true);

  /* Assert credentials are correct for channel_2. */
  /* Channel_2 credentials are username, password, NULL, NULL. */
  Rpl_channel_credentials::get_instance().get_credentials("channel_2", user,
                                                          pass, auth);
  ASSERT_EQ(std::string("username"), user.second);
  ASSERT_EQ(std::string("password"), pass.second);
  ASSERT_EQ(std::string(""), auth.second);
  ASSERT_EQ(user.first, true);
  ASSERT_EQ(pass.first, true);
  ASSERT_EQ(auth.first, false);

  /* Assert credentials are correct for channel_3. */
  /* Channel_3 credentials are username, NULL, NULL, NULL. */
  Rpl_channel_credentials::get_instance().get_credentials("channel_3", user,
                                                          pass, auth);
  ASSERT_EQ(std::string("username"), user.second);
  ASSERT_EQ(std::string(""), pass.second);
  ASSERT_EQ(std::string(""), auth.second);
  ASSERT_EQ(user.first, true);
  ASSERT_EQ(pass.first, false);
  ASSERT_EQ(auth.first, false);

  /* Assert credentials are correct for channel_4. */
  /* Channel_4 credentials are "", "", "", "". */
  Rpl_channel_credentials::get_instance().get_credentials("channel_4", user,
                                                          pass, auth);
  ASSERT_EQ(std::string(""), user.second);
  ASSERT_EQ(std::string(""), pass.second);
  ASSERT_EQ(std::string(""), auth.second);
  ASSERT_EQ(user.first, true);
  ASSERT_EQ(pass.first, true);
  ASSERT_EQ(auth.first, true);

  /* Assert credentials are correct for channel_5. */
  /* Channel_5 credentials are NULL, NULL, NULL, NULL. */
  Rpl_channel_credentials::get_instance().get_credentials("channel_5", user,
                                                          pass, auth);
  ASSERT_EQ(user.second.empty(), true);
  ASSERT_EQ(pass.second.empty(), true);
  ASSERT_EQ(auth.second.empty(), true);
  ASSERT_EQ(user.first, false);
  ASSERT_EQ(pass.first, false);
  ASSERT_EQ(auth.first, false);

  /* get returns error when channel does not exist. */
  ASSERT_EQ(Rpl_channel_credentials::get_instance().get_credentials(
                "channel_does_not_exist", user, pass, auth),
            1);
  ASSERT_EQ(user.second.empty(), true);
  ASSERT_EQ(pass.second.empty(), true);
  ASSERT_EQ(auth.second.empty(), true);
  ASSERT_EQ(user.first, false);
  ASSERT_EQ(pass.first, false);
  ASSERT_EQ(auth.first, false);

  /* Delete all channels. */
  Rpl_channel_credentials::get_instance().reset();
  ASSERT_EQ(true,
            Rpl_channel_credentials::get_instance().number_of_channels() == 0);
}

}  // namespace credential_struct_unittest
