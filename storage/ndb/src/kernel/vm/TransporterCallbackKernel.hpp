/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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
   * Assign trps to this TransporterReceiveHandle
   */
  void assign_trps(Uint32 *recv_thread_idx_array);
#endif
  void *m_trpman;

  void assign_trpman(void *trpman)
  {
    m_trpman = trpman;
  }
  /* TransporterCallback interface. */
  bool deliver_signal(SignalHeader * const header,
                      Uint8 prio,
                      TransporterError &error_code,
                      Uint32 * const signalData,
                      LinearSectionPtr ptr[3]) override;
  void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes) override;
  void reportConnect(NodeId nodeId) override;
  void reportDisconnect(NodeId nodeId, Uint32 errNo) override;
  void reportError(NodeId nodeId, TransporterError errorCode,
                   const char *info = 0) override;
  void transporter_recv_from(NodeId node) override;
  int checkJobBuffer() override;
  ~TransporterReceiveHandleKernel() override { }
};


#undef JAM_FILE_ID

#endif
