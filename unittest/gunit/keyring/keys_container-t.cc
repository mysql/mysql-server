/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include <my_global.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mysql/plugin_keyring.h>
#include <keys_container.h>
#include "mock_logger.h"
#include <fstream>

#if !defined(MERGE_UNITTESTS)
#ifdef HAVE_PSI_INTERFACE
namespace keyring
{
  PSI_memory_key key_memory_KEYRING = PSI_NOT_INSTRUMENTED;
  PSI_memory_key key_LOCK_keyring = PSI_NOT_INSTRUMENTED;
}
#endif
mysql_rwlock_t LOCK_keyring;
#endif

namespace keyring__keys_container_unittest
{
  using namespace keyring;
  using ::testing::Return;
  using ::testing::InSequence;
  using ::testing::_;
  using ::testing::StrEq;

  my_bool check_if_file_exists_and_TAG_is_correct(const char* file_name)
  {
    char tag[4];
    std::fstream file;

    file.open(file_name, std::fstream::in | std::fstream::binary);
    if (!file.is_open())
      return FALSE;
    file.seekg(0, file.end);
    if (file.tellg() < 3)
      return FALSE; // File do not contains tag
    file.seekg(-3, file.end);
    file.read(tag, 3);
    tag[3]= '\0';
    file.close();
    return strcmp(tag, "EOF") == 0;
  }

  class Keys_container_test : public ::testing::Test
  {
  public:
    Keys_container_test() : file_name("./keyring") {}
  protected:
    virtual void SetUp()
    {
      sample_key_data= "Robi";
      sample_key= new Key("Roberts_key", "AES", "Robert", sample_key_data.c_str(), sample_key_data.length()+1);

      remove(file_name.c_str());
      remove("./keyring.backup");

      logger= new Mock_logger();
      keys_container= new Keys_container(logger);
    }
    virtual void TearDown()
    {
      remove(file_name.c_str());
      delete keys_container;
      delete logger;
    }
    void create_keyring_file(const char *file_name, const char *keyring_buffer);
    void generate_keyring_file_with_correct_structure(const char *file_name);
    void generate_keyring_file_with_incorrect_file_version(const char *file_name);
    void generate_keyring_file_with_incorrect_TAG(const char *file_name);
  protected:
    Keys_container *keys_container;
    ILogger *logger;
    Key *sample_key;
    std::string sample_key_data;
    std::string file_name;
  };

  void Keys_container_test::create_keyring_file(const char *file_name, const char *keyring_buffer)
  {
    std::fstream file;
    file.open(file_name,
              std::fstream::out | std::fstream::binary | std::fstream::trunc);
    ASSERT_TRUE(file.is_open());
    file.write(keyring_buffer, strlen(keyring_buffer));
    file.close();
  }

  void Keys_container_test::generate_keyring_file_with_correct_structure(const char *file_name)
  {
    static const char *keyring_buffer= "Keyring file version:1.0EOF";
    create_keyring_file(file_name, keyring_buffer);
  }

  void Keys_container_test::generate_keyring_file_with_incorrect_file_version(const char *file_name)
  {
    static const char *keyring_buffer= "Keyring file version:2.0EOF";
    create_keyring_file(file_name, keyring_buffer);
  }

  void Keys_container_test::generate_keyring_file_with_incorrect_TAG(const char *file_name)
  {
    static const char *keyring_buffer= "Keyring file version:2.0EF";
    create_keyring_file(file_name, keyring_buffer);
  }

  TEST_F(Keys_container_test, InitWithFileWithCorrectStruct)
  {
    const char *keyring_correct_struct= "./keyring_correct_struct";
    remove(keyring_correct_struct);
    generate_keyring_file_with_correct_structure(keyring_correct_struct);
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, keyring_correct_struct), 0);
    remove(keyring_correct_struct);
    delete sample_key; //unused in this test
  }

// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_test, InitWithFileWithIncorrectKeyringVersion)
  {
    const char *keyring_incorrect_version= "./keyring_incorrect_version";
    remove(keyring_incorrect_version);
    generate_keyring_file_with_incorrect_file_version(keyring_incorrect_version);
    Buffered_file_io keyring_io(logger);
    EXPECT_CALL(*((Mock_logger *)logger),
                log(MY_ERROR_LEVEL, StrEq("Incorrect Keyring file version")));
    EXPECT_EQ(keys_container->init(&keyring_io, keyring_incorrect_version), 1);
    remove(keyring_incorrect_version);
    delete sample_key; //unused in this test
  }
#endif  // HAVE_UBSAN

  TEST_F(Keys_container_test, InitWithFileWithIncorrectTAG)
  {
    const char *keyring_incorrect_tag= "./keyring_incorrect_tag";
    remove(keyring_incorrect_tag);
    generate_keyring_file_with_incorrect_TAG(keyring_incorrect_tag);
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, keyring_incorrect_tag), 1);
    remove(keyring_incorrect_tag);
    delete sample_key; //unused in this test
  }

  TEST_F(Keys_container_test, StoreFetchRemove)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    Key key_id("Roberts_key", NULL, "Robert",NULL,0);
    IKey* fetched_key= keys_container->fetch_key(&key_id);

    ASSERT_TRUE(fetched_key != NULL);
    std::string expected_key_signature= "Roberts_keyRobert";
    EXPECT_STREQ(fetched_key->get_key_signature()->c_str(), expected_key_signature.c_str());
    EXPECT_EQ(fetched_key->get_key_signature()->length(), expected_key_signature.length());
    uchar* key_data_fetched= fetched_key->get_key_data();
    size_t key_data_fetched_size= fetched_key->get_key_data_size();
    EXPECT_STREQ(sample_key_data.c_str(), reinterpret_cast<const char*>(key_data_fetched));
    EXPECT_STREQ("AES", fetched_key->get_key_type()->c_str());
    ASSERT_TRUE(sample_key_data.length()+1 == key_data_fetched_size);

    keys_container->remove_key(&keyring_io, &key_id);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 0);
    my_free(fetched_key->release_key_data());
  }

  TEST_F(Keys_container_test, FetchNotExisting)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    Key key_id("Roberts_key", NULL, "Robert",NULL,0);
    IKey* fetched_key= keys_container->fetch_key(&key_id);
    ASSERT_TRUE(fetched_key == NULL);
    delete sample_key; //unused in this test
  }

  TEST_F(Keys_container_test, RemoveNotExisting)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    Key key_id("Roberts_key", "AES", "Robert",NULL,0);
    ASSERT_TRUE(keys_container->remove_key(&keyring_io, &key_id) == TRUE);
    delete sample_key; //unused in this test
  }

  TEST_F(Keys_container_test, StoreFetchNotExisting)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    Key key_id("NotRoberts_key", NULL, "NotRobert",NULL,0);
    IKey* fetched_key= keys_container->fetch_key(&key_id);
    ASSERT_TRUE(fetched_key == NULL);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
  }

  TEST_F(Keys_container_test, StoreRemoveNotExisting)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    Key key_id("NotRoberts_key", "AES", "NotRobert",NULL,0);
    // Failed to remove key
    ASSERT_TRUE(keys_container->remove_key(&keyring_io, &key_id) == TRUE);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
  }

  TEST_F(Keys_container_test, StoreStoreStoreFetchRemove)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    std::string key_data1("Robi1");
    Key *key1= new Key("Roberts_key1", "AES", "Robert", key_data1.c_str(), key_data1.length()+1);

    EXPECT_EQ(keys_container->store_key(&keyring_io, key1), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);

    std::string key_data2("Robi2");
    Key *key2= new Key("Roberts_key2", "AES", "Robert", key_data2.c_str(), key_data2.length()+1);

    EXPECT_EQ(keys_container->store_key(&keyring_io, key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 3);

    std::string key_data3("Robi3");
    Key *key3= new Key("Roberts_key3", "AES", "Robert", key_data3.c_str(), key_data3.length()+1);

    EXPECT_EQ(keys_container->store_key(&keyring_io, key3), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 4);

    Key key2_id("Roberts_key2", NULL, "Robert",NULL,0);
    IKey* fetched_key= keys_container->fetch_key(&key2_id);

    ASSERT_TRUE(fetched_key != NULL);
    std::string expected_key_signature= "Roberts_key2Robert";
    EXPECT_STREQ(fetched_key->get_key_signature()->c_str(), expected_key_signature.c_str());
    EXPECT_EQ(fetched_key->get_key_signature()->length(), expected_key_signature.length());
    uchar *key_data_fetched= fetched_key->get_key_data();
    size_t key_data_fetched_size= fetched_key->get_key_data_size();
    EXPECT_STREQ(key_data2.c_str(), reinterpret_cast<const char*>(key_data_fetched));
    ASSERT_TRUE(key_data2.length()+1 == key_data_fetched_size);

    Key key3_id("Roberts_key3", "AES", "Robert",NULL,0);
    keys_container->remove_key(&keyring_io, &key3_id);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 3);

    my_free(fetched_key->release_key_data());
}

  TEST_F(Keys_container_test, StoreTwiceTheSame)
  {
    Buffered_file_io keyring_io(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 1);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
  }

  class Buffered_file_io_dont_remove_backup : public Buffered_file_io
  {
  public:
    Buffered_file_io_dont_remove_backup(ILogger *logger)
      : Buffered_file_io(logger) {}

    my_bool close()
    {
      buffer.free();
      return FALSE;
    }
  };

  class Keys_container_test_dont_close : public ::testing::Test
  {
  public:
    Keys_container_test_dont_close() : file_name("./keyring") {}
  protected:
    virtual void SetUp()
    {
      sample_key_data= "Robi";
      sample_key= new Key("Roberts_key", "AES", "Robert", sample_key_data.c_str(), sample_key_data.length()+1);
      std::string sample_key_data2="xobi2";
      sample_key2= new Key("Roberts_key2", "AES", "Robert", sample_key_data2.c_str(), sample_key_data2.length()+1);

      //Remove Keyring files just to be save
      remove(file_name.c_str());
      remove("./keyring.backup");
      remove("./keyring.backup.backup");
    }
    virtual void TearDown()
    {
      remove(file_name.c_str());
    }
    void generate_malformed_keyring_file_without_tag(const char *file_name);
  protected:
    Key *sample_key;
    Key *sample_key2;
    std::string sample_key_data;
    std::string file_name;
  };

  void Keys_container_test_dont_close::generate_malformed_keyring_file_without_tag(const char *file_name)
  {
    static const char *malformed_keyring_buffer= "Key1AESRobertKEYDATA"
      "Key2AESZibiDATAKey3DATA...crashing";

    std::fstream file;
    file.open(file_name, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    ASSERT_TRUE(file.is_open());
    file.write(malformed_keyring_buffer, strlen(malformed_keyring_buffer));
    file.close();
  }

  TEST_F(Keys_container_test_dont_close, CheckIfCorrectBackupFileIsCreatedAfterStoringOneKey)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io_dont_remove_backup keyring_io_dont_remove_backup(logger);
    Keys_container *keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io_dont_remove_backup, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io_dont_remove_backup, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), TRUE);

    //Check if backup file is empty
    delete keys_container;
    delete logger;
    logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    keys_container= new Keys_container(logger);
    ASSERT_TRUE(keys_container->init(&keyring_io, "./keyring.backup") == 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 0);

    remove("./keyring.backup");
    remove("./keyring.backup.backup"); //leftover from initializing keyring with backup file
    remove(file_name.c_str());
    delete keys_container;
    delete logger;
    delete sample_key2; //unused in this test
  }

  TEST_F(Keys_container_test_dont_close, CheckIfCorrectBackupFileIsCreatedAfterStoringTwoKeys)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    Keys_container *keys_container= new Keys_container(logger);
    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    //successfully stored the key - backup file does not exist
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);
    delete keys_container;
    delete logger;

    logger= new Mock_logger();
    Buffered_file_io_dont_remove_backup keyring_io_dont_remove_backup(logger);
    keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io_dont_remove_backup, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io_dont_remove_backup, sample_key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);

    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), TRUE);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring"), TRUE);

    delete keys_container;
    delete logger;
    //Check that backup file contains sample_key only
    logger= new Mock_logger();
    Buffered_file_io keyring_io_2(logger);
    keys_container= new Keys_container(logger);
    EXPECT_EQ(keys_container->init(&keyring_io_2, file_name), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    Key sample_key_id("Roberts_key", NULL, "Robert", NULL, 0);
    IKey *fetchedKey= keys_container->fetch_key(&sample_key_id);
    ASSERT_TRUE(fetchedKey != NULL);

    ASSERT_TRUE(*fetchedKey->get_key_signature() == "Roberts_keyRobert");
    ASSERT_TRUE(memcmp(fetchedKey->get_key_data(), "Robi", fetchedKey->get_key_data_size()) == 0);

    remove("./keyring.backup");
    remove("./keyring.backup.backup"); //leftover from initializing keyring with backup file
    remove(file_name.c_str());
    delete keys_container;
    delete logger;
    my_free(fetchedKey->release_key_data());
  }

  TEST_F(Keys_container_test_dont_close, CheckIfCorrectBackupFileIsCreatedBeforeRemovingKey)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    Keys_container *keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    //successfully stored the key - backup file does not exist
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);

    delete keys_container;
    delete logger;
    logger= new Mock_logger();
    Buffered_file_io_dont_remove_backup keyring_io_dont_remove_backup(logger);
    keys_container= new Keys_container(logger);

    ASSERT_TRUE(keys_container->init(&keyring_io_dont_remove_backup, file_name) == 0);
    Key sample_key_id("Roberts_key", "AES", "Robert", NULL, 0);
    EXPECT_EQ(keys_container->remove_key(&keyring_io_dont_remove_backup, &sample_key_id), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), TRUE);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring"), TRUE);

    delete keys_container;
    delete logger;
    //Check that backup file contains sample_key and sample_key2
    logger= new Mock_logger();
    Buffered_file_io keyring_io_2(logger);
    keys_container= new Keys_container(logger);
    EXPECT_EQ(keys_container->init(&keyring_io_2, "./keyring.backup"), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    Key sample_key2_id("Roberts_key2", NULL, "Robert", NULL, 0);
    IKey *fetchedKey= keys_container->fetch_key(&sample_key2_id);
    ASSERT_TRUE(fetchedKey != NULL);
    ASSERT_TRUE(*fetchedKey->get_key_signature() == "Roberts_key2Robert");
    ASSERT_TRUE(memcmp(fetchedKey->get_key_data(), "xobi2", fetchedKey->get_key_data_size()) == 0);

    remove("./keyring.backup");
    remove("./keyring.backup.backup"); //leftover from initializing keyring with backup file
    remove(file_name.c_str());
    delete keys_container;
    delete logger;
    my_free(fetchedKey->release_key_data());
  }

  TEST_F(Keys_container_test_dont_close, CheckIfBackupFileIsNotCreatedForFetching)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    Keys_container *keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    //successfully stored the key - backup file does not exist
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);

    delete keys_container;
    delete logger;
    logger= new Mock_logger();
    Buffered_file_io_dont_remove_backup keyring_io_dont_remove_backup(logger);
    keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io_dont_remove_backup, file_name), 0);
    Key sample_key_id("Roberts_key", NULL, "Robert", NULL, 0);
    IKey *fetchedKey= keys_container->fetch_key(&sample_key_id);
    ASSERT_TRUE(fetchedKey != NULL);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    //check if the backup file was not created
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring"), TRUE);

    remove("./keyring.backup");
    remove(file_name.c_str());
    delete keys_container;
    delete logger;
    my_free(fetchedKey->release_key_data());
  }

  TEST_F(Keys_container_test_dont_close, KeyringfileIsMalformedCheckIfBackupIsLoaded)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    Keys_container *keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    //successfully stored the key - backup file does not exist
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    //Now we have correct backup file
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);

    delete keys_container;
    delete logger;
    logger= new Mock_logger();
    Buffered_file_io_dont_remove_backup keyring_io_dont_remove_backup(logger);
    keys_container= new Keys_container(logger);

    //this key will not be in backup file thus we do not care about it
    Key *sample_key3= new Key("Roberts_key3", "ZZZZ", "MaybeRobert", (void*)("DATA"), strlen("DATA"));

    EXPECT_EQ(keys_container->init(&keyring_io_dont_remove_backup, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io_dont_remove_backup, sample_key3), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 3);
    //Now we have correct backup file
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), TRUE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);

    delete keys_container;
    delete logger;
    remove("./keyring");
    generate_malformed_keyring_file_without_tag("./keyring");
    logger= new Mock_logger();
    Buffered_file_io keyring_io_2(logger);
    keys_container= new Keys_container(logger);

    ASSERT_TRUE(keys_container->init(&keyring_io_2, file_name) == 0);
    //Check that keyring from backup was loaded as the keyring file is corrupted
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    Key sample_key_id("Roberts_key", NULL, "Robert", NULL, 0);
    Key sample_key2_id("Roberts_key2", NULL, "Robert", NULL, 0);
    IKey *fetchedKey= keys_container->fetch_key(&sample_key2_id);
    ASSERT_TRUE(fetchedKey != NULL);
    ASSERT_TRUE(*fetchedKey->get_key_signature() == "Roberts_key2Robert");
    ASSERT_TRUE(memcmp(fetchedKey->get_key_data(), "xobi2", fetchedKey->get_key_data_size()) == 0);
    IKey *fetchedKey2= keys_container->fetch_key(&sample_key_id);
    ASSERT_TRUE(fetchedKey2 != NULL);
    ASSERT_TRUE(*fetchedKey2->get_key_signature() == "Roberts_keyRobert");
    ASSERT_TRUE(memcmp(fetchedKey2->get_key_data(), "Robi", fetchedKey2->get_key_data_size()) == 0);

    //check if the backup file was removed
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring"), TRUE);

    remove("./keyring.backup");
    remove(file_name.c_str());
    delete keys_container;
    delete logger;
    my_free(fetchedKey->release_key_data());
    my_free(fetchedKey2->release_key_data());
  }

// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_test_dont_close, BackupfileIsMalformedCheckItIsIgnoredAndDeleted)
  {
    ILogger *logger= new Mock_logger();
    Buffered_file_io keyring_io(logger);
    Keys_container *keys_container= new Keys_container(logger);

    EXPECT_EQ(keys_container->init(&keyring_io, file_name), 0);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);
    //successfully stored the key - backup file does not exist
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);
    EXPECT_EQ(keys_container->store_key(&keyring_io, sample_key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    //Now we have correct backup file
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    ASSERT_TRUE(check_if_file_exists_and_TAG_is_correct("./keyring") == TRUE);

    delete keys_container;
    delete logger;
    generate_malformed_keyring_file_without_tag("./keyring.backup");
    logger= new Mock_logger();
    Buffered_file_io keyring_io_2(logger);
    keys_container= new Keys_container(logger);

    //Check that backup file was ignored (as backup file is malformed)
    EXPECT_CALL(*((Mock_logger *)logger), log(MY_WARNING_LEVEL, StrEq("Found malformed keyring backup file - removing it")));
    EXPECT_EQ(keys_container->init(&keyring_io_2, file_name), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    Key sample_key_id("Roberts_key", NULL, "Robert", NULL, 0);
    Key sample_key2_id("Roberts_key2", NULL, "Robert", NULL, 0);
    IKey *fetchedKey= keys_container->fetch_key(&sample_key2_id);
    ASSERT_TRUE(fetchedKey != NULL);
    ASSERT_TRUE(*fetchedKey->get_key_signature() == "Roberts_key2Robert");
    ASSERT_TRUE(memcmp(fetchedKey->get_key_data(), "xobi2", fetchedKey->get_key_data_size()) == 0);
    IKey *fetchedKey2= keys_container->fetch_key(&sample_key_id);
    ASSERT_TRUE(fetchedKey2 != NULL);
    ASSERT_TRUE(*fetchedKey2->get_key_signature() == "Roberts_keyRobert");
    ASSERT_TRUE(memcmp(fetchedKey2->get_key_data(), "Robi", fetchedKey2->get_key_data_size()) == 0);

    //check if the backup file was removed
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring.backup"), FALSE);
    EXPECT_EQ(check_if_file_exists_and_TAG_is_correct("./keyring"), TRUE);

    delete keys_container;
    delete logger;
    my_free(fetchedKey->release_key_data());
    my_free(fetchedKey2->release_key_data());
  }
#endif  // HAVE_UBSAN

  class Mock_keyring_io : public IKeyring_io
  {
  public:
    Mock_keyring_io()
    : load_key_from_buffer_on_call_number(-1)
    , operator_in_call_counter(0)
    , buffer_size(0)
    , set_invalid_key_in_operator_in(FALSE)
    {
      char key_data[]= "Robi_test";
      Key key("Roberts_key2", "AES", "Robert", key_data, sizeof(key_data));
      size_t buffer_position= 0;
      buffer= new uchar[key.get_key_pod_size()];
      key.store_in_buffer(buffer, &buffer_position);
      assert(buffer_position == key.get_key_pod_size());
      buffer_size= key.get_key_pod_size();
    }

    ~Mock_keyring_io()
    {
      delete[] buffer;
    }

    MOCK_METHOD1(init, my_bool(std::string *keyring_filename));
    MOCK_METHOD1(open, my_bool(std::string *keyring_filename));
    MOCK_METHOD1(reserve_buffer, void(size_t memory_size));
    MOCK_METHOD0(flush_to_backup, my_bool());

//    virtual my_bool flush_to_keyring(const IKey *key = NULL)= 0;
    MOCK_METHOD2(flush_to_keyring, my_bool(IKey *key, Flush_operation operation));
    MOCK_METHOD1(operator_out, my_bool(const IKey* key));
    MOCK_METHOD1(operator_in, my_bool(IKey* key));
    MOCK_METHOD0(close, my_bool());


    virtual my_bool operator<< (const IKey* key) { return operator_out(key); }
    virtual my_bool operator>> (IKey* key) {
      if (load_key_from_buffer_on_call_number >= 0 && load_key_from_buffer_on_call_number == operator_in_call_counter)
      {
        size_t number_of_bytes_read= 0;
        key->load_from_buffer(buffer, &number_of_bytes_read, buffer_size);
        assert (number_of_bytes_read == buffer_size); //there was only one key in buffer so the whole key should have been read
      }
      operator_in_call_counter++;
      if (set_invalid_key_in_operator_in)
      {
        std::string invalid_key_type("ZZZ");
        key->set_key_type(&invalid_key_type);
      }
      return operator_in(key);
    }

    int load_key_from_buffer_on_call_number;
    int operator_in_call_counter;
    size_t buffer_size;
    my_bool set_invalid_key_in_operator_in;
  protected:
    uchar *buffer;
  };

  class Keys_container_with_mocked_io_test : public ::testing::Test
  {
  protected:
    virtual void SetUp()
    {
      std::string sample_key_data("Robi");
      sample_key= new Key("Roberts_key", "AES", "Robert", sample_key_data.c_str(), sample_key_data.length()+1);

      file_name= "/home/rob/write_key";
    }
    virtual void TearDown()
    {
      remove(file_name.c_str());
      delete keys_container;
    }
  protected:
    Keys_container *keys_container;
    Mock_keyring_io *keyring_io;
    Key *sample_key;
    char* sample_key_data;
    std::string file_name;

    void expect_calls_on_init();
    void expect_calls_on_store_sample_key();
  };

  void Keys_container_with_mocked_io_test::expect_calls_on_init()
  {
    EXPECT_CALL(*keyring_io, init(Pointee(StrEq(file_name))))
      .WillOnce(Return(0)); // init successfull
    EXPECT_CALL(*keyring_io, operator_in(_))
      .WillOnce(Return(0)); //nothing to read
    EXPECT_CALL(*keyring_io, close());
  }

  void Keys_container_with_mocked_io_test::expect_calls_on_store_sample_key()
  {
    {
      InSequence dummy;

      EXPECT_CALL(*keyring_io, open(Pointee(StrEq(file_name))))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(0));
      EXPECT_CALL(*keyring_io, flush_to_backup())
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_keyring(sample_key, STORE_KEY))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, close())
        .WillOnce(Return(0));
    }
  }

// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_with_mocked_io_test, ErrorFromIODuringInit)
  {
    keyring_io= new Mock_keyring_io();
    Mock_logger *logger= new Mock_logger();
    keys_container= new Keys_container(logger);

    keyring_io->load_key_from_buffer_on_call_number= 0;

    EXPECT_CALL(*keyring_io, init(Pointee(StrEq(file_name))))
      .WillOnce(Return(0)); // init successfull
    {
      InSequence dummy;
      EXPECT_CALL(*keyring_io, operator_in(_)).WillOnce(Return(1));
      keyring_io->set_invalid_key_in_operator_in= TRUE;
      EXPECT_CALL(*keyring_io, operator_in(_)).WillOnce(Return(1));
    }
    EXPECT_CALL(*keyring_io, close());
    EXPECT_CALL(*logger, log(MY_ERROR_LEVEL, StrEq("Error while loading keyring content. The keyring might be malformed")));

    EXPECT_EQ(keys_container->init(keyring_io, file_name), 1);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 0);
    delete logger;
    delete keyring_io;
    delete sample_key; //unused in this test
  }
#endif  // HAVE_UBSAN

// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_with_mocked_io_test, StoreAndRemoveKey)
  {
    keyring_io= new Mock_keyring_io();
    ILogger *logger= new Mock_logger();
    keys_container= new Keys_container(logger);
    expect_calls_on_init();
    EXPECT_EQ(keys_container->init(keyring_io, file_name), 0);

    expect_calls_on_store_sample_key();
    EXPECT_EQ(keys_container->store_key(keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    {
      InSequence dummy;

      EXPECT_CALL(*keyring_io, open(Pointee(StrEq(file_name))))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_backup())
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(0));
      EXPECT_CALL(*keyring_io, flush_to_keyring(sample_key, REMOVE_KEY))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, close())
        .WillOnce(Return(0));
    }
    EXPECT_EQ(keys_container->remove_key(keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 0);
    delete logger;
    delete keyring_io;
  }
#endif  // HAVE_UBSAN

// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_with_mocked_io_test, ErrorFromIOWhileRemovingKeyAfterAdding2Keys)
  {
    std::string key_data2("Robi2");
    Key *key2= new Key("Roberts_key2", "AES", "Robert", key_data2.c_str(), key_data2.length()+1);

    keyring_io= new Mock_keyring_io();
    ILogger *logger= new Mock_logger();
    keys_container= new Keys_container(logger);
    expect_calls_on_init();
    EXPECT_EQ(keys_container->init(keyring_io, file_name), 0);

    //First add two keys

    expect_calls_on_store_sample_key();
    EXPECT_EQ(keys_container->store_key(keyring_io, sample_key), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 1);

    {
      InSequence dummy;

      EXPECT_CALL(*keyring_io, open(Pointee(StrEq(file_name))))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_backup())
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()+key2->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(key2))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_keyring(key2, STORE_KEY))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, close())
       .WillOnce(Return(0));
    }

    EXPECT_EQ(keys_container->store_key(keyring_io, key2), 0);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);

    EXPECT_CALL(*(Mock_logger *)logger, log(MY_ERROR_LEVEL, StrEq("Could not flush keys to keyring")));
    {
      InSequence dummy;

      EXPECT_CALL(*keyring_io, open(Pointee(StrEq(file_name))))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()+key2->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(key2))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_backup())
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(key2->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(_))
        .WillOnce(Return(0)); //error while >>
      EXPECT_CALL(*keyring_io, close());
      //error while flushing to keyring file - backup file remains
    }

    EXPECT_EQ(keys_container->remove_key(keyring_io, sample_key), 1);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 2);
    delete logger;
    delete keyring_io;
  }
#endif  // HAVE_UBSAN


// HAVE_UBSAN: undefined behaviour in gmock.
#if !defined(HAVE_UBSAN)
  TEST_F(Keys_container_with_mocked_io_test, ErrorFromIOFlushWhileStoringKey)
  {
    keyring_io= new Mock_keyring_io();
    ILogger *logger= new Mock_logger();
    keys_container= new Keys_container(logger);
    expect_calls_on_init();
    EXPECT_EQ(keys_container->init(keyring_io, file_name), 0);

    EXPECT_CALL(*(Mock_logger *)logger, log(MY_ERROR_LEVEL, StrEq("Could not flush keys to keyring")));
    {
      InSequence dummy;

      EXPECT_CALL(*keyring_io, open(Pointee(StrEq(file_name))))
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(0));
      EXPECT_CALL(*keyring_io, flush_to_backup())
        .WillOnce(Return(0));
      EXPECT_CALL(*keyring_io, reserve_buffer(sample_key->get_key_pod_size()));
      EXPECT_CALL(*keyring_io, operator_out(sample_key))
        .WillOnce(Return(1));
      EXPECT_CALL(*keyring_io, flush_to_keyring(sample_key, STORE_KEY))
        .WillOnce(Return(1));
      //backup file remains
    }

    EXPECT_EQ(keys_container->store_key(keyring_io, sample_key), 1);
    ASSERT_TRUE(keys_container->get_number_of_keys() == 0);
    delete logger;
    delete keyring_io;
    delete sample_key;
  }
#endif  // HAVE_UBSAN

  int main(int argc, char **argv) {
    if (mysql_rwlock_init(key_LOCK_keyring, &LOCK_keyring))
      return TRUE;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
  }
}
