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


#include "dba_internal.hpp"

int DBA__NBP_Intervall = 10;
int DBA__BulkReadCount = 1000;
int DBA__StartTransactionTimout = 0;
int DBA__NBP_Force = 1;

struct DBA__Config {
  int ParamId;
  int * Param;
  int min;
  int max;
  const char * Description;
};

static 
DBA__Config Parameters[] = {
  { 0, &DBA__NBP_Intervall, 4, INT_MAX, 
    "Newton Batch Process Interval(ms)" },
  { 1, &DBA__BulkReadCount, 1, 5000, 
    "Operations per transaction during bulkread" },
  { 2, &DBA__StartTransactionTimout, 0, INT_MAX,
    "Start transaction timeout(ms)" },
  { 3, &DBA__NBP_Force, 0, 2,
    "Newton Batch Process Force send algorithm" } 
};

static const int Params = sizeof(Parameters)/sizeof(DBA__Config);

static
DBA__Config *
getParam(int id){
  for(int i = 0; i<Params; i++)
    if(Parameters[i].ParamId == id)
      return &Parameters[i];
  return 0;
}


extern "C"
DBA_Error_t
DBA_SetParameter(int ParameterId, int Value){
  if(Value == -1){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "Node id is not modifyable");
    return DBA_APPLICATION_ERROR;
  }

  DBA__Config * p = getParam(ParameterId);

  if(p == 0){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "Invalid parameter id: %d",
			ParameterId);
    return DBA_APPLICATION_ERROR;
  }

  if(Value < p->min){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, 
			"Value too small for parameter %d (min = %d)",
			Value, p->min);
    return DBA_APPLICATION_ERROR;
  }

  if(Value > p->max){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, 
			"Value too big for parameter %d (max = %d)",
			Value, p->max);
    return DBA_APPLICATION_ERROR;
  }

  * p->Param = Value;
  return DBA_NO_ERROR;
}
  
extern "C"
DBA_Error_t
DBA_GetParameter(int ParameterId, int * Value){
  if(ParameterId == -1){
    if(DBA__TheNdb == 0){
      DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "DBA_Open() is not called"
			  );
      return DBA_APPLICATION_ERROR;
    }
    * Value = DBA__TheNdb->getNodeId();
    return DBA_NO_ERROR;
  }

  DBA__Config * p = getParam(ParameterId);
  if(p == 0){
    DBA__SetLatestError(DBA_APPLICATION_ERROR, 0, "Invalid parameter id: %d",
			ParameterId);
    return DBA_APPLICATION_ERROR;
  }
  
  * Value = * p->Param;

  return DBA_NO_ERROR;
}

