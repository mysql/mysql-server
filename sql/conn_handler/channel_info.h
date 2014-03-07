/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_CHANNEL_INFO_INCLUDED
#define SQL_CHANNEL_INFO_INCLUDED

#include "my_global.h"         // uint
#include "my_sys.h"            // my_micro_time

class THD;
typedef struct st_vio Vio;


/**
  This abstract base class represents connection channel information
  about a new connection. Its subclasses encapsulate differences
  between different connection channel types.

  Currently we support local and TCP/IP sockets (all platforms),
  named pipes and shared memory (Windows only).
*/
class Channel_info
{
  ulonglong prior_thr_create_utime;

protected:
  /**
    Create and initialize a Vio object.

    @retval   return a pointer to the initialized a vio object.
  */
  virtual Vio* create_and_init_vio() const = 0;

  Channel_info()
  : prior_thr_create_utime(0)
  { }

public:
  virtual ~Channel_info() {}

  /**
    Instantiate and initialize THD object and vio.

    @return
      @retval
        THD* pointer to initialized THD object.
      @retval
        NULL THD object allocation fails.
  */
  virtual THD* create_thd() = 0;

  /**
    Send error back to the client and close the channel.

    @param errorcode   code indicating type of error.
    @param error       operating system specific error code.
    @param senderror   true if the error need to be sent to
                       client else false.
  */
  virtual void send_error_and_close_channel(uint errorcode,
                                            int error,
                                            bool senderror) = 0;

  ulonglong get_prior_thr_create_utime() const
  { return prior_thr_create_utime; }

  void set_prior_thr_create_utime()
  { prior_thr_create_utime= my_micro_time(); }
};

#endif // SQL_CHANNEL_INFO_INCLUDED.
