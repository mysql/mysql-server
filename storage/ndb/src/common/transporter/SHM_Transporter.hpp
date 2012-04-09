/*
   Copyright (C) 2003-2006, 2008 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef SHM_Transporter_H
#define SHM_Transporter_H

#include "Transporter.hpp"
#include "SHM_Buffer.hpp"

#ifdef NDB_WIN32
typedef Uint32 key_t;
#endif

/** 
 * class SHMTransporter
 * @brief - main class for the SHM transporter.
 */

class SHM_Transporter : public Transporter {
  friend class TransporterRegistry;
public:
  SHM_Transporter(TransporterRegistry &,
		  const char *lHostName,
		  const char *rHostName, 
		  int r_port,
		  bool isMgmConnection,
		  NodeId lNodeId,
		  NodeId rNodeId,
		  NodeId serverNodeId,
		  bool checksum, 
		  bool signalId,
		  key_t shmKey,
		  Uint32 shmSize);
  
  /**
   * SHM destructor
   */
  virtual ~SHM_Transporter();
  
  virtual bool configure_derived(const TransporterConfiguration* conf);

  /**
   * Do initialization
   */
  bool initTransporter();
  
  void getReceivePtr(Uint32 ** ptr, Uint32 ** eod){
    reader->getReadPtr(* ptr, * eod);
  }
  
  void updateReceivePtr(Uint32 * ptr){
    reader->updateReadPtr(ptr);
  }
  
protected:
  /**
   * disconnect a segmnet
   * -# deletes the shm buffer associated with a segment
   * -# marks the segment for removal
   */
  void disconnectImpl();

  /**
   * Blocking
   *
   * -# Create shm segment
   * -# Attach to it
   * -# Wait for someone to attach (max wait = timeout), then rerun again
   *    until connection established.
   * @param timeOutMillis - the time to sleep before (ms) trying again.
   * @returns - True if the server managed to hook up with the client,
   *            i.e., both agrees that the other one has setup the segment.
   *            Otherwise false.
   */
  virtual bool connect_server_impl(NDB_SOCKET_TYPE sockfd);

  /**
   * Blocking
   *
   * -# Attach to shm segment
   * -# Check if the segment is setup
   * -# Check if the server set it up
   * -# If all clear, return.
   * @param timeOutMillis - the time to sleep before (ms) trying again.
   * @returns - True if the client managed to hook up with the server,
   *            i.e., both agrees that the other one has setup the segment.
   *            Otherwise false.
   */
  virtual bool connect_client_impl(NDB_SOCKET_TYPE sockfd);

  bool connect_common(NDB_SOCKET_TYPE sockfd);

  bool ndb_shm_create();
  bool ndb_shm_get();
  bool ndb_shm_attach();

  /**
   * Check if there are two processes attached to the segment (a connection)
   * @return - True if the above holds. Otherwise false.
   */
  bool checkConnected();

  
  /**
   * Initialises the SHM_Reader and SHM_Writer on the segment 
   */
  void setupBuffers();

  /**
   * doSend (i.e signal receiver)
   */
  int doSend();
  int m_remote_pid;
  Uint32 m_signal_threshold;

private:
  bool _shmSegCreated;
  bool _attached;
  bool m_connected;
  
  key_t shmKey;
  volatile Uint32 * serverStatusFlag;
  volatile Uint32 * clientStatusFlag;  
  bool setupBuffersDone;
  
#ifdef NDB_WIN32
  HANDLE hFileMapping;
#else
  int shmId;
#endif
  
  int shmSize;
  char * shmBuf;
  
  SHM_Reader * reader;
  SHM_Writer * writer;
  
  /**
   * @return - True if the reader has data to read on its segment.
   */
  bool hasDataToRead() const {
    return reader->empty() == false;
  }

  void make_error_info(char info[], int sz);

  bool send_limit_reached(int bufsize)
  {
    return ((Uint32)bufsize >= m_signal_threshold);
  }
  bool send_is_possible(int timeout_millisec) const { return 1; }
};

#endif
