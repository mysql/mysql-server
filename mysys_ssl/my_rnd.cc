/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <my_rnd.h>

#if defined(HAVE_YASSL)

#if defined(YASSL_PREFIX)
#define RAND_bytes yaRAND_bytes
#endif /* YASSL_PREFIX */

#include <openssl/ssl.h>

#elif defined(HAVE_OPENSSL)
#include <openssl/rand.h>
#include <openssl/err.h>
#endif /* HAVE_YASSL */


/*
  A wrapper to use OpenSSL/yaSSL PRNGs.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
  Generate random number.

  @param rand_st [INOUT] Structure used for number generation.

  @retval                Generated pseudo random number.
*/

double my_rnd(struct rand_struct *rand_st)
{
  rand_st->seed1= (rand_st->seed1*3+rand_st->seed2) % rand_st->max_value;
  rand_st->seed2= (rand_st->seed1+rand_st->seed2+33) % rand_st->max_value;
  return (((double) rand_st->seed1) / rand_st->max_value_dbl);
}



/**
Fill a buffer with random bytes using the SSL library routines

@param buffer       [OUT]   Buffer to receive the random data
@param buffer_size  [IN]    sizeof the the buffer

@retval      1  error ocurred.
@retval      0  OK
*/
int
my_rand_buffer(unsigned char *buffer, size_t buffer_size)
{
  int rc;
#if defined(HAVE_YASSL) /* YaSSL */
  rc= yaSSL::RAND_bytes(buffer, buffer_size);

  if (!rc)
    return 1;
#elif defined(HAVE_OPENSSL)
  rc= RAND_bytes(buffer, buffer_size);

  if (!rc)
  {
    ERR_clear_error();
    return 1;
  }
#else /* no SSL */
#error not using an SSL library not supported
#endif
  return 0;
}


/**
  Generate a random number using the OpenSSL/yaSSL supplied
  random number generator if available.

  @param rand_st [INOUT] Structure used for number generation
                         only if none of the SSL libraries are
                         available.

  @retval                Generated random number.
*/

double my_rnd_ssl(struct rand_struct *rand_st)
{
  unsigned int res;

  if (my_rand_buffer((unsigned char *) &res, sizeof(res)))
    return my_rnd(rand_st);

  return (double)res / (double)UINT_MAX;
}


#ifdef __cplusplus
}
#endif
