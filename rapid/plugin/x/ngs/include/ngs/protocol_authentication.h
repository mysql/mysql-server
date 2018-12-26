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

#ifndef _NGS_PROTOCOL_AUTHENTICATION_H_
#define _NGS_PROTOCOL_AUTHENTICATION_H_


#include "ngs_common/bind.h"
#include "ngs/error_code.h"
#include "ngs/memory.h"


namespace ngs
{
  class Session_interface;
  class Authentication_handler;

  typedef Custom_allocator<Authentication_handler>::Unique_ptr Authentication_handler_ptr;

  class Authentication_handler
  {
  public:
    enum Status
    {
      Ongoing,
      Succeeded,
      Failed,
      Error
    };

    struct Response
    {
      std::string data;
      Status status;
      int error_code;
    };

    virtual ~Authentication_handler() {}

    typedef Authentication_handler_ptr (*create)(Session_interface *session);

    virtual Response handle_start(const std::string &mechanism,
                                  const std::string &data,
                                  const std::string &initial_response) = 0;

    virtual Response handle_continue(const std::string &data) = 0;

    virtual void done() = 0;

    static ngs::Authentication_handler_ptr wrap_ptr(Authentication_handler *auth)
    {
      return ngs::Authentication_handler_ptr(auth, ngs::bind(&ngs::Authentication_handler::done, ngs::placeholders::_1));
    }

  protected:
    std::string compute_password_hash(const std::string &password);
    bool extract_null_terminated_element(const std::string &message, std::size_t &element_position, size_t element_size, char *output);
  };
};

#endif
