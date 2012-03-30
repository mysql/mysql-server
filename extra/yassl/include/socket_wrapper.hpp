/*
   Copyright (C) 2000-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
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



typedef unsigned char byte;


// Wraps Windows Sockets and BSD Sockets
class Socket {
    socket_t socket_;                    // underlying socket descriptor
    bool     wouldBlock_;                // if non-blocking data, for last read 
    bool     nonBlocking_;               // is option set
public:
    explicit Socket(socket_t s = INVALID_SOCKET);
    ~Socket();

    void     set_fd(socket_t s);
    uint     get_ready() const;
    socket_t get_fd()    const;

    uint send(const byte* buf, unsigned int len, unsigned int& sent,
              int flags = 0);
    uint receive(byte* buf, unsigned int len, int flags = 0);

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
