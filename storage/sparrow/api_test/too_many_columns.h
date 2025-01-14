#ifndef _spw_test_too_many_columns_h
#define _spw_test_too_many_columns_h

#include "common.h"

using namespace Sparrow;

//-----------------------------------------------------------------------------
// Create TEST Table with too many columns

class TestTooManyColumns : public Test {
public:
	TestTooManyColumns(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();

private:
	Table* createTableTooLong(const char* table_name, uint& nbCols);
	Table* createTableManyCol(const char* table_name, uint nbCol, bool nullable);
	void sendDataFlow(const Table* table, uint nbCols);
};

#endif	// _spw_test_too_many_columns_h