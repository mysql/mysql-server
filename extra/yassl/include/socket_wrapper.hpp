/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 */


/* The socket wrapper header defines a Socket class that hides the differences
 * between Berkely style sockets and Windows sockets, allowing transparent TCP
 * access.
 */


#ifndef yaSSL_SOCKET_WRAPPER_HPP
#define yaSSL_SOCKET_WRAPPER_HPP


#ifdef _WIN32
    #include <winsock2.h>
#else 
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif


namespace yaSSL {

typedef unsigned int uint;

#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
    const socket_t INVALID_SOCKET = -1;
    const int SD_RECEIVE   = 0;
    const int SD_SEND      = 1;
    const int SD_BOTH      = 2;
    const int SOCKET_ERROR = -1;
#endif

  extern "C" {
    #include "openssl/transport_types.h"
  }

typedef unsigned char byte;


// Wraps Windows Sockets and BSD Sockets
class Socket {
    socket_t socket_;                    // underlying socket descriptor
    bool     wouldBlock_;                // if non-blocking data, for last read 
    bool     nonBlocking_;               // is option set
    void     *ptr_;                      // Argument to transport function
    yaSSL_send_func_t send_func_;        // Function to send data
    yaSSL_recv_func_t recv_func_;        // Function to receive data
public:
    explicit Socket(socket_t s = INVALID_SOCKET);
    ~Socket();

    void     set_fd(socket_t s);
    uint     get_ready() const;
    socket_t get_fd()    const;

    void set_transport_ptr(void *ptr);
    void set_transport_recv_function(yaSSL_recv_func_t recv_func);
    void set_transport_send_function(yaSSL_send_func_t send_func);

    uint send(const byte* buf, unsigned int len, unsigned int& sent);
    uint receive(byte* buf, unsigned int len);

    bool wait();
    bool WouldBlock() const;
    bool IsNonBlocking() const;

    void closeSocket();
    void shutDown(int how = SD_SEND);

    static int  get_lastError();
    static void set_lastError(int error);
private:
    Socket(const Socket&);              // hide copy
    Socket& operator= (const Socket&);  // and assign
};


} // naemspace

#endif // yaSSL_SOCKET_WRAPPER_HPP
