/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#include <gtest/gtest.h>
#include <stddef.h>

#ifdef _WIN32
#include<Windows.h>
#else
#include <pthread.h>
#endif

#ifdef __GNUC__
// YaSSL code has unused parameter warnings, ignore them.
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic push
#endif
#include <runtime.hpp>
#include <yassl_int.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include <buffer.hpp>
#include <cert_wrapper.hpp>
#include <openssl/ssl.h>
#include <rsa.hpp>
#include <yassl_types.hpp>

#include "unittest/gunit/thread_utils.h"

namespace {

using thread::Notification;
using thread::Thread;

class YasslTest : public ::testing::Test
{
  virtual void SetUp()
  {
    const char private_key_buffer[] =
    {
      "-----BEGIN RSA PRIVATE KEY-----\n"
        "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\n"
        "-----END RSA PRIVATE KEY-----"
    };

    const char public_key_buffer[]=
    {
        "-----BEGIN PUBLIC KEY-----\n"
        "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAvV2VNbsQPG0Bh0KC8F4z\n"
        "CGXvMNcSicCiLXxeLWrJsmKZl0ggf2ydymYUUewq+dVxDdh85sdSvxEmtIWvKSRK\n"
        "+RRCAURztq2Succd+24SF5IZYjlIJE/U0AYUxHzUcOsannfzui60IaTHpcBFHTJK\n"
        "6myxGx9MORZmhfv580mfvz4yvgLjS5yGOIS6rlxD9YV1Y04Rx3SXQQBnC7rDBL91\n"
        "ktNWvbclsonfytY19N9p+Gprms30yRT+BmPFB7TqpReeZa3ivg15g/z3BLNyvj3Y\n"
        "KiQM3cd7ENJC2x2LRxL5pG684cFNStSjT4FvA+oh45UnU45aOSEjrxNkBG8ci0e+\n"
        "VKX539rK+nDzTE/MHpnvfHp4DB+kSYBPuKHY2Eaw31NwPpfLWwEJPiDrktJJmRZq\n"
        "ENMHLXksdiqGhvYmI33wZaZAfjbDZFMfPF5yBMBGDZ3aeNz5Le7uqS6g6XMOoiz/\n"
        "d2S5RzRrCol1yqCBPtODjfFPC4K8GGYVkWZgSCf/PRt/DgDnZOfZSSYIQNeyr21e\n"
        "mqgqQ+yhXEGKVjcDTKcbSLiWAdA+GkAzLAXXhafM8mrhpnGKdO4Or6ySz7G1vk2J\n"
        "t2ZSdP740oVSJi59P9NEgXcbd3c4FzjXSOOsxfhPQfobUk3ikt55lN3fBX3mBvUd\n"
        "uxNhAcQ02ZD5zXrX6+loiV8CAwEAAQ==\n"
        "-----END PUBLIC KEY-----"
    };

    FILE *priv_file= fopen("rsa_private_key.pem", "wb");
    ASSERT_TRUE(priv_file != 0);
    fwrite(private_key_buffer, strlen(private_key_buffer), 1, priv_file);
    fclose(priv_file);

    FILE *pub_file= fopen("rsa_public_key.pem", "wb");
    ASSERT_TRUE(pub_file != 0);
    fwrite(public_key_buffer, strlen(public_key_buffer), 1, pub_file);
    fclose(pub_file);
  }

  virtual void TearDown()
  {
    unlink("rsa_private_key.pem");
    unlink("rsa_public_key.pem");
  }
};

class Yassl_thread : public Thread
{
public:
  Yassl_thread(Notification *go, Notification *done)
    : m_sessions_instance(NULL), m_go(go), m_done(done)
  {}
  virtual void run()
  {
    // Wait until my creator tells me to go.
    m_go->wait_for_notification();
    yaSSL::Sessions &sessions= yaSSL::GetSessions();
    m_sessions_instance= &sessions;
    // Tell my creator I'm done.
    m_done->notify();
  }
  yaSSL::Sessions *m_sessions_instance;
private:
  Notification *m_go;
  Notification *m_done;
};


/**
  Verify that yaSSL::sessionsInstance is indeed a singleton.
  If any of the EXPECT_EQ below reports an error, it is not.
  We can also run 'valgrind ./yassl-t'. If there are errors,
  valgrind will report a multiple of
     sizeof(yaSSL::Sessions) == 80
  bytes lost.
 */
TEST_F(YasslTest, ManySessions)
{
  Notification go[5];
  Notification done[5];
  Yassl_thread t0(&go[0], &done[0]);
  Yassl_thread t1(&go[1], &done[1]);
  Yassl_thread t2(&go[2], &done[2]);
  Yassl_thread t3(&go[3], &done[3]);
  Yassl_thread t4(&go[4], &done[4]);

  t0.start();
  t1.start();
  t2.start();
  t3.start();
  t4.start();

  for (int ix= 0; ix < 5; ++ix)
    go[ix].notify();

  for (int ix= 0; ix < 5; ++ix)
    done[ix].wait_for_notification();

  // These are most likely to fail unless we use pthread_once.
  EXPECT_EQ(t0.m_sessions_instance, t1.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t2.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t0.m_sessions_instance, t4.m_sessions_instance);

  // These rarely fail. If they do, we have more than two instances.
  EXPECT_EQ(t1.m_sessions_instance, t2.m_sessions_instance);
  EXPECT_EQ(t1.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t1.m_sessions_instance, t4.m_sessions_instance);

  EXPECT_EQ(t2.m_sessions_instance, t3.m_sessions_instance);
  EXPECT_EQ(t2.m_sessions_instance, t4.m_sessions_instance);

  EXPECT_EQ(t3.m_sessions_instance, t4.m_sessions_instance);

  t0.join();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  yaSSL_CleanUp();
}

using TaoCrypt::RSA_PrivateKey;
using TaoCrypt::RSA_PublicKey;
using TaoCrypt::RSA_Encryptor;
using TaoCrypt::RSA_Decryptor;
using TaoCrypt::PK_Lengths;
using TaoCrypt::Source;
using TaoCrypt::FileSource;
using TaoCrypt::RandomNumberGenerator;
using TaoCrypt::word32;
using TaoCrypt::RSA_BlockType1;
using TaoCrypt::RSAES_Encryptor;
using TaoCrypt::RSAES_Decryptor;
using yaSSL::byte;
using yaSSL::x509;
using yaSSL::RSA;
using yaSSL::CertType;
using yaSSL::RandomPool;
using yaSSL::PEM_read_RSAPrivateKey;
using yaSSL::PEM_read_RSA_PUBKEY;
using yaSSL::RSA_private_decrypt;
using yaSSL::RSA_public_encrypt;

TEST_F(YasslTest, RSA)
{
  FILE *priv_file= fopen("rsa_private_key.pem", "rb");
  RSA * priv_rsa= PEM_read_RSAPrivateKey(priv_file, 0, 0, 0);
  ASSERT_TRUE(priv_rsa != 0);
  fclose(priv_file);

  FILE *pub_file= fopen("rsa_public_key.pem", "rb");
  RSA * pub_rsa= PEM_read_RSA_PUBKEY(pub_file, 0, 0, 0);
  ASSERT_TRUE(pub_rsa != 0);
  fclose(pub_file);

  byte message[] = "Everyone gets Friday off.";
  const unsigned int len = strlen((char*)message);
  byte cipher[512];
  byte plain[512];

  ASSERT_TRUE(RSA_public_encrypt(len, message, cipher, pub_rsa, yaSSL::RSA_PKCS1_PADDING) == 0);
  ASSERT_TRUE(RSA_private_decrypt(priv_rsa->get_cipherLength(), cipher, plain, priv_rsa, yaSSL::RSA_PKCS1_PADDING) == 0);

  ASSERT_TRUE(memcmp(message, plain, len) == 0);

  RSA_free(priv_rsa);
  RSA_free(pub_rsa);
}

}
