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

#ifndef NdbEventOperationImpl_H
#define NdbEventOperationImpl_H

#include <NdbEventOperation.hpp>
#include <signaldata/SumaImpl.hpp>
#include <transporter/TransporterDefinitions.hpp>

class NdbGlobalEventBufferHandle;
class NdbEventOperationImpl : public NdbEventOperation {
public:
  NdbEventOperationImpl(NdbEventOperation &N,
			Ndb *theNdb, 
			const char* eventName, 
			const int bufferLength); 
  ~NdbEventOperationImpl();

  NdbEventOperation::State getState();

  int execute();
  int stop();
  NdbRecAttr *getValue(const char *colName, char *aValue, int n);
  NdbRecAttr *getValue(const NdbColumnImpl *, char *aValue, int n);
  static int wait(void *p, int aMillisecondNumber);
  int next(int *pOverrun);
  bool isConsistent();
  Uint32 getGCI();
  Uint32 getLatestGCI();

  NdbDictionary::Event::TableEvent getEventType();

  /*
  getOperation();
  getGCI();
  getLogType();
  */

  void print();
  void printAll();

  const NdbError & getNdbError() const;
  NdbError m_error;

  Ndb *m_ndb;
  NdbEventImpl *m_eventImpl;
  NdbGlobalEventBufferHandle *m_bufferHandle;

  NdbRecAttr *theFirstPkAttrs[2];
  NdbRecAttr *theCurrentPkAttrs[2];
  NdbRecAttr *theFirstDataAttrs[2];
  NdbRecAttr *theCurrentDataAttrs[2];

  NdbEventOperation::State m_state;
  Uint32 m_eventId;
  int m_bufferId;
  int m_bufferL;
  SubTableData *sdata;
  LinearSectionPtr ptr[3];
};

class NdbGlobalEventBuffer;
class NdbGlobalEventBufferHandle {
public:
  NdbGlobalEventBufferHandle (int MAX_NUMBER_ACTIVE_EVENTS);
  ~NdbGlobalEventBufferHandle ();
  //static NdbGlobalEventBufferHandle *init(int MAX_NUMBER_ACTIVE_EVENTS);

  // returns bufferId 0-N if ok otherwise -1
  int prepareAddSubscribeEvent(NdbEventOperationImpl *, int& hasSubscriber);
  void unprepareAddSubscribeEvent(int bufferId);
  void addSubscribeEvent(int bufferId,
			 NdbEventOperationImpl *ndbEventOperationImpl);

  void unprepareDropSubscribeEvent(int bufferId);
  int prepareDropSubscribeEvent(int bufferId, int& hasSubscriber);
  void dropSubscribeEvent(int bufferId);

  static int getDataL(const int bufferId,
		      SubTableData * &sdata,
		      LinearSectionPtr ptr[3],
		      int *pOverrun);
  static int insertDataL(int bufferId,
			 const SubTableData * const sdata,
			 LinearSectionPtr ptr[3]);
  static void latestGCI(int bufferId, Uint32 gci);
  static Uint32 getLatestGCI();
  static Uint32 getEventId(int bufferId);

  void group_lock();
  void group_unlock();
  int wait(int aMillisecondNumber);
  int m_bufferL;
private:
  friend class NdbGlobalEventBuffer;
  void addBufferId(int bufferId);
  void dropBufferId(int bufferId);

  struct NdbCondition *p_cond;
  int m_nids;
  int m_bufferIds[NDB_MAX_ACTIVE_EVENTS];
};

class NdbGlobalEventBuffer {
private:
  friend class NdbGlobalEventBufferHandle;
  void lockB(int bufferId);
  void unlockB(int bufferId);
  void group_lock();
  void group_unlock();
  void lock();
  void unlock();
  void add_drop_lock();
  void add_drop_unlock();

  NdbGlobalEventBuffer();
  ~NdbGlobalEventBuffer();

  void real_remove(NdbGlobalEventBufferHandle *h);
  void real_init(NdbGlobalEventBufferHandle *h,
		 int MAX_NUMBER_ACTIVE_EVENTS);

  int real_prepareAddSubscribeEvent(NdbGlobalEventBufferHandle *h,
				    NdbEventOperationImpl *,
				    int& hasSubscriber);
  void real_unprepareAddSubscribeEvent(int bufferId);
  void real_addSubscribeEvent(int bufferId, void *ndbEventOperation);

  void real_unprepareDropSubscribeEvent(int bufferId);
  int real_prepareDropSubscribeEvent(int bufferId,
				     int& hasSubscriber);
  void real_dropSubscribeEvent(int bufferId);

  int real_getDataL(const int bufferId,
		    SubTableData * &sdata,
		    LinearSectionPtr ptr[3],
		    int *pOverrun);
  int real_insertDataL(int bufferId,
		       const SubTableData * const sdata,
		       LinearSectionPtr ptr[3]);
  void real_latestGCI(int bufferId, Uint32 gci);
  Uint32 real_getLatestGCI();
  int copy_data_alloc(const SubTableData * const f_sdata,
		      LinearSectionPtr f_ptr[3],
		      SubTableData * &t_sdata,
		      LinearSectionPtr t_ptr[3]);

  int real_wait(NdbGlobalEventBufferHandle *, int aMillisecondNumber);
  int hasData(int bufferId);
  int ID (int bufferId) {return bufferId & 0xFF;};
  int NO (int bufferId) {return bufferId >> 16;};
  int NO_ID (int n, int bufferId) {return (n << 16) | ID(bufferId);};

  Vector<NdbGlobalEventBufferHandle*> m_handlers;

  // Global Mutex used for some things
  NdbMutex *p_add_drop_mutex;

  int m_group_lock_flag;
  Uint32 m_latestGCI;

  int m_no;
  int m_max;
#define MAX_SUBSCRIBERS_PER_EVENT 16
  struct BufItem {
    // local mutex for each event/buffer
    NdbMutex *p_buf_mutex;
    Uint32 gId;
    Uint32 eventType;
    struct Data {
      SubTableData *sdata;
      LinearSectionPtr ptr[3];
    } * data;

    struct Ps {
      NdbGlobalEventBufferHandle *theHandle;
      int b;
      int overrun;
      int bufferempty;
      //void *ndbEventOperation;
    } ps[MAX_SUBSCRIBERS_PER_EVENT];  // only supports 1 subscriber so far

    int subs;
    int f;
    int sz;
    int max_sz;
  };
  BufItem *m_buf;
};
#endif
