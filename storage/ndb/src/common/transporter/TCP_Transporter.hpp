/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TCP_TRANSPORTER_HPP
#define TCP_TRANSPORTER_HPP

#include "Transporter.hpp"

#include <NdbTCP.h>

struct ReceiveBuffer {
  Uint32 *startOfBuffer;    // Pointer to start of the receive buffer 
  Uint32 *readPtr;          // Pointer to start reading data
  
  char   *insertPtr;        // Pointer to first position in the receiveBuffer
                            // in which to insert received data. Earlier
                            // received incomplete messages (slack) are 
                            // copied into the first part of the receiveBuffer

  Uint32 sizeOfData;        // In bytes
  Uint32 sizeOfBuffer;
  
  ReceiveBuffer() {}
  bool init(int bytes);
  void destroy();
  
  void clear();
  void incompleteMessage();
};

class TCP_Transporter : public Transporter {
  friend struct TransporterReceiveData;
  friend class TransporterRegistry;
  friend class Loopback_Transporter;
private:
  // Initialize member variables
  TCP_Transporter(TransporterRegistry&, const TransporterConfiguration* conf);

  // Disconnect, delete send buffers and receive buffer
  virtual ~TCP_Transporter();

  /**
   * Clear any data buffered in the transporter.
   * Should only be called in a disconnected state.
   */
  virtual void resetBuffers();

  virtual bool configure_derived(const TransporterConfiguration* conf);

  /**
   * Allocate buffers for sending and receiving
   */
  bool initTransporter();

  /**
   * Retrieves the contents of the send buffers and writes it on
   * the external TCP/IP interface.
   */
  bool doSend(bool need_wakeup = true);
  
  /**
   * It reads the external TCP/IP interface once 
   * and puts the data in the receiveBuffer
   */
  int doReceive(TransporterReceiveHandle&);

  /**
   * Get Receive Data
   *
   *  Returns - no of bytes to read
   *            and set ptr
   */
  virtual Uint32 getReceiveData(Uint32 ** ptr);
  
  /**
   * Update receive data ptr
   */
  virtual void updateReceiveDataPtr(Uint32 bytesRead);

  inline bool hasReceiveData () const {
    return receiveBuffer.sizeOfData > 0;
  }
protected:
  /**
   * Setup client/server and perform connect/accept
   * Is used both by clients and servers
   * A client connects to the remote server
   * A server accepts any new connections
   */
  virtual bool connect_server_impl(NDB_SOCKET_TYPE sockfd);
  virtual bool connect_client_impl(NDB_SOCKET_TYPE sockfd);
  bool connect_common(NDB_SOCKET_TYPE sockfd);
  
  /**
   * Disconnects a TCP/IP node, possibly blocking.
   */
  virtual void disconnectImpl();
  
private:
  Uint32 maxReceiveSize;
  
  /**
   * Socket options
   */
  int sockOptRcvBufSize;
  int sockOptSndBufSize;
  int sockOptNodelay;
  int sockOptTcpMaxSeg;

  void setSocketOptions(NDB_SOCKET_TYPE socket);

  static bool setSocketNonBlocking(NDB_SOCKET_TYPE aSocket);
  virtual int pre_connect_options(NDB_SOCKET_TYPE aSocket);
  
  bool send_is_possible(int timeout_millisec) const;
  bool send_is_possible(NDB_SOCKET_TYPE fd, int timeout_millisec) const;

  ReceiveBuffer receiveBuffer;

  bool send_limit_reached(int bufsize) { return bufsize > TCP_SEND_LIMIT; }
};

inline
Uint32
TCP_Transporter::getReceiveData(Uint32 ** ptr){
  (* ptr) = receiveBuffer.readPtr;
  return receiveBuffer.sizeOfData;
}

inline
void
TCP_Transporter::updateReceiveDataPtr(Uint32 bytesRead){
  char * ptr = (char *)receiveBuffer.readPtr;
  assert(receiveBuffer.sizeOfData >= bytesRead);
  ptr += bytesRead;
  receiveBuffer.readPtr = (Uint32*)ptr;
  receiveBuffer.sizeOfData -= bytesRead;
  receiveBuffer.incompleteMessage();
}

inline
bool
ReceiveBuffer::init(int bytes){
#ifdef DEBUG_TRANSPORTER
  ndbout << "Allocating " << bytes << " bytes as receivebuffer" << endl;
#endif

  startOfBuffer = new Uint32[((bytes + 0) >> 2) + 1];
  sizeOfBuffer  = bytes + sizeof(Uint32);
  clear();
  return true;
}

inline
void
ReceiveBuffer::destroy(){
  delete[] startOfBuffer;
  sizeOfBuffer  = 0;
  startOfBuffer = 0;
  clear();
}

inline
void
ReceiveBuffer::clear(){
  readPtr    = startOfBuffer;
  insertPtr  = (char *)startOfBuffer;
  sizeOfData = 0;
}

inline
void
ReceiveBuffer::incompleteMessage() {
  if(startOfBuffer != readPtr){
    if(sizeOfData != 0)
      memmove(startOfBuffer, readPtr, sizeOfData);
    readPtr   = startOfBuffer;
    insertPtr = ((char *)startOfBuffer) + sizeOfData;
  }
}


#endif // Define of TCP_Transporter_H
