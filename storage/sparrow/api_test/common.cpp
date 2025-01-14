#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <vector>
#include <iostream>
#include <sstream>

#include "common.h"
#include "sql.h"

using namespace Sparrow;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//	Test

Test::Test(const SQLparams& sql_params) : sql_params_(sql_params) {
	try
	{
		initialize();
		if ( !(connect_=createConnect()) ) 
			throw MyException::create( false, "Failed to get Connection object." );
		if ( connect_->setProperties( sql_params_.getHost(), sql_params_.getLogin(), sql_params_.getPsswd(), sql_params_.getMySQLPort(), sql_params_.getSpwPort() ) < 0 )
			throw MyException::create( false, "Failed to set Properties." );
		if ( connect_->connect() < 0 )
			throw MyException::create( false, "Failed to set Connect to Sparrow." );
	} catch ( const MyException& e ) {
		printf( "Failed to connect to Sparrow: %s : %s\n", e.getText(), errmsg() );
	}
}

Test::~Test() {
	if ( connect_ ) {
		if ( !connect_->isClosed() ) {
			printf( "Disconnecting..." );
			connect_->disconnect();
			printf( "done\n" );
		}
		printf( "Deleting connection object..." );
		delete connect_;
		connect_= NULL;
		printf( "done\n" );
	}

}

void Test::dropTable(const char* table_name) {
	printf( "Dropping Sparrow table '%s'.'%s' ...", sql_params_.getSchema(), table_name);
	MySQLGuard	mysql(sql_params_.getLogin(), sql_params_.getPsswd(), sql_params_.getMySQLPort());
	char	sql[1024];
	sprintf(sql, "drop table if exists %s.%s", sql_params_.getSchema(), table_name);
	mysql.execute(sql);
	printf( "done\n" );
}

void Test::reset(Table*& table) {
	dropTable(table->getTableName());
	printf( "Deleting Sparrow table..." );
	delete table;
	table = NULL;
	printf( "done\n" );
}

uint Test::getFlushInterval() {
	MySQLGuard	mysql(sql_params_.getLogin(), sql_params_.getPsswd(), sql_params_.getMySQLPort());
	mysql.execute("show variables like 'sparrow_flush_interval'");
	MYSQL_RES* result = mysql.get();
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row == 0) {
		printf("Failed to get 'sparrow_flush_interval'\n.");
		return 0;
	}
	const char*	str = row[1];
	uint		value = static_cast<uint32_t>(atoi(str));
	return value;
}