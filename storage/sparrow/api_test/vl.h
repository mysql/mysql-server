#ifndef _spw_test_vl_h
#define _spw_test_vl_h

#include "common.h"

using namespace Sparrow;

//-----------------------------------------------------------------------------
class TestVL : public Test {
public:
	TestVL(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();
	void testColaescing();

private:
	Table* createTableAndSend(const char* table_name, int nbRows, int idCount, int dataCount);
	Table* createTable(const char* table_name, int idCount, int dataCount);
	void sendData(const Table* table, int nbRows, int idCount, int dataCount);
};

#endif	// _spw_test_vl_h