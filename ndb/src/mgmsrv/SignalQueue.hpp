/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef __SIGNALQUEUE_HPP_INCLUDED__
#define __SIGNALQUEUE_HPP_INCLUDED__

#include <NdbApiSignal.hpp>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <Vector.hpp>

/* XXX Look for an already existing definition */
#define DEFAULT_TIMEOUT 5000

class SignalQueue {
public:
  typedef void (* SignalHandler)(void *obj, int gsn, NdbApiSignal *signal);

  SignalQueue();
  ~SignalQueue();

  /**
   * Static wrapper making it possible to call receive without knowing the
   * type of the receiver
   */
  static void receive(void *me, NdbApiSignal *signal);

  /**
   * Enqueues a signal, and notifies any thread waiting for signals.
   */
  void receive(NdbApiSignal *signal);

  NdbApiSignal *waitFor(int gsn,
			NodeId nodeid = 0,
			Uint32 timeout = DEFAULT_TIMEOUT);
  template<class T> bool waitFor(Vector<T> &t,
				 T **handler,
				 NdbApiSignal **signal,
				 Uint32 timeout = DEFAULT_TIMEOUT);
private:
  NdbMutex *m_mutex; /* Locks all data in SignalQueue */
  NdbCondition *m_cond; /* Notifies about new signal in the queue */

  /**
   * Returns the last recently received signal. Must be called with
   * m_mutex locked.
   * The caller takes responsibility for deleting the returned object.
   *
   * @returns NULL if failed, or a received signal
   */
  NdbApiSignal *pop();

  class QueueEntry {
  public:
    NdbApiSignal *signal;
    QueueEntry *next;
  };
  QueueEntry *m_signalQueueHead; /** Head of the queue.
				  *  New entries added on the tail
				  */
};

template<class T> bool
SignalQueue::waitFor(Vector<T> &t,
		     T **handler,
		     NdbApiSignal **signal,
		     Uint32 timeout) {
  Guard g(m_mutex);

  if(m_signalQueueHead == NULL)
    NdbCondition_WaitTimeout(m_cond, m_mutex, timeout);

  if(m_signalQueueHead == NULL)
    return false;

  for(size_t i = 0; i < t.size(); i++) {
    if(t[i].check(m_signalQueueHead->signal)) {
      * handler = &t[i];
      * signal = pop();
      return true;
    }
  }

  return false;
}

#endif /* !__SIGNALQUEUE_HPP_INCLUDED__ */
