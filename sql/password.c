/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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

  Example:
    update user set password=PASSWORD("hello") where user="test"
  This saves a hashed number as a string in the password field.


  New in MySQL 4.1 authentication works even more secure way.
  At the first step client sends user name to the sever, and password if
  it is empty. So in case of empty password authentication is as fast as before.
  At the second stap servers sends scramble to client, which is encoded with
  password stage2 hash stored in the password  database as well as salt, needed
  for client to build stage2 password to decrypt scramble.
  Client decrypts the scramble and encrypts it once again with stage1 password.
  This information is sent to server.
  Server decrypts the scramble to get stage1 password and hashes it to get
  stage2 hash. This hash is when compared to hash stored in the database.

  This authentication needs 2 packet round trips instead of one but it is much
  stronger. Now if one will steal mysql database content he will not be able
  to break into MySQL.

  New Password handling functions by Peter Zaitsev


*****************************************************************************/

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <sha1.h>
#include "mysql.h"



/* Character to use as version identifier for version 4.1 */
#define PVERSION41_CHAR '*'

/* Scramble length for new password version */


/*
  New (MySQL 3.21+) random generation structure initialization

  SYNOPSIS
    randominit()
    rand_st    OUT  Structure to initialize
    seed1      IN   First initialization parameter
    seed2      IN   Second initialization parameter

  RETURN
    none
*/

void randominit(struct rand_struct *rand_st,ulong seed1, ulong seed2)
{						/* For mysql 3.21.# */
#ifdef HAVE_purify
  bzero((char*) rand_st,sizeof(*rand_st));	/* Avoid UMC varnings */
#endif
  rand_st->max_value= 0x3FFFFFFFL;
  rand_st->max_value_dbl=(double) rand_st->max_value;
  rand_st->seed1=seed1%rand_st->max_value ;
  rand_st->seed2=seed2%rand_st->max_value;
}


/*
  Old (MySQL 3.20) random generation structure initialization

  SYNOPSIS
    old_randominit()
    rand_st    OUT  Structure to initialize
    seed1      IN   First initialization parameter

  RETURN
    none
*/

static void old_randominit(struct rand_struct *rand_st,ulong seed1)
{						/* For mysql 3.20.# */
  rand_st->max_value= 0x01FFFFFFL;
  rand_st->max_value_dbl=(double) rand_st->max_value;
  seed1%=rand_st->max_value;
  rand_st->seed1=seed1 ; rand_st->seed2=seed1/2;
}


/*
  Generate Random number

  SYNOPSIS
    rnd()
    rand_st    INOUT  Structure used for number generation

  RETURN
    Generated pseudo random number
*/

double my_rnd(struct rand_struct *rand_st)
{
  rand_st->seed1=(rand_st->seed1*3+rand_st->seed2) % rand_st->max_value;
  rand_st->seed2=(rand_st->seed1+rand_st->seed2+33) % rand_st->max_value;
  return (((double) rand_st->seed1)/rand_st->max_value_dbl);
}


/*
  Generate String of printable random characters of requested length
  String will not be zero terminated.

  SYNOPSIS
    create_random_string()
    length     IN     Lenght of
    rand_st    INOUT  Structure used for number generation
    target     OUT    Buffer for generation

  RETURN
    none
*/

void create_random_string(int length,struct rand_struct *rand_st,char *target)
{
  char *end=target+length;
  /* Use pointer arithmetics as it is faster way to do so. */
  for (; target<end ; target++)
    *target= (char) (rnd(rand_st)*94+33);
}


/*
  Encrypt/Decrypt function used for password encryption in authentication
  Simple XOR is used here but it is OK as we crypt random strings

  SYNOPSIS
    password_crypt()
    from     IN     Data for encryption
    to       OUT    Encrypt data to the buffer (may be the same)
    password IN     Password used for encryption (same length)
    length   IN     Length of data to encrypt

  RETURN
    none
*/

void password_crypt(const char *from,char *to, const char *password,int length)
{
 const char *from_end=from+length;

 while (from < from_end)
   *to++= *(from++) ^* (password++);
}


/*
  Generate binary hash from raw text password
  Used for Pre-4.1 Password handling

  SYNOPSIS
    hash_pasword()
    result   OUT    Store hash in this location
    password IN     Plain text password to build hash

  RETURN
    none
*/

void hash_password(ulong *result, const char *password)
{
  register ulong nr=1345345333L, add=7, nr2=0x12345671L;
  ulong tmp;
  for (; *password ; password++)
  {
    if (*password == ' ' || *password == '\t')
      continue;			/* skipp space in password */
    tmp= (ulong) (uchar) *password;
    nr^= (((nr & 63)+add)*tmp)+ (nr << 8);
    nr2+=(nr2 << 8) ^ nr;
    add+=tmp;
  }
  result[0]=nr & (((ulong) 1L << 31) -1L); /* Don't use sign bit (str2int) */;
  result[1]=nr2 & (((ulong) 1L << 31) -1L);
  return;
}


/*
  Stage one password hashing.
  Used in MySQL 4.1 password handling

  SYNOPSIS
    password_hash_stage1()
    to       OUT    Store stage one hash to this location
    password IN     Plain text password to build hash

  RETURN
    none
*/

void password_hash_stage1(char *to, const char *password)
{
  SHA1_CONTEXT context;
  sha1_reset(&context);
  for (; *password ; password++)
  {
    if (*password == ' ' || *password == '\t')
      continue;/* skip space in password */
    sha1_input(&context,(uint8*) &password[0],1);
  }
  sha1_result(&context,(uint8*)to);
}


/*
  Stage two password hashing.
  Used in MySQL 4.1 password handling

  SYNOPSIS
    password_hash_stage2()
    to       INOUT  Use this as stage one hash and store stage two hash here
    salt     IN     Salt used for stage two hashing

  RETURN
    none
*/

void password_hash_stage2(char *to, const char *salt)
{
  SHA1_CONTEXT context;
  sha1_reset(&context);
  sha1_input(&context,(uint8*) salt, 4);
  sha1_input(&context,(uint8*) to, SHA1_HASH_SIZE);
  sha1_result(&context,(uint8*) to);
}


/*
  Create password to be stored in user database from raw string
  Handles both MySQL 4.1 and Pre-MySQL 4.1 passwords

  SYNOPSIS
    make_scramble_password()
    to       OUT   Store scrambled password here
    password IN    Raw string password
    force_old_scramle
             IN    Force generation of old scramble variant
    rand_st  INOUT Structure for temporary number generation.
  RETURN
    none
*/

void make_scrambled_password(char *to,const char *password,
                             my_bool force_old_scramble,
                             struct rand_struct *rand_st)
{
  ulong hash_res[2];   /* Used for pre 4.1 password hashing */
  unsigned short salt; /* Salt for 4.1 version password */
  uint8 digest[SHA1_HASH_SIZE];
  if (force_old_scramble) /* Pre 4.1 password encryption */
  {
    hash_password(hash_res,password);
    sprintf(to,"%08lx%08lx",hash_res[0],hash_res[1]);
  }
  else /* New password 4.1 password scrambling */
  {
    to[0]=PVERSION41_CHAR; /* New passwords have version prefix */
   /* Rnd returns number from 0 to 1 so this would be good salt generation.*/
    salt=(unsigned short) (rnd(rand_st)*65535+1);
    /* Use only 2 first bytes from it */
    sprintf(to+1,"%04x",salt);
    /* First hasing is done without salt */
    password_hash_stage1((char*) digest, password);
    /* Second stage is done with salt */
    password_hash_stage2((char*) digest,(char*)to+1),
    /* Print resulting hash into the password*/
    sprintf(to+5,
      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      digest[0],digest[1],digest[2],digest[3],digest[4],digest[5],digest[6],
      digest[7],digest[8],digest[9],digest[10],digest[11],digest[12],digest[13],
      digest[14],digest[15],digest[16],digest[17],digest[18],digest[19]);
  }
}


/*
  Convert password from binary string form to salt form
  Used for MySQL 4.1 password handling

  SYNOPSIS
    get_salt_from_bin_password()
    res      OUT Store salt form password here
    password IN  Binary password to be converted
    salt     IN  hashing-salt to be used for salt form generation

  RETURN
    none
*/

void get_salt_from_bin_password(ulong *res,unsigned char *password,ulong salt)
{
  unsigned char *password_end=password+SCRAMBLE41_LENGTH;
  *res=salt;
  res++;

  /* Process password of known length*/
  while (password<password_end)
  {
    ulong val=0;
    uint i;
    for (i=0 ; i < 4 ; i++)
       val=(val << 8)+(*password++);
    *res++=val;
  }
}


/*
  Validate password for MySQL 4.1 password handling.

  SYNOPSIS
    validate_password()
    password IN   Encrypted Scramble which we got from the client
    message  IN   Original scramble which we have sent to the client before
    salt     IN   Password in the salted form to match to

  RETURN
    0 for correct password
   !0 for invalid password
*/

my_bool validate_password(const char *password, const char *message,
                          ulong *salt)
{
  char buffer[SCRAMBLE41_LENGTH]; /* Used for password validation */
  char tmpsalt[8]; /* Temporary value to convert salt to string form */
  ulong salt_candidate[6]; /* Computed candidate salt */
  ulong *sc=salt_candidate; /* we need to be able to increment */
  ulong *salt_end;

  /* Now we shall get stage1 encrypted password in buffer*/
  password_crypt(password,buffer,message,SCRAMBLE41_LENGTH);

  /* For compatibility reasons we use ulong to store salt while we need char */
  sprintf(tmpsalt,"%04x",(unsigned short)salt[0]);

  password_hash_stage2(buffer,tmpsalt);
  /* Convert password to salt to compare */
  get_salt_from_bin_password(salt_candidate,(uchar*) buffer,salt[0]);

  /* Now we shall get exactly the same password as we have stored for user  */
  for (salt_end=salt+5 ; salt < salt_end; )
    if (*++salt != *++sc)
      return 1;

  /* Or password correct*/
  return 0;
}


/*
  Get length of password string which is stored in mysql.user table

  SYNOPSIS
    get_password_length()
    force_old_scramble  IN  If we wish to use pre 4.1 scramble format

  RETURN
    password length >0
*/

int get_password_length(my_bool force_old_scramble)
{
  return (force_old_scramble) ? 16 : SHA1_HASH_SIZE*2+4+1;
}


/*
  Get version of the password based on mysql.user password string

  SYNOPSIS
    get_password_version()
    password IN   Password string as stored in mysql.user

  RETURN
    0 for pre 4.1 passwords
   !0 password version char for newer passwords
*/

char get_password_version(const char *password)
{
  if (password==NULL) return 0;
  if (password[0]==PVERSION41_CHAR) return PVERSION41_CHAR;
  return 0;
}


/*
  Get integer value of Hex character

  SYNOPSIS
    char_val()
    X        IN   Character to find value for

  RETURN
    Appropriate integer value
*/



static inline unsigned int char_val(char X)
{
  return (uint) (X >= '0' && X <= '9' ? X-'0' :
		 X >= 'A' && X <= 'Z' ? X-'A'+10 :
		 X-'a'+10);
}


/*
  Get Binary salt from password as in mysql.user format

  SYNOPSIS
    get_salt_from_password()
    res      OUT  Store binary salt here
    password IN   Password string as stored in mysql.user

  RETURN
    none

  NOTE
    This function does not have length check for passwords. It will just crash
    Password hashes in old format must have length divisible by 8
*/

void get_salt_from_password(ulong *res,const char *password)
{
  if (password) /* zero salt corresponds to empty password */
  {
    if (password[0]==PVERSION41_CHAR) /* if new password */
    {
      uint val=0;
      uint i;
      password++; /* skip version identifier */

      /*get hashing salt from password and store in in the start of array */
      for (i=0 ; i < 4 ; i++)
	val=(val << 4)+char_val(*password++);
      *res++=val;
    }
     /* We process old passwords the same way as new ones in other case */
#ifdef EXTRA_DEBUG
    if (strlen(password)%8!=0)
      fprintf(stderr,"Warning: Incorrect password length for salting: %d\n",
              strlen(password));
#endif
    while (*password)
    {
      ulong val=0;
      uint i;
      for (i=0 ; i < 8 ; i++)
	val=(val << 4)+char_val(*password++);
      *res++=val;
    }
  }
  return;
}


/*
  Get string version as stored in mysql.user from salt form

  SYNOPSIS
    make_password_from_salt()
    to       OUT  Store resulting string password here
    hash_res IN   Password in salt format
    password_version
             IN   According to which version salt should be treated

  RETURN
    none
*/

void make_password_from_salt(char *to, ulong *hash_res,uint8 password_version)
{
  if (!password_version) /* Handling of old passwords. */
    sprintf(to,"%08lx%08lx",hash_res[0],hash_res[1]);
  else
    if (password_version==PVERSION41_CHAR)
      sprintf(to,"%c%04x%08lx%08lx%08lx%08lx%08lx",PVERSION41_CHAR,(unsigned short)hash_res[0],hash_res[1],
              hash_res[2],hash_res[3],hash_res[4],hash_res[5]);
    else /* Just use empty password if we can't handle it. This should not happen */
      to[0]='\0';
}


/*
  Convert password in salted form to binary string password and hash-salt
  For old password this involes one more hashing

  SYNOPSIS
    get_hash_and_password()
    salt         IN  Salt to convert from
    pversion     IN  Password version to use
    hash         OUT Store zero ended hash here
    bin_password OUT Store binary password here (no zero at the end)

  RETURN
    0 for pre 4.1 passwords
   !0 password version char for newer passwords
*/

void get_hash_and_password(ulong *salt, uint8 pversion, char *hash,
			   unsigned char *bin_password)
{
  int t;
  ulong* salt_end;
  ulong val;
  SHA1_CONTEXT context;

  if (pversion)				/* New password version assumed */
  {
    salt_end=salt+5;
    sprintf(hash,"%04x",(unsigned short)salt[0]);
    while (salt<salt_end)
    {
      val=*(++salt);
      for (t=3; t>=0; t--)
      {
        bin_password[t]= (char) (val & 255);
        val>>=8;			/* Scroll 8 bits to get next part*/
      }
      bin_password+=4;			/* Get to next 4 chars*/
    }
  }
  else
  {
    unsigned char *bp= bin_password;	/* Binary password loop pointer */

    /* Use zero starting hash as an indication of old password */
    hash[0]=0;
    salt_end=salt+2;
    /* Encode salt using SHA1 here */
    sha1_reset(&context);
    while (salt<salt_end)		/* Iterate over these elements*/
    {
      val= *salt;
      for (t=3;t>=0;t--)
      {
        bp[t]= (uchar) (val & 255);
        val>>=8;			/* Scroll 8 bits to get next part*/
      }
      bp+= 4;				/* Get to next 4 chars*/
      salt++;
    }
    /* Use 8 bytes of binary password for hash */
    sha1_input(&context,(uint8*)bin_password,8);
    sha1_result(&context,(uint8*)bin_password);
  }
}


/*
  Create key from old password to decode scramble
  Used in 4.1 authentication with passwords stored old way

  SYNOPSIS
    create_key_from_old_password()
    passwd    IN  Password used for key generation
    key       OUT Created 20 bytes key

  RETURN
    None
*/


void create_key_from_old_password(const char *passwd, char *key)
{
  char  buffer[SCRAMBLE41_LENGTH]; /* Buffer for various needs */
  ulong salt[6];    /* Salt (large for safety) */
  /* At first hash password to the string stored in password */
  make_scrambled_password(buffer,passwd,1,(struct rand_struct *)NULL);
  /* Now convert it to the salt form */
  get_salt_from_password(salt,buffer);
  /* Finally get hash and bin password from salt */
  get_hash_and_password(salt,0,buffer,(unsigned char*) key);
}


/*
  Scramble string with password
  Used at pre 4.1 authentication phase.

  SYNOPSIS
    scramble()
    to        OUT Store scrambled message here
    message   IN  Message to scramble
    password  IN  Password to use while scrambling
    old_ver   IN  Forse old version random number generator

  RETURN
    End of scrambled string
*/

char *scramble(char *to,const char *message,const char *password,
	       my_bool old_ver)
{
  struct rand_struct rand_st;
  ulong hash_pass[2],hash_message[2];
  char message_buffer[9]; /* Real message buffer */
  char *msg=message_buffer;

  /* We use special message buffer now as new server can provide longer hash */

  memcpy(message_buffer,message,8);
  message_buffer[8]=0;

  if (password && password[0])
  {
    char *to_start=to;
    hash_password(hash_pass,password);
    hash_password(hash_message,message_buffer);
    if (old_ver)
      old_randominit(&rand_st,hash_pass[0] ^ hash_message[0]);
    else
      randominit(&rand_st,hash_pass[0] ^ hash_message[0],
		 hash_pass[1] ^ hash_message[1]);
    while (*msg++)
      *to++= (char) (floor(my_rnd(&rand_st)*31)+64);
    if (!old_ver)
    {						/* Make it harder to break */
      char extra=(char) (floor(my_rnd(&rand_st)*31));
      while (to_start != to)
	*(to_start++)^=extra;
    }
  }
  *to=0;
  return to;
}


/*
  Check scrambled message
  Used for pre 4.1 password handling

  SYNOPSIS
    scramble()
    scrambled IN  Scrambled message to check
    message   IN  Original message which was scramble
    hash_pass IN  Password which should be used for scrambling
    old_ver   IN  Forse old version random number generator

  RETURN
    0  Password correct
   !0  Password invalid
*/

my_bool check_scramble(const char *scrambled, const char *message,
		       ulong *hash_pass, my_bool old_ver)
{
  struct rand_struct rand_st;
  ulong hash_message[2];
  char buff[16],*to,extra;		   /* Big enough for check */
  const char *pos;
  char message_buffer[SCRAMBLE_LENGTH+1];  /* Copy of message */
  
  /* We need to copy the message as this function can be called for MySQL 4.1
     scramble which is not zero ended and can have zeroes inside
     We could just write zero to proper place in original message but
     this would make it harder to understand code for next generations
  */      

  memcpy(message_buffer,message,SCRAMBLE_LENGTH); /* Ignore the rest */
  message_buffer[SCRAMBLE_LENGTH]=0;
  
  /* Check if this exactly N bytes. Overwise this is something fishy */
  if (strlen(message_buffer)!=SCRAMBLE_LENGTH)
    return 1; /* Wrong password */

  hash_password(hash_message,message_buffer);
  if (old_ver)
    old_randominit(&rand_st,hash_pass[0] ^ hash_message[0]);
  else
    randominit(&rand_st,hash_pass[0] ^ hash_message[0],
	       hash_pass[1] ^ hash_message[1]);
  to=buff;
  for (pos=scrambled ; *pos ; pos++)
    *to++=(char) (floor(my_rnd(&rand_st)*31)+64);
  if (old_ver)
    extra=0;
  else
    extra=(char) (floor(my_rnd(&rand_st)*31));
  to=buff;
  while (*scrambled)
  {
    if (*scrambled++ != (char) (*to++ ^ extra))
      return 1;					/* Wrong password */
  }
  return 0;
}
