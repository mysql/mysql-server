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

// ODBC.cpp : Defines the entry point for the console application.
//

#include "SQL99_test.h"
#include <iostream> // Loose later

using namespace std; //

#define MAXCOL			64
#define DEFCOL			4

#define MAXROW			64
#define DEFROW			8

/*
  NDB_MAXTHREADS used to be just MAXTHREADS, which collides with a
  #define from <sys/thread.h> on AIX (IBM compiler).  We explicitly
  #undef it here lest someone use it by habit and get really funny
  results.  K&R says we may #undef non-existent symbols, so let's go.
*/
#undef MAXTHREADS
#define NDB_MAXTHREADS		24
#define DEFTHREADS		2

#define MAXTABLES		16
#define DEFTABLES		2

#define MAXLOOPS		100000
#define DEFLOOPS		4

#define UPDATE_VALUE	7

#define PKSIZE 2


static int nNoOfThreads = 1 ;
static int nNoOfCol = 4 ;
static int nNoOfRows = 2 ;
static int nNoOfLoops = 0 ;
static int nNoOfTables = 2 ;
static int nAPI = 0 ;
static int tAttributeSize = sizeof(char) ;
static attr_type AttributeType = T_CHAR ;
static int nAggregate = 0 ;
static int nArithmetic = 0 ;
static int nPrint = 0 ;
static int nColList = 0 ;
static char szColNames[MAXCOL*MAX_COL_NAME] = { 0 } ;
int createTables(char* szTableName, int nTables) ;


/*************************************************
Function: main - the entry point
*************************************************/
int main(int argc, char* argv[]){	

	int nRetrunValue = NDBT_FAILED ;
	SQLRETURN rc = SQL_ERROR ;
	double dResultA = 0 ;
    double dResultB = 0 ;
    double dInput = 0 ;
    int x = 0, y = 0 ;
    int* pIntRefBuffer = NULL ;
    float* pFloatRefBuffer = NULL ;
    double*	pDoubleRefBuffer = NULL ;
    char* pCharRefBuffer = NULL ;
	char szColBuffer[MAX_COL_NAME] = { 0 } ;


    ParseArguments(argc, (const char**)argv) ;

    PARAMS* pparams = (PARAMS*)malloc(sizeof(PARAMS)*nNoOfThreads) ;
    memset(pparams, 0, (sizeof(PARAMS)*nNoOfThreads)) ;

    char* szTableNames = (char*)malloc(sizeof(char)*nNoOfTables*MAX_TABLE_NAME) ;
    memset(szTableNames, 0, sizeof(char)*nNoOfTables*MAX_TABLE_NAME) ;

	UintPtr pThreadHandles[NDB_MAXTHREADS] = { NULL } ;

    AssignTableNames(szTableNames, nNoOfTables) ;

	if(nAPI){
		if(0 != createTables(szTableNames, nNoOfTables)){
			printf("Failed to create tables through NDB API; quitting...\n") ;
			NDBT_ProgramExit(NDBT_FAILED) ;
			return NDBT_FAILED ;
		}
	}else{
	
		//CreateDemoTables(szTableNames, nNoOfTables, DROP) ;
		rc = CreateDemoTables(szTableNames, nNoOfTables, CREATE) ;
		if(!(SQL_SUCCESS == rc || SQL_SUCCESS_WITH_INFO == rc)){
			printf("Failed to create tables, quiting now.\n") ;
			NDBT_ProgramExit(NDBT_FAILED) ;
			return NDBT_FAILED ;
		}
	}

	// Store column names in the buffer for use in some stmts
	int k = 0 ;
	for(;;){
		memset((char*)szColBuffer, 0, strlen(szColBuffer)) ;
		sprintf((char*)szColBuffer, "COL%d", k) ;
        strcat((char*)szColNames, (char*)szColBuffer) ;
        ++k ;
		if( k == nNoOfCol ){
			break ;
		}	
		strcat((char*)szColNames, ", ") ;
	} // for


    switch(AttributeType){
        case T_INT:
            pIntRefBuffer = (int*)malloc(sizeof(int)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            memset(pIntRefBuffer, 0, sizeof(int)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            AssignRefNumValues(pIntRefBuffer, T_INT, nPrint) ;
            StartThreads(pparams, (void*)pIntRefBuffer, nNoOfTables, szTableNames, AttributeType, pThreadHandles) ;
            break ;
        case T_FLOAT:
            pFloatRefBuffer = (float*)malloc(sizeof(float)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            memset(pFloatRefBuffer, 0, sizeof(float)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            AssignRefNumValues(pFloatRefBuffer, T_FLOAT, nPrint) ;
            StartThreads(pparams, (void*)pFloatRefBuffer, nNoOfTables, szTableNames, AttributeType, pThreadHandles) ;
            break ;
/*        case T_DOUBLE:
            pDoubleRefBuffer = (double*)malloc(sizeof(double)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            memset(pDoubleRefBuffer, 0, sizeof(double)*nNoOfRows*nNoOfCol*nNoOfThreads) ;
            AssignRefNumValues(pDoubleRefBuffer, T_DOUBLE, 0) ;
            StartThreads(pparams, (void*)pDoubleRefBuffer, nNoOfTables, szTableNames, AttributeType, pThreadHandles) ;
            break ;
*/
        case T_CHAR:
            pCharRefBuffer = (char*)malloc(sizeof(char)*nNoOfRows*nNoOfCol*nNoOfThreads*MAX_CHAR_ATTR_LEN) ;
            memset(pCharRefBuffer, 0, sizeof(char)*nNoOfRows*nNoOfCol*nNoOfThreads*MAX_CHAR_ATTR_LEN) ;
            AssignRefCharValues(pCharRefBuffer, nPrint ) ;
            StartThreads(pparams, (void*)pCharRefBuffer, nNoOfTables, szTableNames, AttributeType, pThreadHandles) ;
            break ;
        default:
            break ;
        }

	NdbThread_SetConcurrencyLevel(nNoOfThreads + 2) ;


	printf("\nPerforming inserts...") ;
	SetThreadOperationType(pparams, T_INSERT) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;
	PrintAll(szTableNames, nNoOfTables, AttributeType) ;

	printf("\nVerifying inserts...") ;
	SetThreadOperationType(pparams, T_READ_VERIFY) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;

	printf("\nPerforming updates...") ;
	SetThreadOperationType(pparams, T_UPDATE) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;
	//PrintAll(szTableNames, nNoOfTables, AttributeType) ;

	printf("\nVerifying updates...") ;
	SetThreadOperationType(pparams, T_READ_VERIFY) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;

	printf("\nPerforming reads...") ;
	SetThreadOperationType(pparams, T_READ) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;
	PrintAll(szTableNames, nNoOfTables, AttributeType) ;


	if(T_CHAR != AttributeType && nAggregate){
		printf("\nTesting aggregate functions for each table\n\n") ;
		printf("FN\tCOLUMN\tVALUE\t\t\tTOTAL ROWS WHERE\n\t\t\t\t\tVALUE(S) > VALUE\n--------------------------------------------------------\n\n") ;

		for(y = 0 ; y < nNoOfTables ; ++y){
			for(x = 0; x < nNoOfCol ; ++x){
				dResultA = dResultB = 0 ;
				AggregateFn(FN_MIN, (char*)(szTableNames + MAX_TABLE_NAME*y), x, NULL, &dResultA, AttributeType) ;
				AggregateFn(FN_COUNT, (char*)(szTableNames + MAX_TABLE_NAME*y) , x, &dResultA, &dResultB, AttributeType) ;
				ATTR_TYPE_SWITCH_AGR("MIN", x, dResultA, dResultB, AttributeType) ;	
				}
			}

			for(y = 0; y < nNoOfTables ; ++y){ 
				for(x = 0; x < nNoOfCol ; ++x){
					dResultA = dResultB = 0 ;
					AggregateFn(FN_MAX, (char*)(szTableNames + MAX_TABLE_NAME*y), x, NULL, &dResultA, AttributeType) ;
					AggregateFn(FN_COUNT, (char*)(szTableNames + MAX_TABLE_NAME*y), x, &dResultA, &dResultB, AttributeType) ;
					ATTR_TYPE_SWITCH_AGR("MAX", x, dResultA, dResultB, AttributeType) ;
					}
				}

			for(y = 0 ; y < nNoOfTables ; ++y){
				for(x = 0; x < nNoOfCol ; ++x){
					dResultA = dResultB = 0 ;
					AggregateFn(FN_AVG, (char*)(szTableNames + MAX_TABLE_NAME*y), x, NULL, &dResultA, AttributeType) ;
					AggregateFn(FN_COUNT, (char*)(szTableNames + MAX_TABLE_NAME*y), x, &dResultA, &dResultB, AttributeType) ;
					ATTR_TYPE_SWITCH_AGR("AVG", x, dResultA, dResultB, AttributeType)
					}
			}

			printf("--------------------------------------------------------\n\n") ;
	}

	if(T_CHAR != AttributeType && nArithmetic){

	float nVal = (rand() % 10) /1.82342 ;

		for(int h = 0 ; h < nNoOfTables ; ++h){

			printf("\nTesting arithmetic operators\nfor each column in %s:\n----------------------\n", (char*)(szTableNames + MAX_TABLE_NAME*sizeof(char)*h) ) ;

			printf("\nOperator [ * ]... \t\t") ;
			ArithOp((char*)(szTableNames + MAX_TABLE_NAME*sizeof(char)*h), nNoOfCol, &nVal, AttributeType, MULTI) ;
			printf("done\n") ;

			printf("\nOperator [ / ]... \t\t") ;
			ArithOp((char*)(szTableNames + MAX_TABLE_NAME*sizeof(char)*h), nNoOfCol, &nVal, AttributeType, DIVIDE) ;
			printf("done\n") ;

			printf("\nOperator [ + ]... \t\t") ;
			ArithOp((char*)(szTableNames + MAX_TABLE_NAME*sizeof(char)*h), nNoOfCol, &nVal, AttributeType, PLUS) ;
			printf("done\n") ;

			printf("\nOperator [ - ]... \t\t") ;
			ArithOp((char*)(szTableNames + MAX_TABLE_NAME*sizeof(char)*h), nNoOfCol, &nVal, AttributeType, MINUS) ;
			printf("done\n\n") ;
			/*	
			printf("\nOperator [ % ]... \t\t") ;
			ArithOp((char*)szTableNames, nNoOfCol, &nVal, AttributeType, MODULO) ;
			printf("done\n\n") ;
			*/	
		}
	}
/*
	printf("\nPerforming deletes...") ;
	SetThreadOperationType(pparams, T_DELETE) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;

	printf("\nVerifying deletes...") ;
	SetThreadOperationType(pparams, T_DELETE_VERIFY) ;
	if(0 < WaitForThreads(pparams)){
		printf("\t\t%d thread(s) failed\n") ;
	}else{
		printf("\t\tdone\n") ;
	}
	printf("----------------------\n\n") ;
*/
	StopThreads(pparams, pThreadHandles) ;

	//PrintAll(szTableNames, nNoOfTables, AttributeType) ;

	//CreateDemoTables(szTableNames, nNoOfTables, DROP) ;

	free((void*)szTableNames) ;
	free((void*)pparams) ;
	free((void*)pIntRefBuffer) ;
	free((void*)pFloatRefBuffer) ;
	free((void*)pDoubleRefBuffer) ;
	free((void*)pCharRefBuffer) ;

	return 0;
}



/**************************************************
Function: ParseArguments
***************************************************/
void ParseArguments(int argc, const char** argv){

    int i = 1;

    while (argc > 1){

        if (strcmp(argv[i], "-t") == 0)
            {
            nNoOfThreads = atoi(argv[i+1]);
            if ((nNoOfThreads < 1) || (nNoOfThreads > NDB_MAXTHREADS))
                nNoOfThreads = DEFTHREADS ;
            }
        else if (strcmp(argv[i], "-c") == 0)
            {
            nNoOfCol = atoi(argv[i+1]);
            if ((nNoOfCol < 2) || (nNoOfCol > MAXCOL))
                nNoOfCol = DEFCOL ;
            }
        else if (strcmp(argv[i], "-l") == 0)
            {
            nNoOfLoops = atoi(argv[i+1]);
            if ((nNoOfLoops < 0) || (nNoOfLoops > MAXLOOPS))
                nNoOfLoops = DEFLOOPS ;
            }
        else if (strcmp(argv[i], "-r") == 0)
            {
            nNoOfRows = atoi(argv[i+1]);;
            if ((nNoOfRows < 0) || (nNoOfRows > MAXROW))
                nNoOfRows = DEFROW ;
            }
		else if (strcmp(argv[i], "-m") == 0)
            {
            nArithmetic = 1 ;
			argc++ ;
			i-- ;
            }
		else if (strcmp(argv[i], "-g") == 0)
            {
            nAggregate = 1 ;
			argc++ ;
			i-- ;
            }
		else if (strcmp(argv[i], "-n") == 0)
            {
			nAPI = 1 ;
			argc++ ;
			i-- ;
            }
		else if (strcmp(argv[i], "-v") == 0)
            {
			nPrint = 1 ;
			argc++ ;
			i-- ;
            }
        else if (strcmp(argv[i], "-a") == 0)
            {
            if(strcmp(argv[i+1], "int") == 0){
                AttributeType = T_INT ;
				tAttributeSize = 32 ;
                }else if(strcmp(argv[i+1], "float") == 0){
                    AttributeType = T_FLOAT ;
					tAttributeSize = 64 ;
                    }else if(strcmp(argv[i+1], "char") == 0){
                        AttributeType = T_CHAR ;
                        }
            }
        else
            {
            cout << "Arguments:\n";
			cout << "-n Create tables using NDB API (vs ODBC by default)" << endl;
            cout << "-t Number of threads; maximum 24, default 2\n" << endl;
            cout << "-c Number of columns per table; maximum 64, default 4\n" << endl;
            cout << "-r Number of rows; maximum 64, default 8\n" << endl;
            cout << "-a Type of attribute to use: int, double or char; default int " << endl;
            cout << "-g Test aggregate functions" << endl;
			cout << "-m Test arithmetic operators" << endl;
			cout << "-v Print executed statements" << endl;
            exit(-1);
            }

        argc -= 2 ;
        i = i + 2 ;
        }

char *szAttrType[MAX_STR_LEN] = { 0 } ;
switch(AttributeType){
        case T_INT:
            strcpy((char*)szAttrType, "Integer") ;
            break ;
        case T_FLOAT:
            strcpy((char*)szAttrType, "Float") ;
            break ;
/*        case T_DOUBLE:
            strcpy((char*)szAttrType, "Double") ;
            break ;
*/
        case T_CHAR:
            strcpy((char*)szAttrType, "Character") ;
            break ;
        default:
            strcpy((char*)szAttrType, "Not defined") ;
            break ;
    }


printf("\n\nCurrent parameters: %d thread(s), %d tables, %d rows, %d colums, attribute type: %s\n\n", nNoOfThreads, nNoOfTables, nNoOfRows, nNoOfCol, szAttrType) ;
    }




/*************************************************
Function: ThreadFnInt - thread function
for int attributes
*************************************************/
void* ThreadFnInt(void* pparams){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR	szStmtBuffer[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szValueBuffer[MAX_VALUE_LEN] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLINTEGER cbInt = 0 ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;

    int r = 0, j = 0 ;
    //Get thread parameters 	
    PARAMS* p = (PARAMS*)pparams ;
    int* pRef = (int*)p->pThreadRef ;

    int* pBindBuffer = (int*)malloc(sizeof(int)*nNoOfCol) ;

    //printf("Thread #%d\n", p->nThreadID) ;

    retcode = GetHandles(&stHandles, GET, 0) ;
	
	if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ) {
		p->report_status = S_STARTED ;
	}else{
		printf("Thread #%d failed to allocate handles, exiting now.\n", p->nThreadID) ;
		free((void*)pBindBuffer) ;
		p->nError = 1 ;
		p->report_status = S_EXIT ;
		return 0 ;
	}
    
	//p->report_status = S_STARTED ;

    //Main thread loop
    for(;;){

        while(S_IDLE == p->thread_status){
            NdbSleep_MilliSleep(1) ;
            }

    if(S_STOP == p->thread_status) {
        break ;
        }else{
            p->thread_status = S_BUSY ;
            }

    switch(p->op_type){


        /************************************** T_INSERT case **************************************/
        case T_INSERT:

            for(r = 0 ; r < nNoOfRows ; ++r){

				if(!nColList){
					sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 
				}else{
					sprintf((char*)szStmtBuffer, "INSERT INTO %s (%s) VALUES(", p->szTableName, szColNames) ;
				}

                //sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 

                for(j = 0 ;;){
                    sprintf((char*)szValueBuffer,"%d", pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szValueBuffer, strlen((char*)szValueBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            strcat((char*)szStmtBuffer, ")") ;
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
            retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
			if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			}else{
				p->nError = 1 ;
				printf("INSERT in thread #%d failed\n", p->nThreadID) ;
				HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
			}
			}
        break ;


        /************************************** T_READ case **************************************/
        case T_READ:

            for(r = 0 ; r < nNoOfRows ; r++){

                sprintf((char*)szStmtBuffer, "SELECT * FROM %s WHERE COL0 = %d", p->szTableName, pRef[nNoOfCol*r]) ;
				if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
                ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS), retcode) ;

                for(j = 0 ; j < nNoOfCol ; ++j){
                    ODBC_FN(SQLBindCol(stHandles.hstmt, (j+1), SQL_C_SLONG, (void*)&pBindBuffer[j], sizeof(SQLINTEGER), &cbInt), retcode) ;
                    }

            for (;;) {
                retcode = SQLFetch(stHandles.hstmt);

                if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
                    for(int k = 0 ; k < nNoOfCol ; ++k){
                        if(p->nVerifyFlag){
                            if(pBindBuffer[k] != pRef[nNoOfCol*r + k])
                                printf("Expected: %d Actual: %d\n", pBindBuffer[k], pRef[nNoOfCol*r + k]) ;
                            }
                        }
                    }else if(SQL_NO_DATA == retcode){
                        break ;
                        }else{
							p->nError = 1 ;
							printf("READ in thread #%d failed\n", p->nThreadID) ;
                            HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
                            }
                    //printf("\n") ;
                }
				SQLCloseCursor(stHandles.hstmt) ;
			}
        break ;


        /************************************** T_UPDATE case **************************************/
        case T_UPDATE:
            for(r = 0 ; r < nNoOfRows ; ++r){

                sprintf((char*)szStmtBuffer, "UPDATE %s SET ", p->szTableName) ; 
                for(j = 1 ;;){
                    pRef[nNoOfCol*r + j] = pRef[nNoOfCol*r + j] + UPDATE_VALUE ;
                    sprintf((char*)szColBuffer,"COL%d = %d",  j, pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szColBuffer, strlen((char*)szColBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
				}
				sprintf((char*)szAuxBuffer, " WHERE COL0 = %d ;", pRef[nNoOfCol*r]) ;		
				strcat((char*)szStmtBuffer, (char*)szAuxBuffer);
				if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
				retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;

				if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
				}else{
					p->nError = 1 ;
					printf("UPDATE in thread %d failed\n", p->nThreadID) ;
					HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
				}
			}
        break ;


        /************************************** T_DELETE case **************************************/
        case T_DELETE:
            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "DELETE * FROM %s WHERE COL0 = %d", p->szTableName, pRef[nNoOfCol*r]) ;
                if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
				retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
				if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
				}else if( 1 == p->nVerifyFlag  && SQL_NO_DATA != retcode){
					p->nError = 1 ;
					printf("\nVerification failed: the row found\n") ;
                }else{
					p->nError = 1 ;
					printf("INSERT in thread %d failed\n", p->nThreadID) ;
					HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
				}

			}
        break ;


        /************************************** default case **************************************/
        default:
            break ;
        }//switch
p->thread_status = S_IDLE ;
        } //for

free((void*)pBindBuffer) ;
GetHandles(&stHandles, FREE, 0) ;
p->thread_status = S_EXIT ;
return 0 ;	
    };



/*************************************************
Function: ThreadFnFloat - thread function
for float attributes
*************************************************/
void* ThreadFnFloat(void* pparams){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR	szStmtBuffer[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szValueBuffer[MAX_VALUE_LEN] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLINTEGER cbFloat = 0 ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;

    int r = 0, j = 0 ;
    //Get thread parameters 	
    PARAMS* p = (PARAMS*)pparams ;

    float* pRef = (float*)p->pThreadRef ;
    float* pBindBuffer = (float*)malloc(sizeof(float)*nNoOfCol) ;

    //printf("Thread #%d\n", p->nThreadID) ;

    retcode = GetHandles(&stHandles, GET, 0) ;
	
	if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ) {
		p->report_status = S_STARTED ;
	}else{
		printf("Thread #%d failed to allocate handles, exiting now.\n", p->nThreadID) ;
		free((void*)pBindBuffer) ;
		p->nError = 1 ;
		p->report_status = S_EXIT ;
		return 0 ;
	}

    //p->report_status = S_STARTED ;

    //Main thread loop
    for(;;){

        while(S_IDLE == p->thread_status){
            NdbSleep_MilliSleep(1) ;
            }

    if(S_STOP == p->thread_status) {
        break ;
        }else{
            p->thread_status = S_BUSY ;
            }

    switch(p->op_type){


        /************************************** T_INSERT case **************************************/
        case T_INSERT:

            for(r = 0 ; r < nNoOfRows ; ++r){

				if(!nColList){
					sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 
				}else{
					sprintf((char*)szStmtBuffer, "INSERT INTO %s (%s) VALUES(", p->szTableName, szColNames) ;
				}

                //sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 

                for(j = 0 ;;){
                    sprintf((char*)szValueBuffer,"%f", pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szValueBuffer, strlen((const char*)szValueBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            strcat((char*)szStmtBuffer, ")") ;
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
            retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
			if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			}else{
				p->nError = 1 ;
				printf("INSERT in thread #%d failed\n", p->nThreadID) ;
				HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
			}
                }
        break ;


        /************************************** T_READ case **************************************/
        case T_READ:

            for(r = 0 ; r < nNoOfRows ; ++r){

                sprintf((char*)szStmtBuffer, "SELECT * FROM %s WHERE COL0 = %f", p->szTableName, pRef[nNoOfCol*r]) ;
				if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
                ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS), retcode) ;

                for(j = 0 ; j < nNoOfCol ; ++j){
                    ODBC_FN(SQLBindCol(stHandles.hstmt, (j+1), SQL_C_FLOAT, (void*)&pBindBuffer[j], sizeof(SQLFLOAT), &cbFloat), retcode) ;
                    }

            for (;;) {
                retcode = SQLFetch(stHandles.hstmt);

                if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
                    for(int k = 0 ; k < nNoOfCol ; ++k){
                        if(p->nVerifyFlag){
                            if(abs(pBindBuffer[k] - pRef[nNoOfCol*r + k]) > FLTDEV )
                                printf("Expected: %f Actual: %f\n", pBindBuffer[k], pRef[nNoOfCol*r + k]) ;
                            }
                        }
                    }else if(SQL_NO_DATA == retcode){
                        break ;
					}else{
						p->nError = 1 ;
						printf("READ in thread #%d failed\n", p->nThreadID) ;
						HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
					}
                    //printf("\n") ;
                }
        SQLCloseCursor(stHandles.hstmt) ;
                }
        break ;


        /************************************** T_UPDATE case **************************************/
        case T_UPDATE:
            for(r = 0 ; r < nNoOfRows ; ++r){

                sprintf((char*)szStmtBuffer, "UPDATE %s SET ", p->szTableName) ; 
                for(j = 1 ;;){
                    pRef[nNoOfCol*r + j] = pRef[nNoOfCol*r + j] + UPDATE_VALUE ;
                    sprintf((char*)szColBuffer,"COL%d = %f",  j, pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szColBuffer, strlen((char*)szColBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            sprintf((char*)szAuxBuffer, " WHERE COL0 = %f ;", pRef[nNoOfCol*r]) ;		
            strcat((char*)szStmtBuffer, (char*)szAuxBuffer);
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
			retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
			if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			}else{
				p->nError = 1 ;
				printf("UPDATE in thread #%d failed\n", p->nThreadID) ;
				HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
			}
			}
        break ;


        /************************************** T_DELETE case **************************************/
        case T_DELETE:
            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "DELETE * FROM %s WHERE COL0 = %f", p->szTableName, pRef[nNoOfCol*r]) ;
                if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
				retcode = SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS) ;
				if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
				}else if( 1 == p->nVerifyFlag && SQL_NO_DATA != retcode){
					p->nError = 1 ;
                    printf("\nVerification failed: still row exists\n") ;
				}else{
					p->nError = 1 ;
					printf("DELETE in thread #%d failed\n", p->nThreadID) ;
					HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
				}
			}
        break ;


        /************************************** default case **************************************/
        default:
            break ;
        }//switch
p->thread_status = S_IDLE ;
        } //for

free((void*)pBindBuffer) ;
GetHandles(&stHandles, FREE, 0) ;
p->thread_status = S_EXIT ;
return 0 ;	
    };



/*************************************************
Function: ThreadFnDouble - thread function
for double attributes
*************************************************/
/*
void* ThreadFnDouble(void* pparams){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR	szStmtBuffer[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szValueBuffer[MAX_VALUE_LEN] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLINTEGER cbDouble = 0 ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;
    int r = 0, j = 0 ;

    //Get thread parameters 	
    PARAMS* p = (PARAMS*)pparams ;
    double* pRef = (double*)p->pThreadRef ;

    double* pBindBuffer = (double*)malloc(sizeof(double)*nNoOfCol) ;

    //printf("Thread #%d\n", p->nThreadID) ;

    retcode = GetHandles(&stHandles, GET, 0) ;
	
	if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ) {
		p->report_status = S_STARTED ;
	}else{
		printf("Thread #%d failed to allocate handles, exiting now.\n", p->nThreadID) ;
		free((void*)pBindBuffer) ;
		p->report_status = S_EXIT ;
		return 0 ;
	}
    //p->report_status = S_STARTED ;

    //Main thread loop
    for(;;){

        while(S_IDLE == p->thread_status){
            NdbSleep_MilliSleep(1) ;
            }

    if(S_STOP == p->thread_status) {
        break ;
        }else{
            p->thread_status = S_BUSY ;
            }

    switch(p->op_type){


  /************************************** T_INSERT case **************************************/
  /*     case T_INSERT:

            for(r = 0 ; r < nNoOfRows ; ++r){	
                sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 
                for(j = 0 ;;){
                    sprintf((char*)szValueBuffer,"%.9f", pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szValueBuffer, strlen((const char*)szValueBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            strcat((char*)szStmtBuffer, ")") ;
            ODBC_FN(SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS), retcode) ;
                }

        break ;


  /************************************** T_READ case **************************************/
  /*      case T_READ:

            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "SELECT * FROM %s WHERE COL0 = %.9f", p->szTableName, pRef[nNoOfCol*r]) ;
                ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS), retcode) ;
                for(j = 0 ; j < nNoOfCol ; ++j){
                    ODBC_FN(SQLBindCol(stHandles.hstmt, (j+1), SQL_C_DOUBLE, (void*)&pBindBuffer[j], sizeof(SQLDOUBLE), &cbDouble), retcode) ;
                    }
            for (;;) {
                retcode = SQLFetch(stHandles.hstmt);

                if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
                    for(int k = 0 ; k < nNoOfCol ; ++k){
                        if(p->nVerifyFlag){
                            if(abs(pBindBuffer[k] - pRef[nNoOfCol*r + k]) > DBLDEV)
                                printf("Expected: %.9f Actual: %.9f\n", pBindBuffer[k], pRef[nNoOfCol*r + k]) ;
                            }
                        }
                    }else if(SQL_NO_DATA == retcode){
                        break ;
                        }else{
                            HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
                            }
                    //printf("\n") ;
                }
        SQLCloseCursor(stHandles.hstmt) ;
                }
        break ;


    /************************************** T_UPDATE case **************************************/
    /*    case T_UPDATE:
            for(r = 0 ; r < nNoOfRows ; ++r){

                sprintf((char*)szStmtBuffer, "UPDATE %s SET ", p->szTableName) ; 
                for(j = 1 ;;){
                    pRef[nNoOfCol*r + j] = pRef[nNoOfCol*r + j] + UPDATE_VALUE ;
                    sprintf((char*)szColBuffer,"COL%d = %.9f",  j, pRef[nNoOfCol*r + j]) ;
                    strncat((char*)szStmtBuffer, (char*)szColBuffer, strlen((char*)szColBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            sprintf((char*)szAuxBuffer, " WHERE COL0 = %.9f ;", pRef[nNoOfCol*r]) ;		
            strcat((char*)szStmtBuffer, (char*)szAuxBuffer);	
            ODBC_FN(SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS), retcode) ;
                }
        break ;


  /************************************** T_DELETE case **************************************/
  /*      case T_DELETE:
            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "DELETE FROM %s WHERE COL0 = %.9f", p->szTableName, pRef[nNoOfCol*r]) ;
                retcode = SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS) ;
                if( 1 == p->nVerifyFlag && SQL_NO_DATA != retcode ){
                    printf("\nVerification failed: still row exists\n") ;
                    }
                }
        break ;


    /************************************** default case **************************************/
    /*    default:
            break ;
        }//switch
p->thread_status = S_IDLE ;
        } //for

free((void*)pBindBuffer) ;
GetHandles(&stHandles, FREE, 0) ;
p->thread_status = S_EXIT ;
return 0 ;	
    };



/*************************************************
Function: ThreadFnChar - thread function
for character attributes
*************************************************/
void* ThreadFnChar(void* pparams){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR	szStmtBuffer[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szValueBuffer[MAX_VALUE_LEN] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLINTEGER cbChar = 0 ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;
    int r = 0, j = 0 ;

    //Get thread parameters 	
    PARAMS* p = (PARAMS*)pparams ;
    char* pRef = (char*)p->pThreadRef ;
    char* pBindBuffer = (char*)malloc(sizeof(char)*nNoOfCol*MAX_CHAR_ATTR_LEN) ;

    //printf("Thread #%d\n", p->nThreadID) ;

    retcode = GetHandles(&stHandles, GET, 0) ;
	
	if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ) {
		p->report_status = S_STARTED ;
	}else{
		printf("Thread #%d failed to allocate handles, retcode = %d, exiting now.\n", p->nThreadID, retcode) ;
		p->nError = 1 ;
		free((void*)pBindBuffer) ;
		p->report_status = S_EXIT ;
		return 0 ;
	}

    //Main thread loop
    for(;;){
		
		while(S_IDLE == p->thread_status){
            NdbSleep_MilliSleep(1) ;
		}

		if(S_STOP == p->thread_status) {
			break ;
        }else{
            p->thread_status = S_BUSY ;
		}

		switch(p->op_type){


        /************************************** T_INSERT case **************************************/
        case T_INSERT:

            for(r = 0 ; r < nNoOfRows ; ++r){
                memset(szStmtBuffer, 0, strlen(szStmtBuffer)) ;
				if(!nColList){
					sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 
				}else{
					sprintf((char*)szStmtBuffer, "INSERT INTO %s (%s) VALUES(", p->szTableName, szColNames) ;
				}

                for(j = 0 ;;){
                    sprintf((char*)szValueBuffer,"'%s'", (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + j*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
                    strncat((char*)szStmtBuffer, (char*)szValueBuffer, strlen((const char*)szValueBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            strcat((char*)szStmtBuffer, ")") ;
			
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
			retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
			if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			}else{
				p->nError = 1 ;
				printf("INSERT in thread #%d failed\n", p->nThreadID) ;
				HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
			}
			}

        break ;


        /************************************** T_READ case **************************************/
        case T_READ:

            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "SELECT * FROM %s WHERE COL0 = '%s'", p->szTableName, (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
       			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
				ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmtBuffer, SQL_NTS), retcode) ;
                for(j = 0 ; j < nNoOfCol ; ++j){
                    ODBC_FN(SQLBindCol(stHandles.hstmt, (j+1), SQL_C_CHAR, (void*)(pBindBuffer+j*MAX_CHAR_ATTR_LEN*sizeof(char)), MAX_CHAR_ATTR_LEN, &cbChar), retcode) ;
                    }
            for (;;) {
				retcode = SQLFetch(stHandles.hstmt);

                if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
					for(int k = 0 ; k < nNoOfCol ; ++k){
						if(p->nVerifyFlag){
                            if(!strcmp((char*)(pBindBuffer + k*MAX_CHAR_ATTR_LEN*sizeof(char)), (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + k*MAX_CHAR_ATTR_LEN*sizeof(char))))
                                printf("Expected: %s Actual: %s\n", (char*)(pBindBuffer + k*MAX_CHAR_ATTR_LEN*sizeof(char)), (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + k*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
                            }
					}
				}else if(SQL_NO_DATA == retcode){
					break ;
				}else{
					p->nError = 1 ;
					printf("READ in thread #%d failed\n", p->nThreadID) ;
					HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
				}
                    //printf("\n") ;
			}
			SQLCloseCursor(stHandles.hstmt) ;
			}
        break ;


        /************************************** T_UPDATE case **************************************/
        case T_UPDATE:
            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "UPDATE %s SET ", p->szTableName) ; 
                for(j = 1 ;;){
                    swab((char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + j*MAX_CHAR_ATTR_LEN*sizeof(char)), (char*)szColBuffer, MAX_CHAR_ATTR_LEN*sizeof(char)) ;
                    memcpy((void*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + j*MAX_CHAR_ATTR_LEN*sizeof(char)), (void*)szColBuffer, MAX_CHAR_ATTR_LEN*sizeof(char)) ;					
                    sprintf((char*)szColBuffer,"COL%d = '%s'",  j, (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char) + j*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
                    strncat((char*)szStmtBuffer, (char*)szColBuffer, strlen((char*)szColBuffer)) ;
                    ++j ;
                    if(nNoOfCol == j) break ;
                    strcat((char*)szStmtBuffer, ", ") ;
                    }
            sprintf( (char*)szAuxBuffer, " WHERE COL0 = '%s';", (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char)) ) ;		
            strcat((char*)szStmtBuffer, (char*)szAuxBuffer) ;
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
            retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
			if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			}else{
				p->nError = 1 ;
				printf("UPDATE in thread #%d failed\n", p->nThreadID) ;
				HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
			}
			}
        break ;


        /************************************** T_DELETE case **************************************/
        case T_DELETE:
            for(r = 0 ; r < nNoOfRows ; ++r){
                sprintf((char*)szStmtBuffer, "DELETE FROM %s WHERE COL0 = '%s\'", p->szTableName, (char*)(pRef + nNoOfCol*r*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
            	if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
				retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
				if(SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
				}else if(1 == p->nVerifyFlag && SQL_NO_DATA != retcode){
                    p->nError = 1 ;
					printf("\nVerification failed: still row exists\n") ;
				}else{
					p->nError = 1 ;
					printf("INSERT in thread #%d failed\n", p->nThreadID) ;
					HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
                }
			}
        break ;


        /************************************** default case **************************************/
        default:
            break ;
		}//switch
	p->thread_status = S_IDLE ;
	} //for

	free((void*)pBindBuffer) ;
	GetHandles(&stHandles, FREE, 0) ;
	p->thread_status = S_EXIT ;
	return 0 ;	
};



/*************************************************
Function: CreateDemoTable
*************************************************/
SQLRETURN CreateDemoTables(char* szTableName, int nTables, table_opt op){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR	szStmtBuffer[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
	SQLCHAR szAuxBuffer[32] = { 0 } ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;
    int c = 0  ;
	
	GetHandles(&stHandles, GET, 0) ;

    if(CREATE == op){
		
        for(c = 0; c < nTables ; ++c){
			sprintf((char*)szStmtBuffer, "CREATE TABLE %s (", (char*)(szTableName+MAX_TABLE_NAME*c)) ;
            int j = 0 ;
			for(;;){
				sprintf((char*)szColBuffer, "COL%d ", j) ;
                strcat((char*)szStmtBuffer, (char*)szColBuffer) ;
                ++j ;

                switch(AttributeType){
                    case T_INT:
                        strcat((char*)szStmtBuffer, "INTEGER") ;
                        break ;
                    case T_FLOAT:
                        strcat((char*)szStmtBuffer, "FLOAT") ;
                        break ;

/*                    case T_DOUBLE:
                        strcat((char*)szStmtBuffer, "DOUBLE") ;
                        break ;
*/
                    case T_CHAR:
						sprintf((char*)szAuxBuffer, "CHAR(%d)", MAX_CHAR_ATTR_LEN) ;
						strcat((char*)szStmtBuffer,  (char*)szAuxBuffer) ;
                        break ;
                    default:
                        break ;
				}

				if(nNoOfCol <= j){	
					strcat((char*)szStmtBuffer, ")") ;
					break ;
				}	
				strcat((char*)szStmtBuffer, ", ") ;				
            } //for(;;)
			
			
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
			ODBC_FN(SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS), retcode) ;
			if(SQL_SUCCESS != retcode) HandleError(stHandles.hstmt , SQL_HANDLE_STMT) ;

			
		}// for()

	}else{
		
		for(c = 0 ; c < nTables ; ++c){
			sprintf((char*)szStmtBuffer, "DROP TABLE %s ", (char*)(szTableName + MAX_TABLE_NAME*c)) ;
			//ODBC_FN(SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS), retcode) ;
			if(nPrint) printf("\n> %s\n", szStmtBuffer) ;
			retcode = SQLExecDirect(stHandles.hstmt, szStmtBuffer, SQL_NTS) ;
		}

	}

    GetHandles(&stHandles, FREE, 0) ;

    return retcode ;
}



/*************************************************
Function: AssignTableNames()
*************************************************/
inline void AssignTableNames(char* szBuffer, int nTables){
    for(int c = 0 ; c < nTables ; ++c){
        sprintf((char*)(szBuffer + MAX_TABLE_NAME*sizeof(char)*c), "TAB%d", c) ;
        }
return ;
    }




/*************************************************
Function: StartThreads()
*************************************************/

inline void StartThreads(PARAMS* p, void* pRef, int nTables, char* szTables, attr_type attrType, UintPtr* pHandles) {

    int* pInt = NULL ;
    float* pFloat = NULL ;
    double* pDouble = NULL ;
    char* pChar = NULL ;
	UintPtr pTmpThread = NULL ;

    bool bFlap = 1 ;
    for(int f = 0 ; f < nNoOfThreads ; ++f){
        p[f].nThreadID = f ;
        p[f].nError = 0 ;
        p[f].thread_status = S_IDLE ;
        p[f].op_type = T_WAIT ;
        if(bFlap){
			strncpy((char*)p[f].szTableName, (char*)szTables, MAX_TABLE_NAME) ;
		}else{
            strncpy((char*)p[f].szTableName, (char*)(szTables + MAX_TABLE_NAME*sizeof(char)), MAX_TABLE_NAME) ;
		}
        bFlap = !bFlap ;
		//pTmpThread = pHandles[ ;

        switch(attrType){
            case T_INT:
                pInt = (int*)pRef ;
                p[f].pThreadRef = (void*)&pInt[nNoOfRows*nNoOfCol*f] ;
                pHandles[f] = (UintPtr)NdbThread_Create(ThreadFnInt, (void**)&p[f], 32768, "SQL99_test", NDB_THREAD_PRIO_MEAN) ;
                break ;
            case T_FLOAT:
                pFloat = (float*)pRef ;
                p[f].pThreadRef = (void*)&pFloat[nNoOfRows*nNoOfCol*f] ;
                pHandles[f] = (UintPtr)NdbThread_Create(ThreadFnFloat, (void**)&p[f], 32768, "SQL99_test", NDB_THREAD_PRIO_MEAN) ;
                break ;
            /*
			case T_DOUBLE:
                pDouble = (double*)pRef ;
                p[f].pThreadRef = (void*)&pDouble[nNoOfRows*nNoOfCol*f] ;
                pHandles[f] = (UintPtr)NdbThread_Create(ThreadFnDouble, (void**)&p[f], 32768, "SQL99_test", NDB_THREAD_PRIO_MEAN) ;
                break ;
			*/
            case T_CHAR:
                pChar = (char*)pRef ;
                p[f].pThreadRef = (void*)&pChar[nNoOfRows*nNoOfCol*f*MAX_CHAR_ATTR_LEN] ;
                pHandles[f] = (UintPtr)NdbThread_Create(ThreadFnChar,  (void**)&p[f], 32768, "SQL99_test", NDB_THREAD_PRIO_MEAN) ;
            default:
                break ;
            }
    while(!(S_STARTED != p[f].report_status || S_EXIT != p[f].report_status)){
        NdbSleep_MilliSleep(1) ;
	}
        }
	return ;
}



/*************************************************
Function: SetThreadOperationType()
*************************************************/
inline void SetThreadOperationType(PARAMS* p, type op){

    for(int e = 0 ; e < nNoOfThreads ; ++e){	
        p[e].nVerifyFlag = 0 ;
        if(T_READ_VERIFY == op){
            p[e].nVerifyFlag = 1 ;
            p[e].op_type = T_READ ;
            }else if(T_DELETE_VERIFY == op){
                p[e].nVerifyFlag = 1 ;
                p[e].op_type = T_DELETE ;
                }else{
                    p[e].op_type = op ;
                    }
            p[e].thread_status = S_GET_BUSY ;
        }
return ;
    }



/*************************************************
Function: WaitForThreads()
*************************************************/
inline int WaitForThreads(PARAMS* p) {
	
	int ret_value = 0 ;
    for(int w = 0 ; w < nNoOfThreads ; ++w){
        while(!(S_IDLE != p[w].thread_status || S_EXIT != p[w].report_status)) {
            NdbSleep_MilliSleep(1) ;
		}
		ret_value += p[w].nError ;
	}
	return ret_value ;
}



/*************************************************
Function: StopThreads()
*************************************************/
inline void StopThreads(PARAMS* p, UintPtr* pHandles) {

    for(int k = 0 ; k < nNoOfThreads ; ++k){
		while(!(S_IDLE != p[k].thread_status || S_EXIT != p[k].report_status)){
			NdbSleep_MilliSleep(1) ;
		}
		p[k].thread_status = S_STOP ;
		while(!(S_EXIT != p[k].thread_status || S_EXIT != p[k].report_status)){
			NdbSleep_MilliSleep(1) ;
		}
		NdbThread_Destroy((NdbThread**)&pHandles[k]) ;
	}

	return ;
}



/*************************************************
Function: PrintAll()
*************************************************/
inline void PrintAll(char* szTableName, int nTables, attr_type attrType){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR* szStmt[MAX_SQL_STMT] = { 0 } ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;
    double* pDoubleBuffer = NULL ;
    char* pCharBuffer = NULL ;

    if(T_CHAR != attrType){
        pDoubleBuffer = (double*)malloc(sizeof(double)*nNoOfCol) ;
	}else{
         pCharBuffer = (char*)malloc(sizeof(char)*nNoOfCol*MAX_CHAR_ATTR_LEN) ;
	}

    SQLINTEGER   cbLen = 0 ;

    GetHandles(&stHandles, GET, 0) ;

    for(int c = 0 ; c < nTables ; ++c){

        int nCol = 0, nRows = 0 ;

        printf("Table: \"%s\":\n------------------\n", (char*)(szTableName + MAX_TABLE_NAME*c*sizeof(char))) ;

        sprintf((char*)szStmt, "SELECT * FROM %s", (char*)(szTableName + MAX_TABLE_NAME*c*sizeof(char))) ;
		if(nPrint) printf("\n> %s\n", szStmt) ;
        ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmt, SQL_NTS), retcode) ;

        for(int i = 0 ; i < nNoOfCol ; ++i){

            if(T_CHAR != attrType){
                ODBC_FN(SQLBindCol(stHandles.hstmt, (i+1), SQL_C_DOUBLE, (void*)&pDoubleBuffer[i], sizeof(SQLDOUBLE), &cbLen), retcode) ;
			}else{
                ODBC_FN(SQLBindCol(stHandles.hstmt, (i+1), SQL_C_CHAR, (void*)(pCharBuffer + i*MAX_CHAR_ATTR_LEN*sizeof(char)), MAX_CHAR_ATTR_LEN*sizeof(char), &cbLen), retcode) ;
			}
            nCol++ ;

		}

    int k = 0 ;	//out of the <for> loop
    for (;;) {

        retcode = SQLFetch(stHandles.hstmt);			
        if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ){
            for(k = 0 ; k < nNoOfCol ; ++k){
                if(T_CHAR != attrType){
                    ATTR_TYPE_SWITCH_T(pDoubleBuffer[k], AttributeType) ;
                    }else{
                        printf("%s\t", (char*)(pCharBuffer + k*MAX_CHAR_ATTR_LEN)) ;
					}
                }
            }else if(SQL_NO_DATA == retcode){
                if(0 == k){
                    printf("<empty>\n") ;
                    break ;
                    }else{
                        break ;
					}
                }else{
                    HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
				}

            ++nRows ;
            printf("\n") ;
        }

	SQLCloseCursor(stHandles.hstmt) ;

	printf("------------------\n") ;
	printf("Rows: %d Columns: %d\n\n", nRows, nCol) ;

			}

	free((void*)pDoubleBuffer) ;
	free((void*)pCharBuffer) ;
	GetHandles(&stHandles, FREE, 0) ;

	return ;
}



/*************************************************
Function: AssignRefCharValues()
*************************************************/
void AssignRefCharValues(char* pRef, bool bVerbose) {

    int count = 0, rows = 0, nThreadOffset = 0, nRowOffset = 0 ;
    char szStrBuffer[MAX_CHAR_ATTR_LEN] = { 0 } ;
    int char_count = sizeof(szANSI)/sizeof(char) ;

    for(int c = 0 ; c < nNoOfThreads ; ++c){
        nThreadOffset = nNoOfRows*nNoOfCol*c*MAX_CHAR_ATTR_LEN*sizeof(char) ;
        for(int d = 0 ; d < nNoOfRows ; ++d){
            nRowOffset = nNoOfCol*d*MAX_CHAR_ATTR_LEN*sizeof(char) ; ++rows ;
            for(int i = 0 ; i < nNoOfCol ; ++i){
                for(int j = 0 ; j < (MAX_CHAR_ATTR_LEN - 2) ; ++j){
                    int h = (char)(rand() % (char_count-1)) ;
                    szStrBuffer[j] = szANSI[h] ;
                    }
            szStrBuffer[MAX_CHAR_ATTR_LEN - 1] = '\0' ;

            strcpy((char*)(pRef + nThreadOffset + nRowOffset + i*MAX_CHAR_ATTR_LEN*sizeof(char)), (char*)szStrBuffer) ;
            count++ ;
            if(bVerbose){
                printf(" %s ", (char*)(pRef + nThreadOffset + nRowOffset + i*MAX_CHAR_ATTR_LEN*sizeof(char))) ;
                }
                }
        if(bVerbose) { 
            printf("\n") ;
            NdbSleep_MilliSleep(10) ;
            }
            }
        }

if(bVerbose){
    printf("_____________________") ;
    printf("\nRows: %d Values: %d\n\n", rows, count) ;
    }

return ;
    }


/*


sprintf((char*)szStmtBuffer, "INSERT INTO %s VALUES(", p->szTableName) ; 				
for(j = 0 ;;){
strcat((char*)szStmtBuffer, "?") ;
++j ;
if(nNoOfCol == j) break ;
strcat((char*)szStmtBuffer, ", ") ;
}
strcat((char*)szStmtBuffer, ")") ;

ODBC_FN(SQLPrepare(stHandles.hstmt, szStmtBuffer, SQL_NTS), retcode) ;

for(j = 0 ; j < nNoOfCol ; ++j){
ODBC_FN(SQLBindParameter(stHandles.hstmt, (j+1), SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_FLOAT, 0, 0, (void*)&pBindBuffer[j], 0, &cbFloat), retcode) ;
HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
}

for(r = 0 ; r < nNoOfRows ; ++r){
for(j = 0 ; j < nNoOfCol ; ++j){
pBindBuffer[j] = pRef[nNoOfCol*r + j] ;
}
ODBC_FN(SQLExecute(stHandles.hstmt), retcode) ;
HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
}

*/




/*************************************************
Function: HandleError
*************************************************/

void HandleError(void* handle, SQLSMALLINT HandleType){

    SQLCHAR szError[MAX_STR_LEN], szSqlState[32] ;
    SQLINTEGER nError = 0 ;
    SQLSMALLINT nHandleType = HandleType ;
    SQLSMALLINT nLength = 0 ;
    SQLHANDLE SQLHandle = handle ;
    SQLGetDiagRec(nHandleType, SQLHandle, 1, szSqlState, &nError, szError, 128, &nLength) ;
    printf("Error: %s\nSqlState: %s\n", szError, szSqlState) ;

    return ;
    }



/*************************************************
Function: ReportError
*************************************************/

void ReportError(char* szFn, char* szBuffer, char* szFile, int iLine){

    printf("%s %s\nFile: %s\nLine: %d\n", szFn, szBuffer, szFile, iLine) ;

    return ;
}



/*************************************************
Function: GetHandles()
*************************************************/

SQLRETURN GetHandles(ODBC_HANDLES* pHandles, handle_op op, bool bDriverInfo){

    SQLRETURN   retcode = SQL_ERROR ;

    if(GET == op){
		
		retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &pHandles->henv);

        if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ) {
			retcode = SQLSetEnvAttr(pHandles->henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC2, 0);

            if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode) {
				retcode = SQLAllocHandle(SQL_HANDLE_DBC, pHandles->henv, &pHandles->hdbc);

                if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode) {
                    
					//SQLSetConnectAttr(pHandles->hdbc, SQL_LOGIN_TIMEOUT, (void*)5, 0);
					
					retcode = SQLConnect(pHandles->hdbc, (SQLCHAR*)"", SQL_NTS, (SQLCHAR*)"", SQL_NTS, (SQLCHAR*)"", SQL_NTS ) ;
					
					SQL_SUCCESS == SQLSetConnectAttr(pHandles->hdbc, SQL_ATTR_AUTOCOMMIT, (void*)SQL_AUTOCOMMIT_ON, 0) ;
						//printf("AUTOCOMMIT is on\n") ; 
					
					if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode)	{
						// retcode still holds the value returned by SQLConnect
					    retcode = SQLAllocHandle(SQL_HANDLE_STMT, pHandles->hdbc, &pHandles->hstmt) ;
						if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode) {
							if(bDriverInfo) GetDriverAndSourceInfo(pHandles->hdbc) ;
							// printf("All handles allocated OK\n", retcode);
						}else{ // SQLAllocHandle()
							REPORTERROR((char*)"SQLAllocHandle()", (char*)"failed") ;
							HandleError(pHandles->hdbc, SQL_HANDLE_DBC) ;
							ODBC_FN(SQLDisconnect(pHandles->hdbc), retcode) ;
							ODBC_FN(SQLFreeHandle(SQL_HANDLE_DBC, pHandles->hdbc), retcode) ;
							ODBC_FN(SQLFreeHandle(SQL_HANDLE_ENV, pHandles->henv), retcode) ;
							retcode = SQL_ERROR ;
						}
					}else{ // SQLConnect()
						REPORTERROR((char*)"SQLConnect()", (char*)"failed" ) ;
						HandleError(pHandles->hdbc, SQL_HANDLE_DBC) ;
						ODBC_FN(SQLFreeHandle(SQL_HANDLE_DBC, pHandles->hdbc), retcode) ;
						ODBC_FN(SQLFreeHandle(SQL_HANDLE_ENV, pHandles->henv), retcode) ;
						retcode = SQL_ERROR ;
					}
				}else{ // SQLAllocHandle()
					REPORTERROR((char*)"SQLAllocHandle()", "failed" ) ;
					HandleError(pHandles->hdbc, SQL_HANDLE_DBC) ;
					ODBC_FN(SQLFreeHandle(SQL_HANDLE_ENV, pHandles->henv), retcode) ;
					retcode = SQL_ERROR ;
				}
			}else{ // SQLSetEnvAttr()
				REPORTERROR((char*)"SQLSetEnvAttr()", "failed" ) ;
				HandleError(pHandles->henv, SQL_HANDLE_ENV) ;
				ODBC_FN(SQLFreeHandle(SQL_HANDLE_ENV, pHandles->henv), retcode) ;
				retcode = SQL_ERROR ;
			}
		}else{ // SQLAllocHandle()
			REPORTERROR((char*)"SQLAllocHandle()", "failed" ) ;
			HandleError(pHandles->henv, SQL_HANDLE_ENV) ;
			retcode = SQL_ERROR ;
		}
	}else{
			ODBC_FN(SQLFreeHandle(SQL_HANDLE_STMT, pHandles->hstmt), retcode) ;
			ODBC_FN(SQLDisconnect(pHandles->hdbc), retcode) ;
			ODBC_FN(SQLFreeHandle(SQL_HANDLE_DBC, pHandles->hdbc), retcode) ;
			ODBC_FN(SQLFreeHandle(SQL_HANDLE_ENV, pHandles->henv), retcode) ;
	}

    return retcode ;
}



/*************************************************
Function: AggretateFn():
<aggr_fn fn> - name of the aggregate function to use
<char* szTableName> - name of the table
<int nCol> - number of the column
<double* pdIn> - pointer to double containing the value to be used in a call to COUNT; used only by this function
<double* pdOut> - pointer to double that will recieve the result
<attr_type attrType> - type of the attribute
*************************************************/
SQLRETURN AggregateFn(aggr_fn fn, char* szTableName, int nCol, double* pdIn, double* pdOut, attr_type attrType){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR* szStmt[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szValueBuffer[MAX_VALUE_LEN] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;
    SQLINTEGER  cbDouble = 0 ;

    GetHandles(&stHandles, GET, 0) ;

    switch(fn){
    case FN_COUNT:
        switch(attrType){
    case T_INT:
        sprintf((char*)szStmt, "SELECT COUNT(*) FROM %s WHERE COL%d > %d", szTableName, nCol, (int)*pdIn) ;
        break ;
    case T_FLOAT:
        sprintf((char*)szStmt, "SELECT COUNT(*) FROM %s WHERE COL%d > %f", szTableName, nCol, (float)*pdIn) ;
        break ;
/*    case T_DOUBLE:
        sprintf((char*)szStmt, "SELECT COUNT(*) FROM %s WHERE COL%d > %.15f", szTableName, nCol, *pdIn) ;
        break ;
*/
    default:
        break ;
            }
    break ;
    case FN_SUM:
        sprintf((char*)szStmt, "SELECT SUM(COL%d) FROM %s", nCol, szTableName) ;
        break ;
    case FN_AVG:
        sprintf((char*)szStmt, "SELECT AVG(COL%d) FROM %s", nCol, szTableName) ;
        break ;
    case FN_MAX:
        sprintf((char*)szStmt, "SELECT MAX(COL%d) FROM %s", nCol, szTableName) ;
        break ;
    case FN_MIN:
        sprintf((char*)szStmt, "SELECT MIN(COL%d) FROM %s", nCol, szTableName) ;
        break ;
    case FN_VARIANCE: // not implemented
        //sprintf((char*)szStmt, "SELECT VARIANCE(COL%d) FROM %s;", nCol, szTableName) ;
        break ;
    case FN_STDDEV: // not implemented
        //sprintf((char*)szStmt, "SELECT STDDEV(COL%d) FROM %s;", nCol, szTableName) ;
        break ;
    default:
        break ;
        }
//printf("%s\n", szStmt) ; 

retcode = SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmt, SQL_NTS) ;
if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ){
    retcode = SQLBindCol(stHandles.hstmt, 1, SQL_C_DOUBLE, (void*)pdOut, sizeof(SQLDOUBLE), &cbDouble) ;
    if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode ){
        retcode = SQLFetch(stHandles.hstmt) ;
        }
    }

if(SQL_SUCCESS != retcode && SQL_SUCCESS_WITH_INFO != retcode){
    HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
    }

SQLCloseCursor(stHandles.hstmt) ;

GetHandles(&stHandles, FREE, 0) ;

return retcode ;

    };



/*************************************************
Function: GetDriverAndSourceInfo()
*************************************************/
SQLRETURN GetDriverAndSourceInfo(SQLHDBC hdbc){

    SQLRETURN retcode = SQL_ERROR ;

    SQLCHAR			buffer[255] ;
    SQLUSMALLINT	snValue = 0 ;
    SQLSMALLINT		outlen =  0 ;

    printf( "-------------------------------------------\n" ) ;

    retcode = SQLGetInfo( hdbc, SQL_DATA_SOURCE_NAME, buffer, 255, &outlen ) ;

    printf( "Connected to Server: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_DATABASE_NAME, buffer, 255, &outlen ) ;
    printf( "      Database name: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_SERVER_NAME, buffer, 255, &outlen ) ;
    printf( "      Instance name: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_DBMS_NAME, buffer, 255, &outlen ) ;
    printf( "          DBMS name: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_DBMS_VER, buffer, 255, &outlen ) ;
    printf( "       DBMS version: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_ODBC_VER, buffer, 255, &outlen ) ;
    printf( "       ODBC version: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_DRIVER_NAME, buffer, 255, &outlen ) ;
    printf( "        Driver name: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_DRIVER_VER, buffer, 255, &outlen ) ;
    printf( "     Driver version: %s\n", buffer ) ;

    retcode = SQLGetInfo( hdbc, SQL_MAX_DRIVER_CONNECTIONS, &snValue, sizeof(SQLSMALLINT), &outlen ) ;
    printf( "    Max connections: %d\n", snValue ) ;

    retcode = SQLGetInfo( hdbc, SQL_CURSOR_COMMIT_BEHAVIOR, &snValue, sizeof(SQLSMALLINT), &outlen ) ;
    printf( "Autocommit behavior:") ;

    switch(snValue){
    case SQL_CB_DELETE:
        printf(" SQL_CB_DELETE\n") ;
        break ;
    case SQL_CB_CLOSE:
        printf(" SQL_CB_CLOSE\n") ;
        break ;
    case SQL_CB_PRESERVE:
        printf(" SQL_CB_PRESERVE\n") ;
        break ;
    default:
        printf(" undefined\n") ;
        break ;
        }

	printf( "-------------------------------------------\n" ) ;

	return retcode ;

}



/*************************************************
Function: ArithOp()
*************************************************/

int ArithOp(char* szTable, int nTotalCols, float* pValue, attr_type attrType, arth_op op){
	
    SQLRETURN retcode = SQL_ERROR ;
	int nVerRet = -1 ;
    SQLCHAR szStmt[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szEndBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;
    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;

    void* pBuffer = NULL ;
    SQLINTEGER BindInt = 0, IntResult = 0, RefIntResult = 0 ;
    SQLFLOAT BindFloat = 0, FloatResult = 0, RefFloatResult = 0 ;
    SQLDOUBLE BindDouble = 0, DoubleResult = 0, RefDoubleResult = 0 ;
    SQLINTEGER cbSize = 0 ;
    SQLINTEGER  cbLen = 0 ;
    SQLSMALLINT cbTarget = 0 ;

    GetHandles(&stHandles, GET, 0) ;

    for(int c = 0 ; c < nTotalCols ; ++c){

		sprintf((char*)szStmt, "SELECT COL%d, (COL%d", c, c) ;
        switch(op){
        case MINUS:
            strcat((char*)szStmt, " - ") ;
            break ;
        case PLUS:
            strcat((char*)szStmt, " + ") ;
            break ;
        case MULTI:
            strcat((char*)szStmt, " * ") ;
            break ;
        case DIVIDE:
            strcat((char*)szStmt, " / ") ;
            break ;
        case MODULO:
            //strcat((char*)szStmt, " % ") ; Not implemented
            GetHandles(&stHandles, FREE, 0) ;
            return -1 ; //Close handles and return
            break ;
        default:
            break ;
		}

		sprintf((char*)(szAuxBuffer),"%.9f) ", *((float*)(pValue))) ;
		strcat((char*)szStmt, (char*)szAuxBuffer) ;
		sprintf((char*)szEndBuffer, "FROM %s", szTable) ;
		strcat((char*)szStmt, (char*)szEndBuffer) ;

		ODBC_FN(SQLExecDirect(stHandles.hstmt, (SQLCHAR*)szStmt, SQL_NTS), retcode) ;
		if(retcode == SQL_ERROR){
			HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ; 
			printf("\n%s\n", szStmt) ;
		}

		SQLSMALLINT cbNameLen = 0, cbSqlType = 0, cbNullable = 0, cbColScale = 0  ;
		SQLINTEGER cbColSize = 0 ;
		SQLDescribeCol(stHandles.hstmt, 2, szColBuffer, MAX_COL_NAME-1, &cbNameLen, &cbSqlType, (unsigned long*)&cbColSize, &cbColScale, &cbNullable) ;

		switch(cbSqlType){
			case SQL_NUMERIC:
				pBuffer = &IntResult ;
				cbSize = sizeof(SQLINTEGER) ;
				cbTarget = SQL_C_ULONG ;
			case SQL_INTEGER:
				pBuffer = &IntResult ;
				cbSize = sizeof(SQLINTEGER) ;
				cbTarget = SQL_C_LONG ;
				break ;
			case SQL_FLOAT:
				pBuffer = &FloatResult ; 
				cbSize = sizeof(SQLFLOAT) ;
				cbTarget = SQL_C_FLOAT ;
				break ;
			case SQL_DOUBLE:
				pBuffer = &DoubleResult ;
				cbSize = sizeof(SQLDOUBLE) ;
				cbTarget = SQL_C_DOUBLE ;
				break ;
			default:
				printf("\nUndefined result type: %d\n", cbSqlType) ;
				break ;
		}

		switch(attrType){
			case T_INT:
				ODBC_FN(SQLBindCol(stHandles.hstmt, 1, SQL_C_SLONG, (void*)&BindInt, sizeof(SQLINTEGER), &cbLen), retcode) ;
				break ;
			case T_FLOAT:
				ODBC_FN(SQLBindCol(stHandles.hstmt, 1, SQL_C_FLOAT, (void*)&BindFloat, sizeof(SQLFLOAT), &cbLen), retcode) ;
				break ;
	/*        case T_DOUBLE:
				ODBC_FN(SQLBindCol(stHandles.hstmt, 1, SQL_C_DOUBLE, (void*)&BindDouble, sizeof(SQLDOUBLE), &cbLen), retcode) ;
				break ;
	*/
			default:
				break ;
		}

		ODBC_FN(SQLBindCol(stHandles.hstmt, 2, cbTarget, pBuffer, cbSize, &cbLen), retcode) ;

		retcode = SQLFetch(stHandles.hstmt) ;

		if (SQL_SUCCESS == retcode || SQL_SUCCESS_WITH_INFO == retcode){
			switch(attrType){
				case T_INT:
					switch(cbSqlType){
						case SQL_INTEGER:
							nVerRet = VerifyArthOp((int*)&BindInt, pValue, (int*)pBuffer, op) ;
							break ;
						case SQL_FLOAT:
							nVerRet = VerifyArthOp((int*)&BindInt, pValue, (float*)pBuffer, op) ;
							break ;
						case SQL_DOUBLE:
							nVerRet = VerifyArthOp((int*)&BindInt, pValue, (double*)pBuffer, op) ;
							break ;
						case SQL_NUMERIC:
							nVerRet = VerifyArthOp((int*)&BindInt, pValue, (int*)pBuffer, op) ;
							break ;
						default:
							break ;
					}
					break ;

				case T_FLOAT:
					switch(cbSqlType){
						case SQL_INTEGER:
							nVerRet = VerifyArthOp((float*)&BindFloat, pValue, (int*)pBuffer, op) ;
							break ;
						case SQL_FLOAT:
							nVerRet = VerifyArthOp((float*)&BindFloat, pValue, (float*)pBuffer, op) ;
							break ;
						case SQL_DOUBLE:
							nVerRet = VerifyArthOp((float*)&BindFloat, pValue, (double*)pBuffer, op) ;
							break ;
						default:
							break ;
					}
					break ;
	/*                case T_DOUBLE:
						switch(cbSqlType){
					case SQL_INTEGER:
						nVerRet = VerifyArthOp((double*)&BindDouble, pValue, (int*)pBuffer, op) ;
						break ;
					case SQL_FLOAT:
						nVerRet = VerifyArthOp((double*)&BindDouble, pValue, (float*)pBuffer, op) ;
						break ;
					case SQL_DOUBLE:
						nVerRet = VerifyArthOp((double*)&BindDouble, pValue, (double*)pBuffer, op) ;
						break ;
					default:
						break ;
							}
					break ;
	*/
				default:
					break ;
			}
			if(-1 == nVerRet){
				printf("\nVerification failed.\n") ;
				return nVerRet ;
			}else if(SQL_NO_DATA == retcode){
				break ;
			}
		}else{

			HandleError(stHandles.hstmt, SQL_HANDLE_STMT) ;
		}

		SQLCloseCursor(stHandles.hstmt) ;
		}
		
		GetHandles(&stHandles, FREE, 0) ;
		
		return nVerRet ;
}




/*************************************************
Function: Join()
*************************************************/
SQLRETURN Join(char* szTable, int nTables, int nCol, join_type joinType){

    SQLRETURN retcode = SQL_ERROR ;
    SQLCHAR szStmt[MAX_SQL_STMT] = { 0 } ;
    SQLCHAR szEndBuffer[MAX_STR_LEN] = { 0 } ;
    SQLCHAR szColBuffer[MAX_COL_NAME] = { 0 } ;
    SQLCHAR szAuxBuffer[MAX_STR_LEN] = { 0 } ;

    ODBC_HANDLES stHandles ;
    memset(&stHandles, 0, sizeof(ODBC_HANDLES)) ;

    int c = 0, t = 0 ;

    GetHandles(&stHandles, GET, 0) ;

    for(c = 0 ; c < nCol ; ++c) {

        switch(joinType){
        case ITSELF:
            sprintf((char*)szStmt, "SELECT * FROM %s, %s", (char*)szTable, (char*)szTable) ;
            break ;
        case EQUI:
            break ;
        case NON_EQUI:
            break ;
        case INNER:
            break ;
        case OUTTER:
            break ;
        default:
            break ;
            }
        }

GetHandles(&stHandles, FREE, 0) ;

return retcode ;

}



SQLRETURN GetResults(SQLHSTMT){

    SQLRETURN retcode = SQL_ERROR ;

    return retcode ;
}

/*

int createTables(char* szTableName, int nTables){
  
	for (int i = 0; i < nNoOfCol; i++){
		snprintf(attrName[i], MAXSTRLEN, "COL%d", i) ;
	}

	for (int i = 0; i < nTables; i++){
		snprintf(tableName[i], MAXSTRLEN, "TAB%d", i) ;
	}
  
	for(unsigned i = 0; i < nTables; i++){
		
		ndbout << "Creating " << szTableName[i] << "... " ;
		
		NDBT_Table tmpTable(szTableName[i]) ;
		
		tmpTable.setStoredTable(!theTempTable) ;
    
  		tmpTable.addAttribute(NDBT_Attribute(attrName[0],
                                    UnSigned,
                                    4, // 4 Bytes
                                    TupleKey));
	}

		
	for (int j = 1 ; j < nNoOfCol ; j++)
		tmpTable.addAttribute(NDBT_Attribute(attrName[j], UnSigned, 4*tAttributeSize)) ;

	if(tmpTable.createTableInDb(pMyNdb) == -1){
		return -1 ;
	}
    
	ndbout << "done" << endl ;
		
	return 0;
}
*/

/*************************************************
Function: createTables()
Uses NDB API to create tables for the tests
*************************************************/

int createTables(char* szTableName, int nTables){

  Ndb * pNdb = new Ndb("TEST_DB") ;
  pNdb->init();
  
  ndbout << "Waiting for ndb to become ready..." <<endl;
  if (pNdb->waitUntilReady(10000) != 0){
    ndbout << "NDB is not ready" << endl;
    ndbout << "Benchmark failed!" << endl;
    delete pNdb ;
	return -1 ;
  }

  NdbSchemaCon          *MySchemaTransaction = NULL ;
  NdbSchemaOp           *MySchemaOp = NULL ;
  int                   check = -1 ;
  char					szColNameBuffer[MAX_COL_NAME] = { 0 } ;
  int					tLoadFactor = 80 ;
  
  for(int i=0 ; i < nTables ; ++i) {
	  
	  ndbout << "Creating " << (char*)(szTableName+MAX_TABLE_NAME*i) << "..." << endl ;

      MySchemaTransaction = pNdb->startSchemaTransaction() ;
	  //printf("MySchemaTransaction - OK\n") ;
      if(MySchemaTransaction == NULL){
		  printf("MySchemaTransaction is NULL\n") ;
		  delete pNdb ;
		  return -1 ;
	  }
      
      MySchemaOp = MySchemaTransaction->getNdbSchemaOp();       
  	  //printf("MySchemaTransaction->getNdb... - OK\n") ;
      if(MySchemaOp == NULL){
  		  printf("MySchemaOp is NULL\n") ;
		  delete pNdb ;
		  return -1 ;
	  }
      
      check = MySchemaOp->createTable( (const char*)(szTableName+MAX_TABLE_NAME*i)
                                       ,8                       // Table Size
                                       ,TupleKey                // Key Type
                                       ,40                      // Nr of Pages
                                       ,All
                                       ,6
                                       ,(tLoadFactor - 5)
                                       ,(tLoadFactor)
                                       ,1
                                       ,0
                                       );
      
      if (check == -1){
 		  printf("MySchemaOp->createTable failed\n") ;
		  delete pNdb ;
		  return -1 ;
	  }

      snprintf(szColNameBuffer, MAX_COL_NAME, "COL%d", 0) ;
      check = MySchemaOp->createAttribute( szColNameBuffer,
                                           TupleKey,
                                           32,
                                           PKSIZE,
                                           UnSigned,
                                           MMBased,
                                           NotNullAttribute );
      
      if (check == -1){
  		  printf("MySchemaOp->createAttribute() #1 failed\n") ;
		  delete pNdb ;
		  return -1 ;
	  }

      for (int j = 1; j < nNoOfCol ; j++){
		  snprintf(szColNameBuffer, MAX_COL_NAME, "COL%d", j) ;
		  check = MySchemaOp->createAttribute(szColNameBuffer,
                                             NoKey,
                                             32,
                                             tAttributeSize,
                                             UnSigned,
                                             MMBased,
                                             NotNullAttribute );
		
		  if (check == -1){
    		  printf("MySchemaOp->createAttribute() #2 failed\n") ;
			  delete pNdb ;
			  return -1;
		  }
      }
      
      if (MySchemaTransaction->execute() == -1){
   		  printf("MySchemaTransaction->execute() failed\n") ;
		  printf("%s\n", MySchemaTransaction->getNdbError().message) ; 
		  return -1 ;
		  delete pNdb ;
	  }
      
      pNdb->closeSchemaTransaction(MySchemaTransaction);
    }
  
  return 0;
}



