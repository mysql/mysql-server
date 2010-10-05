/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef trp_client_hpp
#define trp_client_hpp

#include <ndb_global.h>

class NdbApiSignal;
struct LinearSectionPtr;

class trp_client
{
public:
  virtual ~trp_client() {}

  virtual void trp_deliver_signal(const NdbApiSignal *,
                                  const LinearSectionPtr ptr[3]) = 0;
  virtual void trp_node_status(Uint32, Uint32 event) = 0;
};

class PollGuard
{
public:
  PollGuard(class NdbImpl&);
  ~PollGuard() { unlock_and_signal(); }
  int wait_n_unlock(int wait_time, Uint32 nodeId, Uint32 state,
                    bool forceSend= false);
  int wait_for_input_in_loop(int wait_time, bool forceSend);
  void wait_for_input(int wait_time);
  int wait_scan(int wait_time, Uint32 nodeId, bool forceSend);
  void unlock_and_signal();
private:
  class TransporterFacade *m_tp;
  class NdbWaiter *m_waiter;
  Uint32 m_block_no;
  bool m_locked;
};

#endif
