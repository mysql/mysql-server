#ifndef _spw_test_common_h
#define _spw_test_common_h


//#include "../api/include/spw_global.h"
//#include "../api/include/global.h"
#include "../api/include/connection.h"
#include "../api/misc.h"
#include "exception.h"
#include "utils.h"

class Test {

protected:
	SQLparams		sql_params_;
	Sparrow::Connection*		connect_;

public:
	Test(const SQLparams& sql_params);
	virtual ~Test();

	void reset(Sparrow::Table*& table);
	void dropTable(const char* table_name);
	uint getFlushInterval();

};

#endif	// _spw_test_common_h