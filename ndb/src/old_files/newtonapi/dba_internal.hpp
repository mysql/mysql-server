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

#ifndef DBA_INTERNAL_HPP
#define DBA_INTERNAL_HPP

#include <ndb_global.h>

extern "C" {
#include "dba.h"
}

#include <NdbApi.hpp>
#include <NdbMutex.h>
#include <NdbOut.hpp>

#ifndef INT_MAX
#define INT_MAX          2147483647
#endif

#ifdef DEBUG
#define DBA_DEBUG(x) ndbout << x << endl
#else
#define DBA_DEBUG(x)
#endif

extern Ndb      * DBA__TheNdb;
extern NdbMutex * DBA__TheNewtonMutex;

extern unsigned DBA__SentTransactions;
extern unsigned DBA__RecvTransactions;

/**
 * Configuration
 */
extern int DBA__NBP_Intervall;          // Param 0
extern int DBA__BulkReadCount;          // Param 1
extern int DBA__StartTransactionTimout; // Param 2
extern int DBA__NBP_Force;              // Param 3

/**
 * Error handling
 */
void DBA__SetLatestError(DBA_Error_t, DBA_ErrorCode_t, const char *, ...);

/**
 * Magic string
 *
 * Used to make sure that user passes correct pointers
 */
const int DBA__MagicLength = 4;
const char DBA__TheMagic[DBA__MagicLength] = { 'K', 'E', 'S', 'O' };

struct DBA_Binding {
  char magic[DBA__MagicLength];
  int  checkSum;
  
  char * tableName;
  int structSz;
  
  int noOfKeys;
  int *keyIds;
  int *keyOffsets;
  
  int noOfColumns;
  int *columnIds;
  int *columnOffsets;

  int noOfSubBindings;
  struct DBA_Binding **subBindings;
  int * subBindingOffsets;
  
  int data[1];
};

struct DBA__DataTypesMapping {
  DBA_DataTypes_t newtonType;
  NdbDictionary::Column::Type ndbType;
};

const DBA__DataTypesMapping DBA__DataTypesMappings[] = {
  { DBA_CHAR, NdbDictionary::Column::Char },
  { DBA_INT,  NdbDictionary::Column::Int }
};

const int DBA__NoOfMappings = sizeof(DBA__DataTypesMappings)/
                              sizeof(DBA__DataTypesMapping);

/**
 * Validate magic string and checksum of a binding
 */
bool DBA__ValidBinding(const DBA_Binding_t * bindings);
bool DBA__ValidBindings(const DBA_Binding_t * const * pBindings, int n);

/**
 * Recursive equalGetValue (used for read)
 *           equalSetValue (used for write)
 *           equal         (used for delete)
 */
bool DBA__EqualGetValue(NdbOperation *, const DBA_Binding_t *, void *);
bool DBA__EqualSetValue(NdbOperation *, const DBA_Binding_t *, const void *);
bool DBA__Equal        (NdbOperation *, const DBA_Binding_t *, const void *);

inline void require(bool test){
  if(!test)
    abort();
}

#endif
