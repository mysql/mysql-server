/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/


//**************************************************************************** 
// 
//  AUTHOR 
//      Åsa Fransson 
// 
//  NAME 
//      TransporterCallback 
// 
// 
//***************************************************************************/ 
#ifndef TRANSPORTER_CALLBACK_H 
#define TRANSPORTER_CALLBACK_H 
 
#include <kernel_types.h> 
#include "TransporterDefinitions.hpp" 
#include "TransporterRegistry.hpp"
 
/**
 * The TransporterReceiveCallback class encapsulates
 * the receive aspects of the transporter code that is
 * specific to particular
 * upper layer (NDB API, single-threaded kernel, or multi-threaded kernel).
 */
class TransporterReceiveHandle : public TransporterReceiveData {
public:
  /**
   * This method is called to deliver a signal to the upper layer.
   *
   * The method may either execute the signal immediately (NDB API), or
   * queue it for later execution (kernel).
   *
   * @returns true if no more signals should be delivered
   */
  virtual bool deliver_signal(SignalHeader * const header,
                              Uint8 prio,
                              Uint32 * const signalData,
                              LinearSectionPtr ptr[3]) = 0;

  /**
   * This method is called regularly (currently after receive from each
   * transporter) by the transporter code.
   *
   * It provides an opportunity for the upper layer to interleave signal
   * handling with signal reception, if so desired, so as to not needlessly
   * overflow the received signals job buffers. Ie. the single-threaded
   * kernel implementation currently executes received signals if the
   * job buffer reaches a certain percentage of occupancy.
   *
   * The method should return non-zero if signals were execute, zero if not.
   */
  virtual int checkJobBuffer() = 0;

  /**
   * Same as reportSendLen(), but for received data.
   *
   * For multithreaded cases, this is only called while holding the global
   * receive lock.
   */
  virtual void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes) = 0;

  /**
   * Transporter code calls this method when a connection to a node has been
   * established (state becomes CONNECTED).
   *
   * This is called from TransporterRegistry::update_connections(), which only
   * runs from the receive thread.
   */
  virtual void reportConnect(NodeId nodeId) = 0;

  /**
   * Transporter code calls this method when a connection to a node is lost
   * (state becomes DISCONNECTED).
   *
   * This is called from TransporterRegistry::update_connections(), which only
   * runs from the receive thread.
   */
  virtual void reportDisconnect(NodeId nodeId, Uint32 errNo) = 0;

  /**
   * Called by transporter code to report error
   *
   * This is called from TransporterRegistry::update_connections(), which only
   * runs from the receive thread.
   */
  virtual void reportError(NodeId nodeId, TransporterError errorCode,
                           const char *info = 0) = 0;

  /**
   * Called from transporter code after a successful receive from a node.
   *
   * Used for heartbeat detection by upper layer.
   */
  virtual void transporter_recv_from(NodeId node) = 0;

  /**
   *
   */
  virtual ~TransporterReceiveHandle() { };

#ifndef NDEBUG
  /**
   * 'm_active' is used by 'class TransporterReceiveWatchdog' in 
   * DEBUG to detect concurrent calls to ::update_connections and
   * ::performReceive() which isn't allowed.
   */
  TransporterReceiveHandle() : m_active(false) {};
  volatile bool m_active;
#endif
};

/**
 * The TransporterCallback class encapsulates those aspects of the transporter
 * code that is specific to particular upper layer (NDB API, single-threaded
 * kernel, or multi-threaded kernel).
 */
class TransporterCallback {
public:
  /**
   * The transporter periodically calls this method, indicating the number
   * of sends done to one NodeId, as well as total bytes sent.
   *
   * For multithreaded cases, this is only called while the send lock for the
   * given node is held.
   */
  virtual void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes) = 0;

  /**
   * Locking (no-op in single-threaded VM).
   *
   * These are used to lock/unlock the transporter for connect and disconnect
   * operation.
   *
   * Upper layer must implement these so that between return of
   * lock_transporter() and call of unlock_transporter(), no thread will be
   * running simultaneously in performSend() (for that node) or
   * performReceive().
   *
   * See src/common/transporter/trp.txt for more information.
   */
  virtual void lock_transporter(NodeId node) { }
  virtual void unlock_transporter(NodeId node) { }

  /**
   * ToDo: In current patch, these are not used, instead we use default
   * implementations in TransporterRegistry.
   */

  /**
   * Notify upper layer of explicit wakeup request
   *
   * The is called from the thread holding receiving data from the
   * transporter, under the protection of the transporter lock.
   */
  virtual void reportWakeup() { }

  /**
   * Ask upper layer to supply a list of struct iovec's with data to
   * send to a node.
   *
   * The call should fill in data from all threads (if any).
   *
   * The call will write at most MAX iovec structures starting at DST.
   *
   * Returns number of entries filled-in on success, -1 on error.
   *
   * Will be called from the thread that does performSend(), so multi-threaded
   * use cases must be prepared for that and do any necessary locking.
   */
  virtual Uint32 get_bytes_to_send_iovec(NodeId, struct iovec *dst, Uint32) = 0;

  /**
   * Called when data has been sent, allowing to free / reuse the space. Passes
   * number of bytes sent.
   *
   * Note that this may be less than the sum of all iovec::iov_len supplied
   * (in case of partial send). In particular, one iovec entry may have been
   * partially sent, and may not be freed until another call to bytes_sent()
   * which covers the rest of its data.
   *
   * Returns total amount of unsent data in send buffers for this node.
   *
   * Like get_bytes_to_send_iovec(), this is called during performSend().
   */
  virtual Uint32 bytes_sent(NodeId node, Uint32 bytes) = 0;

  /**
   * Called to check if any data is available for sending with doSend().
   *
   * Like get_bytes_to_send_iovec(), this is called during performSend().
   */
  virtual bool has_data_to_send(NodeId node) = 0;

  /**
   * Called to completely empty the send buffer for a node (ie. disconnect).
   *
   * Can be called to check that no one has written to the sendbuffer
   * since it was reset last time by using the "should_be_emtpy" flag
   */
  virtual void reset_send_buffer(NodeId node, bool should_be_empty=false) = 0;

  virtual ~TransporterCallback() { };
};


/**
 * This interface implements send buffer access for the
 * TransporterRegistry::prepareSend() method.
 *
 * It is used to allocate send buffer space for signals to send, and can be
 * used to do per-thread buffer allocation.
 *
 * Reading and freeing data is done from the TransporterCallback class,
 * methods get_bytes_to_send_iovec() and bytes_send_iovec().
 */
class TransporterSendBufferHandle {
public:
  /**
   * Get space for packing a signal into, allocate more buffer as needed.
   *
   * The max_use parameter is a limit on the amount of unsent data (whether
   * delivered through get_bytes_to_send_iovec() or not) for one node; the
   * method must return NULL rather than allow to exceed this amount.
   */
  virtual Uint32 *getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                              Uint32 max_use) = 0;
  /**
   * Called when new signal is packed.
   *
   * Returns number of bytes in buffer not yet sent (this includes data that
   * was made available to send with get_bytes_to_send_iovec(), but has not
   * yet been marked as really sent from bytes_sent()).
   */
  virtual Uint32 updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio) = 0;

  /**
   * Provide a mechanism to check the level of risk in using the send buffer.
   * This is useful in long-running activities to ensure that they don't
   * jeopardize short, high priority actions in the cluster.
   */
  virtual void getSendBufferLevel(NodeId node, SB_LevelType &level) = 0;

  /**
   * Called during prepareSend() if send buffer gets full, to do an emergency
   * send to the remote node with the hope of freeing up send buffer for the
   * signal to be queued.
   */
  virtual bool forceSend(NodeId node) = 0;

  virtual ~TransporterSendBufferHandle() { };
};

#endif
