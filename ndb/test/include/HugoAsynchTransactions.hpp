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

#ifndef HUGO_ASYNCH_TRANSACTIONS_HPP
#define HUGO_ASYNCH_TRANSACTIONS_HPP


#include <NDBT.hpp>
#include <HugoCalculator.hpp>
#include <HugoTransactions.hpp>

class HugoAsynchTransactions : private HugoTransactions {
public:
  HugoAsynchTransactions(const NdbDictionary::Table&);
  ~HugoAsynchTransactions();
  int loadTableAsynch(Ndb*, 
		      int records = 0,
		      int batch = 1,
		      int trans = 1,
		      int operations = 1);
  int pkReadRecordsAsynch(Ndb*, 
			  int records = 0,
			  int batch= 1,
			  int trans = 1,
			  int operations = 1);
  int pkUpdateRecordsAsynch(Ndb*, 
			    int records = 0,
			    int batch= 1,
			    int trans = 1,
			    int operations = 1);
  int pkDelRecordsAsynch(Ndb*, 
			 int records = 0,
			 int batch = 1,
			 int trans = 1,
			 int operations = 1);
  void transactionCompleted();

  long getTransactionsCompleted();

private:  
  enum NDB_OPERATION {NO_INSERT, NO_UPDATE, NO_READ, NO_DELETE};

  void allocTransactions(int trans);
  void deallocTransactions();

  int executeAsynchOperation(Ndb*,		      
			     int records,
			     int batch,
			     int trans,
			     int operations,
			     NDB_OPERATION theOperation,
			     ExecType theType = Commit);

  long transactionsCompleted;
  int numTransactions;
  NdbConnection** transactions;
};



#endif

