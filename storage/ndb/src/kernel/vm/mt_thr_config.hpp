/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#ifndef MT_THR_CONFIG_HPP
#define MT_THR_CONFIG_HPP

struct NdbThread;
#include <Vector.hpp>
#include <SparseBitmask.hpp>
#include <BaseString.hpp>
#include "mgmcommon/thr_config.hpp"

#define JAM_FILE_ID 272


/**
 * This class is used by ndbmtd
 *   when setting up threads (and locking)
 */
class THRConfigApplier : public THRConfig
{
public:
  const char * getName(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const unsigned short list[], unsigned cnt) const;
  void appendInfoSendThread(BaseString&, unsigned instance_no) const;

  int do_thread_prio_io(NdbThread*, unsigned &thread_prio);
  int do_thread_prio_watchdog(NdbThread*, unsigned &thread_prio);
  int do_thread_prio_send(NdbThread*,
                          unsigned instance,
                          unsigned &thread_prio);
  int do_thread_prio(NdbThread*,
                     const unsigned short list[],
                     unsigned cnt,
                     unsigned &thread_prio);
  int do_thread_prio(NdbThread*,
                     const T_Thread* thr,
                     unsigned &thread_prio);

  int do_bind(NdbThread*, const unsigned short list[], unsigned cnt);
  int do_bind_io(NdbThread*);
  int do_bind_idxbuild(NdbThread*);
  int do_bind_watchdog(NdbThread*);
  int do_bind_send(NdbThread*, unsigned);
  int do_unbind(NdbThread*);
  bool do_get_nosend(const unsigned short list[],
                     unsigned cnt) const;
  bool do_get_realtime_io() const;
  bool do_get_realtime_wd() const;
  bool do_get_realtime_send(unsigned) const;
  unsigned do_get_spintime_send(unsigned) const;
  bool do_get_realtime(const unsigned short list[],
                       unsigned cnt) const;
  unsigned do_get_spintime(const unsigned short list[],
                           unsigned cnt) const;
protected:
  const T_Thread* find_thread(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const T_Thread*) const;
  int do_bind(NdbThread*, const T_Thread*);
};

/**
 * This class is used to temporarily change the thread
 * type during some task
 */
class THRConfigRebinder
{
public:
  THRConfigRebinder(THRConfigApplier*, THRConfig::T_Type, NdbThread*);
  ~THRConfigRebinder();
private:
  THRConfigApplier* m_config_applier;
  int m_state;
  NdbThread* m_thread;
};

#undef JAM_FILE_ID

#endif
