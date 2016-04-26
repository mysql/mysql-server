/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "ngs_common/options_ssl.h"

#include "mysql/service_ssl_wrapper.h"
#include "ngs/memory.h"
#include <iterator>

using namespace ngs;

std::string Options_session_ssl::ssl_cipher()
{
  char result[1024];

  ssl_wrapper_cipher(m_vio, result, sizeof(result));

  return result;
}

std::string Options_session_ssl::ssl_version()
{
  char result[256];

  ssl_wrapper_version(m_vio, result, sizeof(result));

  return result;
}

std::vector<std::string> Options_session_ssl::ssl_cipher_list()
{
  std::vector<std::string> result;
  const size_t num_of_elements = 1024;
  const char *versions[num_of_elements];

  long number_of_items = ssl_wrapper_cipher_list(m_vio, versions, num_of_elements);

  std::copy(versions, versions + number_of_items, std::back_inserter(result));

  return result;
}

long Options_session_ssl::ssl_verify_depth()
{
  return  ssl_wrapper_verify_depth(m_vio);
}

long Options_session_ssl::ssl_verify_mode()
{
  return  ssl_wrapper_verify_mode(m_vio);
}

long Options_session_ssl::ssl_sessions_reused()
{
  return 0;
}

long Options_session_ssl::ssl_get_verify_result_and_cert()
{
  return ssl_wrapper_get_verify_result_and_cert(m_vio);
}

std::string Options_session_ssl::ssl_get_peer_certificate_issuer()
{
  char issuer[1024];

  ssl_wrapper_get_peer_certificate_issuer(m_vio, issuer, sizeof(issuer));

  return issuer;
}

std::string Options_session_ssl::ssl_get_peer_certificate_subject()
{
  char subject[1024];

  ssl_wrapper_get_peer_certificate_subject(m_vio, subject, sizeof(subject));

  return subject;
}


long Options_context_ssl::ssl_ctx_verify_depth()
{
  return ssl_wrapper_ctx_verify_depth(m_vio_ssl);
}

long Options_context_ssl::ssl_ctx_verify_mode()
{
  return ssl_wrapper_ctx_verify_mode(m_vio_ssl);
}

std::string Options_context_ssl::ssl_server_not_after()
{
  char result[200];

  ssl_wrapper_ctx_server_not_after(m_vio_ssl, result, sizeof(result));

  return result;
}

std::string Options_context_ssl::ssl_server_not_before()
{
  char result[200];

  ssl_wrapper_ctx_server_not_before(m_vio_ssl, result, sizeof(result));

  return result;
}

long Options_context_ssl::ssl_sess_accept_good()
{
  return ssl_wrapper_sess_accept_good(m_vio_ssl);
}

long Options_context_ssl::ssl_sess_accept()
{
  return ssl_wrapper_sess_accept(m_vio_ssl);
}

long Options_context_ssl::ssl_accept_renegotiates()
{
  return 0;
}

long Options_context_ssl::ssl_session_cache_hits()
{
  return 0;
}

long Options_context_ssl::ssl_session_cache_misses()
{
  return 0;
}

std::string Options_context_ssl::ssl_session_cache_mode()
{
  return "OFF";
}

long Options_context_ssl::ssl_session_cache_overflows()
{
  return 0;
}

long Options_context_ssl::ssl_session_cache_size()
{
  return 0;
}

long Options_context_ssl::ssl_session_cache_timeouts()
{
  return 0;
}

long Options_context_ssl::ssl_used_session_cache_entries()
{
  return 0;
}
