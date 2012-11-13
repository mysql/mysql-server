/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* password checking routines */
/*****************************************************************************
  The main idea is that no password are sent between client & server on
  connection and that no password are saved in mysql in a decodable form.

  On connection a random string is generated and sent to the client.
  The client generates a new string with a random generator inited with
  the hash values from the password and the sent string.
  This 'check' string is sent to the server where it is compared with
  a string generated from the stored hash_value of the password and the
  random string.

  The password is saved (in user.password) by using the PASSWORD() function in
  mysql.

  This is .c file because it's used in libmysqlclient, which is entirely in C.
  (we need it to be portable to a variety of systems).
  Example:
    update user set password=PASSWORD("hello") where user="test"
  This saves a hashed number as a string in the password field.

  The new authentication is performed in following manner:

  SERVER:  public_seed=create_random_string()
           send(public_seed)

  CLIENT:  recv(public_seed)
           hash_stage1=sha1("password")
           hash_stage2=sha1(hash_stage1)
           reply=xor(hash_stage1, sha1(public_seed,hash_stage2)

           // this three steps are done in scramble() 

           send(reply)

     
  SERVER:  recv(reply)
           hash_stage1=xor(reply, sha1(public_seed,hash_stage2))
           candidate_hash2=sha1(hash_stage1)
           check(candidate_hash2==hash_stage2)

           // this three steps are done in check_scramble()

*****************************************************************************/

#include <password.h>
#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <sha1.h>
#include <my_rnd.h>
#include "mysql.h"
#include "crypt_genhash_impl.h"

/************ MySQL 3.23-4.0 authentication routines: untouched ***********/

/*
  New (MySQL 3.21+) random generation structure initialization
  SYNOPSIS
    randominit()
    rand_st    OUT  Structure to initialize
    seed1      IN   First initialization parameter
    seed2      IN   Second initialization parameter
*/

void randominit(struct rand_struct *rand_st, ulong seed1, ulong seed2)
{                                               /* For mysql 3.21.# */
#ifdef HAVE_purify
  memset(rand_st, 0, sizeof(*rand_st));       /* Avoid UMC varnings */
#endif
  rand_st->max_value= 0x3FFFFFFFL;
  rand_st->max_value_dbl=(double) rand_st->max_value;
  rand_st->seed1=seed1%rand_st->max_value ;
  rand_st->seed2=seed2%rand_st->max_value;
}

/*
    Generate binary hash from raw text string 
    Used for Pre-4.1 password handling
  SYNOPSIS
    hash_password()
    result       OUT store hash in this location
    password     IN  plain text password to build hash
    password_len IN  password length (password may be not null-terminated)
*/

void hash_password(ulong *result, const char *password, uint password_len)
{
  register ulong nr=1345345333L, add=7, nr2=0x12345671L;
  ulong tmp;
  const char *password_end= password + password_len;
  for (; password < password_end; password++)
  {
    if (*password == ' ' || *password == '\t')
      continue;                                 /* skip space in password */
    tmp= (ulong) (uchar) *password;
    nr^= (((nr & 63)+add)*tmp)+ (nr << 8);
    nr2+=(nr2 << 8) ^ nr;
    add+=tmp;
  }
  result[0]=nr & (((ulong) 1L << 31) -1L); /* Don't use sign bit (str2int) */;
  result[1]=nr2 & (((ulong) 1L << 31) -1L);
}


/*
    Create password to be stored in user database from raw string
    Used for pre-4.1 password handling
  SYNOPSIS
    my_make_scrambled_password_323()
    to        OUT store scrambled password here
    password  IN  user-supplied password
    pass_len  IN  length of password string
*/

void my_make_scrambled_password_323(char *to, const char *password,
                                    size_t pass_len)
{
  ulong hash_res[2];
  hash_password(hash_res, password, (uint) pass_len);
  sprintf(to, "%08lx%08lx", hash_res[0], hash_res[1]);
}


/*
  Wrapper around my_make_scrambled_password_323() to maintain client lib ABI
  compatibility.
  In server code usage of my_make_scrambled_password_323() is preferred to
  avoid strlen().
  SYNOPSIS
    make_scrambled_password_323()
    to        OUT store scrambled password here
    password  IN  NULL-terminated string with user-supplied password
*/

void make_scrambled_password_323(char *to, const char *password)
{
  my_make_scrambled_password_323(to, password, strlen(password));
}


/*
    Scramble string with password.
    Used in pre 4.1 authentication phase.
  SYNOPSIS
    scramble_323()
    to       OUT Store scrambled message here. Buffer must be at least
                 SCRAMBLE_LENGTH_323+1 bytes long
    message  IN  Message to scramble. Message must be at least
                 SRAMBLE_LENGTH_323 bytes long.
    password IN  Password to use while scrambling
*/

void scramble_323(char *to, const char *message, const char *password)
{
  struct rand_struct rand_st;
  ulong hash_pass[2], hash_message[2];

  if (password && password[0])
  {
    char extra, *to_start=to;
    const char *message_end= message + SCRAMBLE_LENGTH_323;
    hash_password(hash_pass,password, (uint) strlen(password));
    hash_password(hash_message, message, SCRAMBLE_LENGTH_323);
    randominit(&rand_st,hash_pass[0] ^ hash_message[0],
               hash_pass[1] ^ hash_message[1]);
    for (; message < message_end; message++)
      *to++= (char) (floor(my_rnd(&rand_st)*31)+64);
    extra=(char) (floor(my_rnd(&rand_st)*31));
    while (to_start != to)
      *(to_start++)^=extra;
  }
  *to= 0;
}


/**
  Check scrambled message. Used in pre 4.1 password handling.

  @param scrambled  Scrambled message to check.
  @param message    Original random message which was used for scrambling.
  @param hash_pass  Password which should be used for scrambling.

  @remark scrambled and message must be SCRAMBLED_LENGTH_323 bytes long.

  @return FALSE if password is correct, TRUE otherwise.
*/

my_bool
check_scramble_323(const unsigned char *scrambled, const char *message,
                   ulong *hash_pass)
{
  struct rand_struct rand_st;
  ulong hash_message[2];
  /* Big enough for checks. */
  uchar buff[16], scrambled_buff[SCRAMBLE_LENGTH_323 + 1];
  uchar *to, extra;
  const uchar *pos;

  /* Ensure that the scrambled message is null-terminated. */
  memcpy(scrambled_buff, scrambled, SCRAMBLE_LENGTH_323);
  scrambled_buff[SCRAMBLE_LENGTH_323]= '\0';
  scrambled= scrambled_buff;

  hash_password(hash_message, message, SCRAMBLE_LENGTH_323);
  randominit(&rand_st,hash_pass[0] ^ hash_message[0],
             hash_pass[1] ^ hash_message[1]);
  to=buff;
  DBUG_ASSERT(sizeof(buff) > SCRAMBLE_LENGTH_323);
  for (pos=scrambled ; *pos && to < buff+sizeof(buff) ; pos++)
    *to++=(char) (floor(my_rnd(&rand_st)*31)+64);
  if (pos-scrambled != SCRAMBLE_LENGTH_323)
    return 1;
  extra=(char) (floor(my_rnd(&rand_st)*31));
  to=buff;
  while (*scrambled)
  {
    if (*scrambled++ != (uchar) (*to++ ^ extra))
      return 1;                                 /* Wrong password */
  }
  return 0;
}

static inline uint8 char_val(uint8 X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
      X >= 'A' && X <= 'Z' ? X-'A'+10 : X-'a'+10);
}


/*
    Convert password from hex string (as stored in mysql.user) to binary form.
  SYNOPSIS
    get_salt_from_password_323()
    res       OUT store salt here 
    password  IN  password string as stored in mysql.user
  NOTE
    This function does not have length check for passwords. It will just crash
    Password hashes in old format must have length divisible by 8
*/

void get_salt_from_password_323(ulong *res, const char *password)
{
  res[0]= res[1]= 0;
  if (password)
  {
    while (*password)
    {
      ulong val=0;
      uint i;
      for (i=0 ; i < 8 ; i++)
        val=(val << 4)+char_val(*password++);
      *res++=val;
    }
  }
}


/*
    Convert scrambled password from binary form to asciiz hex string.
  SYNOPSIS
    make_password_from_salt_323()
    to    OUT store resulting string password here, at least 17 bytes 
    salt  IN  password in salt format, 2 ulongs 
*/

void make_password_from_salt_323(char *to, const ulong *salt)
{
  sprintf(to,"%08lx%08lx", salt[0], salt[1]);
}


/*
     **************** MySQL 4.1.1 authentication routines *************
*/

/**
  Generate string of printable pseudo random characters of requested length.
  
  @param to[out]    Buffer for generation; must be at least length+1 bytes
                    long; result string is always null-terminated
  @param length[in] How many random characters to put in buffer
  @param rand_st    Structure used for number generation
    
  @note This function is restricted for use with
    native_password_authenticate() because of security reasons.
      
  DON'T RELY ON THIS FUNCTION FOR A UNIFORMLY DISTRIBUTION OF BITS!
  
*/

void create_random_string(char *to, uint length, struct rand_struct *rand_st)
{
  char *end= to + length;
  /*
    Warning: my_rnd() is a fast prng, but it doesn't necessarily have a uniform
    distribution.
  */
  for (; to < end; to++)
    *to= (char) (my_rnd(rand_st) * 94 + 33);
  *to= '\0';
}


/* Character to use as version identifier for version 4.1 */

#define PVERSION41_CHAR '*'


/*
    Convert given octet sequence to asciiz string of hex characters;
    str..str+len and 'to' may not overlap.
  SYNOPSIS
    octet2hex()
    buf       OUT output buffer. Must be at least 2*len+1 bytes
    str, len  IN  the beginning and the length of the input string

  RETURN
    buf+len*2
*/

char *octet2hex(char *to, const char *str, uint len)
{
  const char *str_end= str + len; 
  for (; str != str_end; ++str)
  {
    *to++= _dig_vec_upper[((uchar) *str) >> 4];
    *to++= _dig_vec_upper[((uchar) *str) & 0x0F];
  }
  *to= '\0';
  return to;
}


/*
    Convert given asciiz string of hex (0..9 a..f) characters to octet
    sequence.
  SYNOPSIS
    hex2octet()
    to        OUT buffer to place result; must be at least len/2 bytes
    str, len  IN  begin, length for character string; str and to may not
                  overlap; len % 2 == 0
*/ 

static void
hex2octet(uint8 *to, const char *str, uint len)
{
  const char *str_end= str + len;
  while (str < str_end)
  {
    register char tmp= char_val(*str++);
    *to++= (tmp << 4) | char_val(*str++);
  }
}


/*
    Encrypt/Decrypt function used for password encryption in authentication.
    Simple XOR is used here but it is OK as we crypt random strings. Note,
    that XOR(s1, XOR(s1, s2)) == s2, XOR(s1, s2) == XOR(s2, s1)
  SYNOPSIS
    my_crypt()
    to      OUT buffer to hold crypted string; must be at least len bytes
                long; to and s1 (or s2) may be the same.
    s1, s2  IN  input strings (of equal length)
    len     IN  length of s1 and s2
*/

static void
my_crypt(char *to, const uchar *s1, const uchar *s2, uint len)
{
  const uint8 *s1_end= s1 + len;
  while (s1 < s1_end)
    *to++= *s1++ ^ *s2++;
}

#if defined(HAVE_OPENSSL)
void my_make_scrambled_password(char *to, const char *password,
                                size_t pass_len)
{

  char salt[CRYPT_SALT_LENGTH + 1];
  
  generate_user_salt(salt, CRYPT_SALT_LENGTH + 1);
  my_crypt_genhash(to,
                     CRYPT_MAX_PASSWORD_SIZE,
                     password,
                     pass_len,
                     salt,
                     0);

}
#endif
/**
  Compute two stage SHA1 hash of the password :

    hash_stage1=sha1("password")
    hash_stage2=sha1(hash_stage1)

  @param password    [IN]   Password string.
  @param pass_len    [IN]   Length of the password.
  @param hash_stage1 [OUT]  sha1(password)
  @param hash_stage2 [OUT]  sha1(hash_stage1)
*/

inline static
void compute_two_stage_sha1_hash(const char *password, size_t pass_len,
                                 uint8 *hash_stage1, uint8 *hash_stage2)
{
  /* Stage 1: hash password */
  compute_sha1_hash(hash_stage1, password, pass_len);

  /* Stage 2 : hash first stage's output. */
  compute_sha1_hash(hash_stage2, (const char *) hash_stage1, SHA1_HASH_SIZE);
}


/*
    MySQL 4.1.1 password hashing: SHA conversion (see RFC 2289, 3174) twice
    applied to the password string, and then produced octet sequence is
    converted to hex string.
    The result of this function is used as return value from PASSWORD() and
    is stored in the database.
  SYNOPSIS
    my_make_scrambled_password_sha1()
    buf       OUT buffer of size 2*SHA1_HASH_SIZE + 2 to store hex string
    password  IN  password string
    pass_len  IN  length of password string
*/

void my_make_scrambled_password_sha1(char *to, const char *password,
                                     size_t pass_len)
{
  uint8 hash_stage2[SHA1_HASH_SIZE];

  /* Two stage SHA1 hash of the password. */
  compute_two_stage_sha1_hash(password, pass_len, (uint8 *) to, hash_stage2);

  /* convert hash_stage2 to hex string */
  *to++= PVERSION41_CHAR;
  octet2hex(to, (const char*) hash_stage2, SHA1_HASH_SIZE);
}
  

/*
  Wrapper around my_make_scrambled_password() to maintain client lib ABI
  compatibility.
  In server code usage of my_make_scrambled_password() is preferred to
  avoid strlen().
  SYNOPSIS
    make_scrambled_password()
    buf       OUT buffer of size 2*SHA1_HASH_SIZE + 2 to store hex string
    password  IN  NULL-terminated password string
*/

void make_scrambled_password(char *to, const char *password)
{
  my_make_scrambled_password_sha1(to, password, strlen(password));
}


/*
    Produce an obscure octet sequence from password and random
    string, recieved from the server. This sequence corresponds to the
    password, but password can not be easily restored from it. The sequence
    is then sent to the server for validation. Trailing zero is not stored
    in the buf as it is not needed.
    This function is used by client to create authenticated reply to the
    server's greeting.
  SYNOPSIS
    scramble()
    buf       OUT store scrambled string here. The buf must be at least 
                  SHA1_HASH_SIZE bytes long. 
    message   IN  random message, must be exactly SCRAMBLE_LENGTH long and 
                  NULL-terminated.
    password  IN  users' password 
*/

void
scramble(char *to, const char *message, const char *password)
{
  uint8 hash_stage1[SHA1_HASH_SIZE];
  uint8 hash_stage2[SHA1_HASH_SIZE];

  /* Two stage SHA1 hash of the password. */
  compute_two_stage_sha1_hash(password, strlen(password), hash_stage1,
                              hash_stage2);

  /* create crypt string as sha1(message, hash_stage2) */;
  compute_sha1_hash_multi((uint8 *) to, message, SCRAMBLE_LENGTH,
                          (const char *) hash_stage2, SHA1_HASH_SIZE);
  my_crypt(to, (const uchar *) to, hash_stage1, SCRAMBLE_LENGTH);
}


/*
    Check that scrambled message corresponds to the password; the function
    is used by server to check that recieved reply is authentic.
    This function does not check lengths of given strings: message must be
    null-terminated, reply and hash_stage2 must be at least SHA1_HASH_SIZE
    long (if not, something fishy is going on).
  SYNOPSIS
    check_scramble_sha1()
    scramble     clients' reply, presumably produced by scramble()
    message      original random string, previously sent to client
                 (presumably second argument of scramble()), must be 
                 exactly SCRAMBLE_LENGTH long and NULL-terminated.
    hash_stage2  hex2octet-decoded database entry
    All params are IN.

  RETURN VALUE
    0  password is correct
    !0  password is invalid
*/

my_bool
check_scramble_sha1(const uchar *scramble_arg, const char *message,
                    const uint8 *hash_stage2)
{
  uint8 buf[SHA1_HASH_SIZE];
  uint8 hash_stage2_reassured[SHA1_HASH_SIZE];

  /* create key to encrypt scramble */
  compute_sha1_hash_multi(buf, message, SCRAMBLE_LENGTH,
                          (const char *) hash_stage2, SHA1_HASH_SIZE);
  /* encrypt scramble */
  my_crypt((char *) buf, buf, scramble_arg, SCRAMBLE_LENGTH);

  /* now buf supposedly contains hash_stage1: so we can get hash_stage2 */
  compute_sha1_hash(hash_stage2_reassured, (const char *) buf, SHA1_HASH_SIZE);

  return test(memcmp(hash_stage2, hash_stage2_reassured, SHA1_HASH_SIZE));
}

my_bool
check_scramble(const uchar *scramble_arg, const char *message,
               const uint8 *hash_stage2)
{
  return check_scramble_sha1(scramble_arg, message, hash_stage2);
}

/*
  Convert scrambled password from asciiz hex string to binary form.

  SYNOPSIS
    get_salt_from_password()
    res       OUT buf to hold password. Must be at least SHA1_HASH_SIZE
                  bytes long.
    password  IN  4.1.1 version value of user.password
*/
    
void get_salt_from_password(uint8 *hash_stage2, const char *password)
{
  hex2octet(hash_stage2, password+1 /* skip '*' */, SHA1_HASH_SIZE * 2);
}

/*
    Convert scrambled password from binary form to asciiz hex string.
  SYNOPSIS
    make_password_from_salt()
    to    OUT store resulting string here, 2*SHA1_HASH_SIZE+2 bytes 
    salt  IN  password in salt format
*/

void make_password_from_salt(char *to, const uint8 *hash_stage2)
{
  *to++= PVERSION41_CHAR;
  octet2hex(to, (const char*) hash_stage2, SHA1_HASH_SIZE);
}

