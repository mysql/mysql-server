/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef TRANSPORTER_CALLBACK_KERNEL_HPP
#define TRANSPORTER_CALLBACK_KERNEL_HPP

#include <TransporterCallback.hpp>

#define JAM_FILE_ID 305


class TransporterReceiveHandleKernel
  : public TransporterReceiveHandle
{
public:
#ifdef NDBD_MULTITHREADED
  TransporterReceiveHandleKernel(Uint32 thr_no, Uint32 recv_thr_no) :
    m_thr_no(thr_no), m_receiver_thread_idx(recv_thr_no) {}

  /**
   * m_thr_no == index in m_thr_data[]
   */
  Uint32 m_thr_no;

  /**
   * m_receiver_thread_idx == m_thr_no - firstReceiverThread ==
   *   instance() - 1(proxy)
   */
  Uint32 m_receiver_thread_idx;

  /**
   * Assign nodes to this TransporterReceiveHandle
   */
  void assign_nodes(NodeId *recv_thread_idx_array);
#endif

  /* TransporterCallback interface. */
  bool deliver_signal(SignalHeader * const header,
                      Uint8 prio,
                      Uint32 * const signalData,
                      LinearSectionPtr ptr[3]);
  void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportConnect(NodeId nodeId);
  void reportDisconnect(NodeId nodeId, Uint32 errNo);
  void reportError(NodeId nodeId, TransporterError errorCode,
                   const char *info = 0);
  void transporter_recv_from(NodeId node);
  int checkJobBuffer();
  virtual ~TransporterReceiveHandleKernel() { }
};


#undef JAM_FILE_ID

#endif
