#define MYSQL_SERVER 1
#include "my_sys.h"
#include "my_dbug.h"
//#include <sql_priv.h>				// For mysqld options.
#include "sql/query_options.h"		// For mysqld options.
#include "errmsg.h"

#include "sql.h"		// For configuration parameters.
#include "m_string.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// MySQLGuard
//////////////////////////////////////////////////////////////////////////////////////////////////////

MySQLGuard::MySQLGuard(const char* username, const char* password, const uint port) _THROW_(MyException)
	: result_(0) {
	mysql_ = mysql_init(0);
	uint protocol = MYSQL_PROTOCOL_TCP;
	mysql_options(mysql_, MYSQL_OPT_PROTOCOL, &protocol);

	// Use big timeouts because some operations may be long (e.g. drop Sparrow table
	// may require deleting a lot of files.)
	uint big = 86400;
	mysql_options(mysql_, MYSQL_OPT_READ_TIMEOUT, &big);
	mysql_options(mysql_, MYSQL_OPT_WRITE_TIMEOUT, &big);
	if (mysql_real_connect(mysql_, 0, username, password, 0, port, 0, 0) == 0) {
		MySQLGuard::check(mysql_, 0);
	}
}

void MySQLGuard::execute(const char* stmt) _THROW_(MyException) {
	DBUG_PRINT("sparrow_api", ("Statement: %s", stmt));
	if (mysql_real_query(mysql_, stmt, (uint)strlen(stmt)) != 0) {
		MySQLGuard::check(mysql_, stmt);
	}
	clear();
	result_ = mysql_store_result(mysql_);
}

MySQLGuard::~MySQLGuard() {
	clear();
	mysql_close(mysql_);
}

// STATIC
void MySQLGuard::check(MYSQL* mysql, const char* stmt) _THROW_(MyException) {
	const char* sqlError = mysql_error(mysql);
	unsigned int err_code = mysql_errno(mysql);
	const char* msg = "unknown error";
	if (sqlError == 0 || strlen(sqlError) == 0) {
		uint e = mysql->net.last_errno;
		if (e != 0) {
			//msg = ER(e);
		}
	} else {
		msg = sqlError;
	}
	
	if (stmt == 0) {
		MyException	e = MyException::create(false, "Cannot connect: %u, %s", err_code, msg);
		e.set_err_code( err_code );
		throw e;
	} else {
		// Truncate statement to 255 chars if necessary.
		char tstmt[256];
		strncpy(tstmt, stmt, sizeof(tstmt));
		tstmt[sizeof(tstmt) - 1] = 0;
		MyException	e = MyException::create(false, "Cannot execute \"%s\": %u, %s", tstmt, err_code, msg);
		e.set_err_code( err_code );
		throw e;
	}
}
