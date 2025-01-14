#ifndef _spw_test_many_partitions_h
#define _spw_test_many_partitions_h

#include "common.h"

using namespace Sparrow;

//-----------------------------------------------------------------------------

class TestManyPartitions : public Test {
public:
	TestManyPartitions(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();

private:
	void createManyPartitions(const Table* table, uint nbPartitions);
	void insertInterlacedTimestamps(const Table* table);
	void insertInterlacedTimestampsMassive(const Table* table);
};

#endif		// _spw_test_many_partitions_h