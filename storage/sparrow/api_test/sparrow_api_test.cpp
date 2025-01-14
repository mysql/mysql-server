#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "../api/include/connection.h"
#include "../api/misc.h"
#include "exception.h"

#include "all_types.h"
#include "column_optim.h"
#include "column_subset.h"
#include "errors.h"
#include "many_partitions.h"
#include "too_many_columns.h"
#include "vl.h"

//#define TEST_BASIC
//#define TEST_CREATETABLE_TOO_MANY_COLS
//#define TEST_COALESCING
//#define TEST_CREATETABLE
//#define TEST_VL
//#define TEST_MASTERFILE
//#define TEST_INSERT
//#define TEST_INSERT_2
//#define TEST_MANY_PARTITIONS
//#define TEST_INSERT_INTERLACED_TS
//#define TEST_INSERT_INTERLACED_TS_MASS
//#define TEST_INSERT_SELECT_COLUMNS
//#define TEST_INSERT_SELECT_COLUMNS_MASSIVE
#define TEST_COLUMN_OPTIM
//#define TEST_ERRORS
//#define TEST_DISABLE_COALESCING

// Connection properties
#define EXAMPLE_DB   "test_1"
#define EXAMPLE_URL "tcp://127.0.0.1:38000"
#define EXAMPLE_HOST "127.0.0.1"
#define EXAMPLE_MYSQL_PORT	38004
#define EXAMPLE_SPARROW_PORT	38005
#define EXAMPLE_USER "root"
#define EXAMPLE_PASS "infovista"

const char*		schema = "test_spw";


//////////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[])
{
	const char* host	= (argc >= 2 ? argv[1] : EXAMPLE_HOST);
	const char* user	= (argc >= 3 ? argv[2] : EXAMPLE_USER);
	const char* pass	= (argc >= 4 ? argv[3] : EXAMPLE_PASS);
	//const char* database	= (argc >= 5 ? argv[4] : EXAMPLE_DB);
	uint32_t	mysqlPort = EXAMPLE_MYSQL_PORT;
	uint32_t	sparrowPort = EXAMPLE_SPARROW_PORT;
	if ( argc >= 5 ) {
		mysqlPort = atoi(argv[4]);
	}
	if ( argc >= 6 ) {
		sparrowPort = atoi(argv[5]);
	}

	SQLparams	sql_params(host, user, pass, mysqlPort, sparrowPort, schema);

#ifdef TEST_BASIC
	TestAlltypes	test_all(sql_params);
	test_all.run();
#endif

#ifdef TEST_DISABLE_COALESCING
	TestAlltypes	test_all(sql_params);
	test_all.runDisableCoalescingSchemaTest("dummy_table", true);
#endif

#ifdef TEST_ERRORS
	TestErrors	test_errors(sql_params);
	test_errors.run();
#endif

#ifdef TEST_COLUMN_OPTIM
	TestColumnOptim	test_col_optim(sql_params);
	//test_col_optim.runSimple();
	test_col_optim.runtAlterTests();		// Not implemented yet in Sparrow. To test later
	//test_col_optim.runMultiPartTests();
	//test_col_optim.runCoalescingTests();
	//test_col_optim.runDNSTests();
	//test_col_optim.runVolumeTests();
#endif

#ifdef TEST_CREATETABLE_TOO_MANY_COLS
#endif


#ifdef TEST_INSERT_SELECT_COLUMNS
	TestColumnSubset	test_ins_sel_cols(sql_params);
	test_ins_sel_cols.run();
#endif

#ifdef TEST_VL
	TestVL		testVL(sql_params);
	testVL.testColaescing();
#endif


#ifdef TEST_MANY_PARTITIONS
#endif

	getchar();

	return 0;
}


