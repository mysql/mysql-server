/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef Transporter_H
#define Transporter_H

#include <ndb_global.h>

#include <TransporterCallback.hpp>
#include "TransporterDefinitions.hpp"
#include "Packer.hpp"

#include <NdbMutex.h>
#include <NdbThread.h>

class Transporter {
  friend class TransporterRegistry;
public:
  virtual bool initTransporter() = 0;

  /**
   * Destructor
   */
  virtual ~Transporter();

  /**
   * None blocking
   *    Use isConnected() to check status
   */
  virtual void doConnect();
  
  /**
   * Blocking
   */
  virtual void doDisconnect();

  virtual Uint32 * getWritePtr(Uint32 lenBytes, Uint32 prio) = 0;
  virtual void updateWritePtr(Uint32 lenBytes, Uint32 prio) = 0;
  
  /**
   * Are we currently connected
   */
  bool isConnected() const;
  
  /**
   * Remote Node Id
   */
  NodeId getRemoteNodeId() const;


  /**
   * Set callback object
   */
  void setCallbackObject(void * callback);

protected:
  Transporter(NodeId lNodeId,
	      NodeId rNodeId, 
	      int byteorder, 
	      bool compression, 
	      bool checksum, 
	      bool signalId);

  /**
   * Blocking, for max timeOut milli seconds
   *   Returns true if connect succeded
   */
  virtual bool connectImpl(Uint32 timeOut) = 0;
  
  /**
   * Blocking
   */
  virtual void disconnectImpl() = 0;
  
  const NodeId localNodeId;
  const NodeId remoteNodeId;
  
  unsigned createIndex;
  
  int byteOrder;
  bool compressionUsed;
  bool checksumUsed;
  bool signalIdUsed;
  Packer m_packer;
  

private:
  /**
   * Thread and mutex for connect
   */
  NdbThread* theThreadPtr;
  friend void* runConnect(void * me);

protected:
  /**
   * Error reporting from connect thread(s)
   */
  void reportThreadError(NodeId nodeId, 
			 TransporterError errorCode);
  Uint32 getErrorCount();
  TransporterError getThreadError();
  void   resetThreadError();
  TransporterError _threadError;
  Uint32 _timeOutMillis;
  Uint32 _errorCount;

protected:  
  NdbMutex* theMutexPtr;
  bool _connected;     // Are we connected
  bool _connecting;    // Connect thread is running
  bool _disconnecting; // We are disconnecting

  void * callbackObj;
};

inline
bool
Transporter::isConnected() const {
  return _connected;
}

inline
NodeId
Transporter::getRemoteNodeId() const {
  return remoteNodeId;
}

inline 
void 
Transporter::reportThreadError(NodeId nodeId, TransporterError errorCode)
{
#if 0
  ndbout_c("Transporter::reportThreadError (NodeId: %d, Error code: %d)",
	   nodeId, errorCode);
#endif
  _threadError = errorCode;
  _errorCount++;
}

inline
TransporterError 
Transporter::getThreadError(){
  return _threadError;
}

inline
Uint32
Transporter::getErrorCount()
{ 
  return _errorCount;
}

inline 
void 
Transporter::resetThreadError()
{
  _threadError = TE_NO_ERROR;
}

inline 
void
Transporter::setCallbackObject(void * callback) {
  callbackObj = callback;
}

#endif // Define of Transporter_H
