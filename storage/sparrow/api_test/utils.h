#ifndef _spw_test_utils_h
#define _spw_test_utils_h

#ifdef _WIN32
#include "my_inttypes.h"
#else
#include "my_compiler.h"
#endif

class SQLparams {
private:
	char	host_[64];
	char	login_[32];
	char	psswd_[32];
	uint	mysql_port_;
	uint	spw_port_;
	char	schema_[64];

public:
	SQLparams(const char* host, const char* login, const char* psswd, const uint mysql_port, const uint spw_port, const char* schema) 
		: mysql_port_(mysql_port), spw_port_(spw_port)
	{
		strncpy(host_, host, sizeof(host_) - 1);
		host_[sizeof(host_) - 1] = '\0';
		strncpy(login_, login, sizeof(login_) - 1);
		login_[sizeof(login_) - 1] = '\0';
		strncpy(psswd_, psswd, sizeof(psswd_) - 1);
		psswd_[sizeof(psswd_) - 1] = '\0';
		strncpy(schema_, schema, sizeof(schema_) - 1);
		schema_[sizeof(schema_) - 1] = '\0';
	}

	const char* getHost() const {
		return host_;
	}

	const char* getLogin() const {
		return login_;
	}

	const char* getPsswd() const {
		return psswd_;
	}

	uint getMySQLPort() const {
		return mysql_port_;
	}

	uint getSpwPort() const {
		return spw_port_;
	}

	const char* getSchema() const  {
		return schema_;
	}
};

#endif		// _spw_test_utils_h