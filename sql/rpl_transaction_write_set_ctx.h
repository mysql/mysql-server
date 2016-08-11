/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_TRANSACTION_WRITE_SET_CTX_H
#define RPL_TRANSACTION_WRITE_SET_CTX_H

#include "my_global.h"
#include <vector>

/**
  Server side support to provide a service to plugins to report if
  a given transaction should continue or be aborted.
  Its value is reset on Transaction_ctx::cleanup().
  Its value is set through service service_rpl_transaction_ctx.
*/
class Rpl_transaction_write_set_ctx
{
public:
  Rpl_transaction_write_set_ctx();
  virtual ~Rpl_transaction_write_set_ctx() {}

  /*
    Function to add the write set of the hash of the PKE in the std::vector
    in the transaction_ctx object.

    @param[in] hash - the uint64 type hash value of the PKE.
  */
  void add_write_set(uint64 hash);

  /*
    Function to get the pointer of the write set vector in the
    transaction_ctx object.
  */
  std::vector<uint64> *get_write_set();

  /*
    Cleanup function of the vector which stores the PKE.
  */
  void clear_write_set();

private:
  std::vector<uint64> write_set;
};

#endif	/* RPL_TRANSACTION_WRITE_SET_CTX_H */
