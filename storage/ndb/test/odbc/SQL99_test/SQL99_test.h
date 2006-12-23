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


#include <ndb_types.h>
#include <NdbThread.h>
#include <NdbOut.hpp>
#include <NdbSleep.h> 
#include <NDBT.hpp>
#include <sqlext.h>
#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <windows.h>
//#include <process.h>

#define MAX_STR_LEN		128
#define MAX_TABLE_NAME	32
#define MAX_COL_NAME	32
#define MAX_SQL_STMT	2048
#define MAX_VALUE_LEN	32
#define MAX_CHAR_ATTR_LEN 24 
#define NUM_COL_ARITHM	2
#define FLTDEV			0.0001
//#define DBLDEV			0.000000001

#define REPORTERROR(fn, str) ReportError(fn, str, __FILE__, __LINE__)
#define REPORT(str) printf((str))

#define ATTR_TYPE_SWITCH(buffer, ptr, attr)	switch(attr){ \
											case T_INT:\
												sprintf((char*)(buffer),"%d", (int)(ptr)) ;\
												break ;\
											case T_FLOAT:\
												sprintf((char*)(buffer),"%f", (float)(ptr)) ;\
												break ;\
											default:\
												break ;\
											}

#define ATTR_TYPE_SWITCH_T(value, attr)		switch(attr){ \
											case T_INT:\
												printf("%d      \t", (int)(value)) ;\
												break ;\
											case T_FLOAT:\
												printf("%f      \t", (float)(value)) ;\
												break ;\
											default:\
												break ;\
											}

#define ATTR_TYPE_SWITCH_AGR(str, value_A, value_B, value_C, attr)	switch(attr){ \
												case T_INT:\
													printf("%s\t%d       %d\t\t\t%d\n\n", str, value_A, (int)value_B, (int)value_C) ; break ;\
												case T_FLOAT:\
													printf("%s\t%d       %f\t\t\t%d\n\n", str, value_A, value_B, (int)value_C) ; break ;\
												default:\
													break ;\
												}


#define ODBC_FN(fn, rc) rc = ((((fn)))) ; if(SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc){;}else ReportError("ODBC function", "failed in ", __FILE__, __LINE__)


typedef enum attr_type_tag {
	T_INT,
	T_FLOAT,
//	T_DOUBLE,
	T_CHAR
} attr_type ;

typedef enum aggr_fn_tag {
	FN_COUNT,
	FN_SUM,
	FN_AVG,
	FN_MAX,
	FN_MIN,
	FN_VARIANCE,
	FN_STDDEV
} aggr_fn ;

typedef enum join_type_tag {
	ITSELF,
	EQUI,
	NON_EQUI,
	INNER,
	OUTTER
} join_type ;

typedef enum arth_op_tag {
	MINUS,
	PLUS,
	MULTI,
	DIVIDE,
	MODULO
} arth_op ;

typedef struct ODBC_HANDLES_tag{
	SQLHENV     henv ;
	SQLHDBC     hdbc ;
	SQLHSTMT    hstmt ;
} ODBC_HANDLES ;

typedef enum handle_op_tag{
	GET,
	FREE
} handle_op ;

typedef enum test_case_tag {
	NUMERIC_DATA_TYPES,
	CHAR_DATA_TYPES,
	IDENTIFIERS,
	BASIC_QUERY,
	PREDICATE_SEARCH,
	DATA_MANIPULATION,
	NULL_SUPPORT,
	BASIC_CONSTRAINTS,
	TRANSACTION,
	SET_FUNCTIONS,
	BASIC_SCHEMA,
	JOINED_TABLE,
	ALL
} test_case ;

typedef enum status_tag{
	S_STOP,
	S_IDLE, 
	S_STARTED,
	S_GET_BUSY,
	S_BUSY,
	S_EXIT
} status ;

typedef enum type_tag {
	T_INSERT,
	T_READ,
	T_UPDATE,
	T_DELETE,
	T_READ_VERIFY,
	T_DELETE_VERIFY,
	T_WAIT
} type ;

typedef struct PARAMS_tag {
	int nThreadID ;
	int nError ;
	int nVerifyFlag ;
	status thread_status ;
	status report_status ;
	type op_type ;
	void* pThreadRef ;
	char szTableName[MAX_TABLE_NAME] ;
} PARAMS ;

typedef enum table_opt_tag {
	CREATE,
	DROP
} table_opt ;

static char szANSI[] ="0123456789ABCEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ;

void	ReportError(char* szFn, char* szBuffer, char* szFile, int iLine) ;
void	HandleError(void*, SQLSMALLINT) ;
SQLRETURN GetHandles(ODBC_HANDLES*, handle_op, bool) ;
SQLRETURN AggregateFn(aggr_fn, char*, int, double*, double*, attr_type) ;
SQLRETURN GetDriverAndSourceInfo(SQLHDBC) ;
SQLRETURN Join(char*, join_type) ;
SQLRETURN GetResults(SQLHSTMT) ;
int ArithOp(char*, int, float*, attr_type, arth_op) ;
void ParseArguments(int argc, const char** argv) ;
void* ThreadFnInt(void*) ;
void* ThreadFnFloat(void*) ;
//void* ThreadFnDouble(void*) ;
void* ThreadFnChar(void*) ; 
inline void AssignTableNames(char* szBuffer, int nTables) ;
SQLRETURN CreateDemoTables(char*, int, table_opt) ;
inline void StartThreads(PARAMS*, void*, int, char*, attr_type, UintPtr*) ;
inline void SetThreadOperationType(PARAMS*, type) ;
inline int  WaitForThreads(PARAMS*) ;
inline void StopThreads(PARAMS*, UintPtr*) ;
inline void PrintAll(char* szTableName, int, attr_type) ;
void AssignRefCharValues(char*, bool) ;

template <class T, class V>
int VerifyArthOp(V* tValue, float* tOperand, T* tRes, arth_op op){
	
	int nResult = 0 ;
	int nValue = 0, nOperand = 0 ;

	switch(op){
		case MINUS:
			if(FLTDEV < abs((*tValue - *tOperand) - *tRes))
				nResult = -1 ;
			break ;
		case PLUS:
			if(FLTDEV < abs((*tValue + *tOperand) - *tRes))
				nResult = -1 ;
			break ;
		case MULTI:
			if(FLTDEV < abs((*tValue * *tOperand) - *tRes))
				nResult = -1 ;
			break ;
		case DIVIDE:
			if(FLTDEV < abs((*tValue / *tOperand) - *tRes))
				nResult = -1 ;
			break ;
		case MODULO:
			nValue = *tValue ;
			nOperand = *tOperand ;
			if(*tRes != (nValue % nOperand))
				nResult = -1 ;
			break ;
		}
	
	return nResult ;
}

template <class P> void AssignRefNumValues(P* pRef, attr_type attrType, bool bVerbose) {

	int count = 0, rows = 0, nThreadOffset = 0, nRowOffset = 0 ;
	P* p = (P*)pRef ;

	float fRandomBase = (rand()*rand()) % 100;
	for(int c = 0 ; c < nNoOfThreads ; ++c){
		nThreadOffset = nNoOfRows*nNoOfCol*c ;
		for(int d = 0 ; d < nNoOfRows ; ++d){
			nRowOffset = nNoOfCol*d ; ++rows ;
			for(int i = 0 ; i < nNoOfCol ; ++i){
				(p[nThreadOffset + nRowOffset + i]) = (fRandomBase*(c+1) + (d+3)*7 + i)/1.1034093201 ;
				++count ;
				if(bVerbose){
					ATTR_TYPE_SWITCH_T(p[nThreadOffset + nRowOffset + i], AttributeType) ;
				}
			}
			if(bVerbose) { printf("\n") ; NdbSleep_MilliSleep(10) ;
			}
		}
	}
	
	if(bVerbose){
		printf("_____________________") ;
		printf("\nRows: %d Values: %d\n\n", rows, count) ;
	}

	return ;
} 


