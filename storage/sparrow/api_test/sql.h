#ifndef _test_sql_h_
#define _test_sql_h_

//#include "my_global.h"
#include "mysql.h"
#include "common.h"
#include "exception.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MySQLGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

class MySQLGuard {
private:

	MYSQL* mysql_;
	MYSQL_RES* result_;

public:

	MySQLGuard(const char* username, const char* password, const uint port) _THROW_(MyException);

	void execute(const char* stmt) _THROW_(MyException);

	MYSQL_RES* get() {
		return result_;
	}

	~MySQLGuard();

	void clear() {
		if (result_ != 0) {
			mysql_free_result(result_);
			result_ = 0;
		}
	}

	static void check(MYSQL* mysql, const char* stmt) _THROW_(MyException);
};

#endif	// _test_sql_h_
