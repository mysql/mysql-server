#ifndef _spw_test_errors_h
#define _spw_test_errors_h

#include "common.h"

using namespace Sparrow;


//-----------------------------------------------------------------------------

class TestErrors : public Test {
public:
	TestErrors(const SQLparams& sql_params) : Test(sql_params) {;}

	void run();

private:
	void runInsertionTests(const Table*);
};

#endif	// _spw_test_errors_h