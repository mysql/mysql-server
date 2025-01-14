#ifndef _spw_test_column_optim_h
#define _spw_test_column_optim_h

#include "common.h"

using namespace Sparrow;

//-----------------------------------------------------------------------------

class TestColumnOptim : public Test {
private:
	uint	flushInterval_;

public:
	TestColumnOptim(const SQLparams& sql_params);
	//~TestColumnOptim();

	void runSimple();
	void runMultiPartTests();
	void runtAlterTests();
	void runCoalescingTests(const uint nbRows=10);
	void runDNSTests();
	void runVolumeTests();

private:
	Table* createTable(const char* table_name, int droppedCol=-1, int insertCol=-1, uint64_t coalescingPeriod=3600*1000);
	Table* createTableDNS(const char* table_name, uint64_t coalescingPeriod=3600*1000);
	void sendData(const Table* table, uint nbRows, int null_col, int nb_col);
	void sendDataDNS(const Table* table, uint nbRows);

	void runtAlterTests(const int null_col);
	void waitForFlush();
};

#endif	// _spw_test_column_optim_h