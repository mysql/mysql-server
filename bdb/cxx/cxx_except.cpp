/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_except.cpp,v 11.7 2000/09/21 15:05:45 dda Exp $";
#endif /* not lint */

#include <string.h>

#include "db_cxx.h"
#include "cxx_int.h"

// tmpString is used to create strings on the stack
//
class tmpString
{
public:
	tmpString(const char *str1,
		  const char *str2 = 0,
		  const char *str3 = 0,
		  const char *str4 = 0,
		  const char *str5 = 0)
	{
		int len = strlen(str1);
		if (str2)
			len += strlen(str2);
		if (str3)
			len += strlen(str3);
		if (str4)
			len += strlen(str4);
		if (str5)
			len += strlen(str5);

		s_ = new char[len+1];

		strcpy(s_, str1);
		if (str2)
			strcat(s_, str2);
		if (str3)
			strcat(s_, str3);
		if (str4)
			strcat(s_, str4);
		if (str5)
			strcat(s_, str5);
	}
	~tmpString()                      { delete [] s_; }
	operator const char *()           { return (s_); }

private:
	char *s_;
};

// Note: would not be needed if we can inherit from exception
// It does not appear to be possible to inherit from exception
// with the current Microsoft library (VC5.0).
//
static char *dupString(const char *s)
{
	char *r = new char[strlen(s)+1];
	strcpy(r, s);
	return (r);
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbException                             //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbException::~DbException()
{
	if (what_)
		delete [] what_;
}

DbException::DbException(int err)
:	err_(err)
{
	what_ = dupString(db_strerror(err));
}

DbException::DbException(const char *description)
:	err_(0)
{
	what_ = dupString(tmpString(description));
}

DbException::DbException(const char *prefix, int err)
:	err_(err)
{
	what_ = dupString(tmpString(prefix, ": ", db_strerror(err)));
}

DbException::DbException(const char *prefix1, const char *prefix2, int err)
:	err_(err)
{
	what_ = dupString(tmpString(prefix1, ": ", prefix2, ": ", db_strerror(err)));
}

DbException::DbException(const DbException &that)
:	err_(that.err_)
{
	what_ = dupString(that.what_);
}

DbException &DbException::operator = (const DbException &that)
{
	if (this != &that) {
		err_ = that.err_;
		if (what_)
			delete [] what_;
		what_ = 0;           // in case new throws exception
		what_ = dupString(that.what_);
	}
	return (*this);
}

int DbException::get_errno() const
{
	return (err_);
}

const char *DbException::what() const
{
	return (what_);
}
