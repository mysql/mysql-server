/*  Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; version 2 of the
    License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

/**
  @file
  Ssl functions wrapper service implementation. For more information please check
  the function comments.
*/

#include <my_global.h>
#include "mysql/service_rules_table.h"
#include "openssl/ssl.h"
#if defined(HAVE_YASSL)
using namespace yaSSL;
#endif // defined(HAVE_YASSL)
#include "mysql/service_ssl_wrapper.h"

#ifndef EMBEDDED_LIBRARY

namespace ssl_wrappe_service
{

int MY_ATTRIBUTE((visibility("default")))
dummy_function_to_ensure_we_are_linked_into_the_server() { return 1; }

} // namespace ssl_wrappe_service

extern "C"
{

#ifdef HAVE_YASSL

static char *
my_asn1_time_to_string(ASN1_TIME *time, char *buf, size_t len)
{
  if (!time)
      return NULL;
  return yaSSL_ASN1_TIME_to_string(time, buf, len);
}

#else /* openssl */

static char *
my_asn1_time_to_string(ASN1_TIME *time, char *buf, size_t len)
{
  int n_read;
  char *res= NULL;
  BIO *bio= BIO_new(BIO_s_mem());

  if (bio == NULL)
    return NULL;

  if (!ASN1_TIME_print(bio, time))
    goto end;

  n_read= BIO_read(bio, buf, (int) (len - 1));

  if (n_read > 0)
  {
    buf[n_read]= 0;
    res= buf;
  }

end:
  BIO_free(bio);
  return res;
}

#endif

/**
  Return version of SSL used in current connection

  @param vio              VIO connection descriptor
  @param buffer           Character buffer in which the version is going to be placed
  @param buffer_size      Size of the character buffer
*/
void ssl_wrapper_version(Vio *vio, char *buffer, const size_t buffer_size)
{
  const char *ssl_version= SSL_get_version((SSL*)vio->ssl_arg);

  strncpy(buffer, ssl_version, buffer_size);
  buffer[buffer_size - 1]= '\0';
}


/**
  Return cipher used in current connection

  @param vio              VIO connection descriptor
  @param buffer           Character buffer in which the cipher name is going to be placed
  @param buffer_size      Size of the character buffer
*/
void ssl_wrapper_cipher(Vio *vio, char *buffer, const size_t buffer_size)
{
  const char *ssl_version= SSL_get_cipher((SSL*)vio->ssl_arg);

  strncpy(buffer, ssl_version, buffer_size);
  buffer[buffer_size - 1]= '\0';
}

/**
  Return cipher list that can be used for SSL

  @param vio                       VIO connection descriptor
  @param clipher_list              Pointer to an array of c-strings
  @param maximun_num_of_elements   Size of the pointer array
*/
long ssl_wrapper_cipher_list(Vio *vio, const char **clipher_list, const size_t maximun_num_of_elements)
{
  const char *cipher= NULL;
  int         index= 0;
  size_t      element= 0;

  while(element < maximun_num_of_elements)
  {
     cipher = SSL_get_cipher_list((SSL*)vio->ssl_arg, index++);

     if (NULL == cipher)
       break;

     clipher_list[element++]= cipher;
  }

  return element;
}

/**
  Return the verification depth limit set in SSL

  @param vio              VIO connection descriptor

  @return
    -1 default values should be used
    >0 verification depth
*/
long ssl_wrapper_verify_depth(Vio *vio)
{
  return SSL_get_verify_depth((SSL*)vio->ssl_arg);
}

long ssl_wrapper_verify_mode(Vio *vio)
{
  return SSL_get_verify_mode((SSL*)vio->ssl_arg);
}

/**
  Return issuer name form peers ssl certificate

  @param vio              VIO connection descriptor
  @param issuer           Character buffer in which the issuer name is going to be placed
  @param issuer_size      Size of character buffer for the issuer name
*/
void ssl_wrapper_get_peer_certificate_issuer(Vio *vio, char *issuer, const size_t issuer_size)
{
  X509 *cert = NULL;
  if (!(cert = SSL_get_peer_certificate((SSL*)vio->ssl_arg)))
  {
    issuer[0]= '\0';
    return;
  }

  X509_NAME_oneline(X509_get_issuer_name(cert), issuer, issuer_size);
  X509_free(cert);
}

/**
  Return subject field form peers ssl certificate

  @param vio              VIO connection descriptor
  @param subject          Character buffer in which the subject is going to be placed
  @param subject_size     Size of character buffer for the subject
*/
void ssl_wrapper_get_peer_certificate_subject(Vio *vio, char *subject, const size_t subject_size)
{
  X509 *cert = NULL;
  if (!(cert = SSL_get_peer_certificate((SSL*)vio->ssl_arg)))
  {
    subject[0]= '\0';
    return;
  }

  X509_NAME_oneline(X509_get_subject_name(cert), subject, subject_size);
  X509_free(cert);
}

/**
  Check is peer certificate is present and try to verify it

  @param vio                 VIO connection descriptor

  @return
    X509_V_OK verification of peer certificate succeeded
    -1        verification failed
*/
long ssl_wrapper_get_verify_result_and_cert(Vio *vio)
{
  long result = 0;

  if (X509_V_OK != (result = SSL_get_verify_result((SSL*)vio->ssl_arg)))
    return result;

  X509 *cert = NULL;
  if (!(cert = SSL_get_peer_certificate((SSL*)vio->ssl_arg)))
    return -1;

  X509_free(cert);

  return X509_V_OK;
}

/**
  Return the verification depth limit set in SSL context

  @param vio_ssl              VIO SSL contex descriptor

  @return
    -1 default values should be used
    >0 verification depth
*/
long ssl_wrapper_ctx_verify_depth(struct st_VioSSLFd *vio_ssl)
{
  return SSL_CTX_get_verify_depth(vio_ssl->ssl_context);
}

/**
  Return the verification mode set in SSL context

  @param vio_ssl              VIO SSL contex descriptor

  @return
    -1 default values should be used
    >0 verification mode
*/
long ssl_wrapper_ctx_verify_mode(struct st_VioSSLFd *vio_ssl)
{
  return SSL_CTX_get_verify_mode(vio_ssl->ssl_context);
}

/**
  Return the last day the server certificate is valid

  @param vio_ssl              VIO SSL contex descriptor
  @param no_after             Character buffer for to be filed with the date in human readble format
  @param no_after_size        Size of the character buffer
*/
void  ssl_wrapper_ctx_server_not_after(struct st_VioSSLFd *vio_ssl, char *no_after, const size_t no_after_size)
{
  SSL *ssl= SSL_new(vio_ssl->ssl_context);
  if (NULL == ssl)
  {
    no_after[0]= '\0';
    return;
  }

  X509      *cert= SSL_get_certificate(ssl);
  ASN1_TIME *not_after_time= X509_get_notAfter(cert);
  const char *result= my_asn1_time_to_string(not_after_time, no_after, no_after_size);

  if (!result)
  {
    no_after[0]= '\0';
  }

  SSL_free(ssl);
}

/**
  Return the first day the server certificate is valid

  @param vio_ssl              VIO SSL contex descriptor
  @param no_before            Character buffer for to be filed with the date in human readble format
  @param no_before_size       Size of the character buffer
*/
void ssl_wrapper_ctx_server_not_before(struct st_VioSSLFd *vio_ssl, char *no_before, const size_t no_before_size)
{
  SSL *ssl= SSL_new(vio_ssl->ssl_context);
  if (NULL == ssl)
  {
    no_before[0]= '\0';
    return;
  }

  X509      *cert= SSL_get_certificate(ssl);
  ASN1_TIME *not_before_time= X509_get_notBefore(cert);
  const char *result= my_asn1_time_to_string(not_before_time, no_before, no_before_size);

  if (!result)
  {
    no_before[0]= '\0';
  }

  SSL_free(ssl);
}

long ssl_wrapper_sess_accept(struct st_VioSSLFd *vio_ssl)
{
  return SSL_CTX_sess_accept(vio_ssl->ssl_context);
}

long ssl_wrapper_sess_accept_good(struct st_VioSSLFd *vio_ssl)
{
  return SSL_CTX_sess_accept_good(vio_ssl->ssl_context);
}

/**
  Cleanup data allocated by SSL on thread stack

*/
void ssl_wrapper_thread_cleanup()
{
#if !defined(HAVE_YASSL)
  ERR_clear_error();
  ERR_remove_state(0);
#endif // !defined(HAVE_YASSL)
}

} /* extern "C" */

#endif // EMBEDDED_LIBRARY
