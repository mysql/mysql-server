/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/auth/sha2_password_common.h" /* validate_sha256_scramble */

#include "my_dbug.h"                    /* DBUG instrumentation */
#include "my_inttypes.h"                /* typedefs */
#include "mysql_com.h"                  /* generate_sha256_scramble */
#include "sql/auth/i_sha2_password.h"   /* Internal classes */

namespace sha2_password
{
  /**
    SHA256 digest generator constructor

    Initializes digest context and sets
    status of initialization.

    If m_ok is set to false at the end,
    it indicates a problem in initialization.
  */

  SHA256_digest::SHA256_digest()
    : m_ok(false)
  {
    init();
  }


  /**
    Release acquired memory
  */

  SHA256_digest::~SHA256_digest()
  {
    deinit();
  }


  /**
    Update digest with plaintext

    @param [in] src    Plaintext to be added
    @param [in] length Length of the plaintext

    @returns digest update status
      @retval true Problem updating digest
      @retval false Success
  */

  bool SHA256_digest::update_digest(const void *src, unsigned int length)
  {
    DBUG_ENTER("SHA256_digest::update_digest");
    if (!m_ok || !src)
    {
      DBUG_PRINT("info", ("Either digest context is not ok or "
                          "source is emptry string"));
      DBUG_RETURN(true);
    }
#ifndef HAVE_YASSL
    m_ok= EVP_DigestUpdate(md_context, src, length);
#else
    md_context->Update((const TaoCrypt::byte *)src, length);
#endif // !HAVE_YASSL
    DBUG_RETURN(!m_ok);
  }


  /**
    Retrive generated digest

    @param [out] digest Digest text
    @param [in]  length Length of the digest buffer

    Assumption : memory for digest has been allocated

    @returns digest retrieval status
      @retval true Error
      @retval false Success
  */

  bool SHA256_digest::retrieve_digest(unsigned char *digest, unsigned int length)
  {
    DBUG_ENTER("SHA256_digest::retrieve_digest");
    if (!m_ok || !digest || length != CACHING_SHA2_DIGEST_LENGTH)
    {
      DBUG_PRINT("info", ("Either digest context is not ok or "
                          "digest length is not as expected."));
      DBUG_RETURN(true);
    }
#ifndef HAVE_YASSL
    m_ok= EVP_DigestFinal_ex(md_context, m_digest, NULL);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_MD_CTX_cleanup(md_context);
#else /* OPENSSL_VERSION_NUMBER < 0x10100000L */
    EVP_MD_CTX_reset(md_context);
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */
#else
    md_context->Final((TaoCrypt::byte *)m_digest);
#endif // !HAVE_YASSL
    memcpy(digest, m_digest, length);
    DBUG_RETURN(!m_ok);
  }


  /**
    Cleanup and reinit
  */

  void SHA256_digest::scrub()
  {
    deinit();
    init();
  }


  /**
    Initialize digest context

    1. Allocate memory for digest context
    2. Call initialization function(s)
  */

  void SHA256_digest::init()
  {
    DBUG_ENTER("SHA256_digest::init");
    m_ok= false;
#ifndef HAVE_YASSL
    md_context= EVP_MD_CTX_create();
    if (!md_context)
    {
      DBUG_PRINT("info", ("Failed to create digest context"));
      DBUG_VOID_RETURN;
    }

    m_ok= (bool) EVP_DigestInit_ex(md_context, EVP_sha256(), NULL);

    if (!m_ok)
    {
      EVP_MD_CTX_destroy(md_context);
      md_context= NULL;
      DBUG_PRINT("info", ("Failed to initialize digest context"));
    }
#else
    md_context= new TaoCrypt::SHA256();
    if (!md_context)
    {
      DBUG_PRINT("info", ("Failed to create digest context"));
      DBUG_VOID_RETURN;
    }

    md_context->Init();
    m_ok= true;

#endif // !HAVE_YASSL
    DBUG_VOID_RETURN;
  }


  /**
    Release allocated memory for digest context
  */

  void SHA256_digest::deinit()
  {
    if (md_context)
#ifndef HAVE_YASSL
      EVP_MD_CTX_destroy(md_context);
#else
      delete md_context;
#endif // !HAVE_YASSL
    md_context= NULL;
    m_ok= false;
  }

  /**
    Generate_scramble constructor

    @param [in] source Plaintext source
    @param [in] rnd    Salt
    @param [in] digest_type Digest type
  */
  Generate_scramble::Generate_scramble(const std::string source,
                                       const std::string rnd,
                                       Digest_info digest_type) /* = Digest_info::SHA256_DIGEST */
    : m_src(source),
      m_rnd(rnd),
      m_digest_type(digest_type)
  {
    switch (m_digest_type)
    {
      case Digest_info::SHA256_DIGEST:
      {
        m_digest_generator= new SHA256_digest();
        m_digest_length= CACHING_SHA2_DIGEST_LENGTH;
        break;
      }
     default:
        DBUG_ASSERT(false);
    };
  }


  /**
    Generate_scramble destructor
  */

  Generate_scramble::~Generate_scramble()
  {
    if (m_digest_generator)
      delete m_digest_generator;
    m_digest_generator= 0;
  }


  /**
    Scramble generation

    @param [out] scramble        Output buffer for generated scramble
    @param [in]  scramble_length Size of scramble buffer

    @note
      SHA2(src) => digest_stage1
      SHA2(digest_stage1) => digest_stage2
      SHA2(digest_stage2, m_rnd) => scramble_stage1
      XOR(digest_stage1, scramble_stage1) => scramble

    @returns Status of scramble generation
      @retval true  Error generating scramble
      @retval false Success
  */

  bool Generate_scramble::scramble(unsigned char *scramble, unsigned int scramble_length)
  {
    DBUG_ENTER("Generate_scramble::scramble");
    unsigned char *digest_stage1;
    unsigned char *digest_stage2;
    unsigned char *scramble_stage1;

    if (!scramble || scramble_length != m_digest_length)
    {
      DBUG_PRINT("info", ("Unexpected scrable length"
                          "Expected: %d, Actual: %d",
                          m_digest_length,
                          !scramble ? 0 : scramble_length));
      DBUG_RETURN(true);
    }

    switch (m_digest_type)
    {
      case Digest_info::SHA256_DIGEST:
      {
        digest_stage1= (unsigned char *)alloca(m_digest_length);
        digest_stage2= (unsigned char *)alloca(m_digest_length);
        scramble_stage1= (unsigned char *)alloca(m_digest_length);
        break;
      }
      default:
      {
        DBUG_ASSERT(false);
        DBUG_RETURN(true);
      }
    }

    /* SHA2(src) => digest_stage1 */
    if (m_digest_generator->update_digest(m_src.c_str(), m_src.length()) ||
        m_digest_generator->retrieve_digest(digest_stage1, m_digest_length))
    {
      DBUG_PRINT("info", ("Failed to generate digest_stage1: SHA2(src)"));
      DBUG_RETURN(true);
    }

    /* SHA2(digest_stage1) => digest_stage2 */
    m_digest_generator->scrub();
    if (m_digest_generator->update_digest(digest_stage1, m_digest_length) ||
        m_digest_generator->retrieve_digest(digest_stage2, m_digest_length))
    {
      DBUG_PRINT("info", ("Failed to generate digest_stage2: SHA2(digest_stage1)"));
      DBUG_RETURN(true);
    }

    /* SHA2(digest_stage2, m_rnd) => scramble_stage1 */
    m_digest_generator->scrub();
    if (m_digest_generator->update_digest(digest_stage2, m_digest_length) ||
        m_digest_generator->update_digest(m_rnd.c_str(), m_rnd.length()) ||
        m_digest_generator->retrieve_digest(scramble_stage1, m_digest_length))
    {
      DBUG_PRINT("info", ("Failed to generate scrmable_stage1: "
                          "SHA2(digest_stage2, m_rnd)"));
      DBUG_RETURN(true);
    }

    /* XOR(digest_stage1, scramble_stage1) => scramble */
    for (uint i=0; i < m_digest_length; ++i)
      scramble[i]= (digest_stage1[i] ^ scramble_stage1[i]);

    DBUG_RETURN(false);
  }


  /**
    Validate scramble constructor
    @param [in] scramble    Scramble to be validated
    @param [in] known       Known digest against which scramble is to be verified
    @param [in] rnd         Salt
    @param [in] rnd_length  Length of the salt buffer
    @param [in] digest_type Type od digest
  */

  Validate_scramble::Validate_scramble(const unsigned char *scramble,
                                       const unsigned char *known,
                                       const unsigned char *rnd,
                                       unsigned int rnd_length,
                                       Digest_info digest_type) /* = Digest_info::SHA256_DIGEST */
    : m_scramble(scramble),
      m_known(known),
      m_rnd(rnd),
      m_rnd_length(rnd_length),
      m_digest_type(digest_type)
  {
    switch (m_digest_type)
    {
      case Digest_info::SHA256_DIGEST:
      {
        m_digest_generator= new SHA256_digest();
        m_digest_length= CACHING_SHA2_DIGEST_LENGTH;
        break;
      }
      default:
        DBUG_ASSERT(false);
        break;
    };
  }


  /** Validate_scramble destructor */

  Validate_scramble::~Validate_scramble()
  {
    if (m_digest_generator)
      delete m_digest_generator;
    m_digest_generator= 0;
  }


  /**
    Validate the scramble

    @note
      SHA2(known, rnd) => scramble_stage1
      XOR(scramble, scramble_stage1) => digest_stage1
      SHA2(digest_stage1) => digest_stage2
      m_known == digest_stage2

    @returns Result of validation process
      @retval false Successful validation
      @retval true Error
  */

  bool Validate_scramble::validate()
  {
    DBUG_ENTER("Validate_scramble::validate");
    unsigned char *digest_stage1= 0;
    unsigned char *digest_stage2= 0;
    unsigned char *scramble_stage1= 0;

    switch (m_digest_type)
    {
      case Digest_info::SHA256_DIGEST:
      {
        digest_stage1= (unsigned char *)alloca(m_digest_length);
        digest_stage2= (unsigned char *)alloca(m_digest_length);
        scramble_stage1= (unsigned char *)alloca(m_digest_length);
        break;
      }
      default:
      {
        DBUG_ASSERT(false);
        DBUG_RETURN(true);
      }
    }

    /* SHA2(known, m_rnd) => scramble_stage1 */
    if (m_digest_generator->update_digest(m_known, m_digest_length) ||
        m_digest_generator->update_digest(m_rnd, m_rnd_length) ||
        m_digest_generator->retrieve_digest(scramble_stage1, m_digest_length))
    {
      DBUG_PRINT("info", ("Failed to generate scramble_stage1: SHA2(known, m_rnd)"));
      DBUG_RETURN(true);
    }

    /* XOR(scramble, scramble_stage1) => digest_stage1 */
    for (unsigned int i= 0; i < m_digest_length; ++i)
      digest_stage1[i]= (m_scramble[i] ^ scramble_stage1[i]);

    /* SHA2(digest_stage1) => digest_stage2 */
    m_digest_generator->scrub();
    if (m_digest_generator->update_digest(digest_stage1, m_digest_length) ||
        m_digest_generator->retrieve_digest(digest_stage2, m_digest_length))
    {
      DBUG_PRINT("info", ("Failed to generate digest_stage2: SHA2(digest_stage1)"));
      DBUG_RETURN(true);
    }

    /* m_known == digest_stage2 */
    if (memcmp(m_known, digest_stage2, m_digest_length) == 0)
      DBUG_RETURN(false);

    DBUG_RETURN(true);
  }
} // sha2_password


/*
  Generate scramble from password and random number.

  @param [out] scramble     Buffer to put generated scramble
  @param [in] scramble_size Size of the output buffer
  @param [in] src           Source text buffer
  @param [in] src_size      Source text buffer size
  @param [in] rnd           Random text buffer
  @param [in] rnd_size      Random text buffer size

  @note
    SHA2(src) => X
    SHA2(X) => Y
    SHA2(XOR(rnd, Y) => Z
    XOR(X, Z) => scramble

  @returns Status of scramble generation
    @retval true  Error
    @retval false Generation successful

*/

bool generate_sha256_scramble(unsigned char *scramble, size_t scramble_size,
                              const char *src, size_t src_size,
                              const char *rnd, size_t rnd_size)
{
  DBUG_ENTER("generate_scramble");
  std::string source(src, src_size);
  std::string random(rnd, rnd_size);

  sha2_password::Generate_scramble scramble_generator(source, random);
  if (scramble_generator.scramble(scramble, scramble_size))
  {
    DBUG_PRINT("info", ("Failed to generate SHA256 based scramble"));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}


/*
  Validate scramble against known text

  @param [in] scramble      Buffer with scramble to be checked
  @param [in] scramble_size Size of scramble buffer
  @param [in] known         Buffer with known text to compare against
  @param [in] known_size    Size of know text buffer
  @param [in] rnd           Buffer with random text
  @param [in] rnd_size      Size of random text buffer

  @note
    XOR(SHA2(secret), SHA2(XOR(rnd, SHA2(SHA2(secret))))) => scramble
    SHA2(SHA2(secret)) => known

    Validation:
    scramble is: XOR(SHA2(secret1), SHA2(XOR(rnd, SHA2(SHA2(secret1)))))
    known is: SHA2(SHA2(secret2))
    Our aim is to check secret1 == secret2
    - From known and rnd we generate: SHA2(XOR(rnd, scramble))
      Let's call it X
    - We then do : XOR(X, scramble) => Let's call this Y
      If secret1 == secret2, this should give us SHA2(secret1)
    - We then do SHA2(Y).
      If secret1 == secret2, this should give us SHA2(SHA2(secret1))
    - If SHA(Y) == known, then we have established that
      secret1 == secret2

  @returns status of validation
    @retval true  scramble does not match known text
    @retval false scramble matches known text

*/

bool validate_sha256_scramble(const unsigned char *scramble,
                              size_t scramble_size MY_ATTRIBUTE((unused)),
                              const unsigned char *known,
                              size_t known_size MY_ATTRIBUTE((unused)),
                              const unsigned char *rnd, size_t rnd_size)
{
  DBUG_ENTER("validate_scramble");

  sha2_password::Validate_scramble scramble_validator(scramble,
                                                      known,
                                                      rnd,
                                                      rnd_size);
  DBUG_RETURN(scramble_validator.validate());
}

