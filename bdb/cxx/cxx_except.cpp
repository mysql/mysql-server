/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: cxx_except.cpp,v 11.17 2002/08/23 01:07:27 mjc Exp $";
#endif /* not lint */

#include <string.h>
#include <errno.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

// tmpString is used to create strings on the stack
//
class tmpString
{
public:
	tmpString(const char *str1,
		  const char *str2 = 0,
		  const char *str3 = 0,
		  const char *str4 = 0,
		  const char *str5 = 0);
	~tmpString()                      { delete [] s_; }
	operator const char *()           { return (s_); }

private:
	char *s_;
};

tmpString::tmpString(const char *str1,
		     const char *str2,
		     const char *str3,
		     const char *str4,
		     const char *str5)
{
	size_t len = strlen(str1);
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
	what_ = dupString(tmpString(prefix1, ": ", prefix2, ": ",
	    db_strerror(err)));
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

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMemoryException                       //
//                                                                    //
////////////////////////////////////////////////////////////////////////

static const char *memory_err_desc = "Dbt not large enough for available data";
DbMemoryException::~DbMemoryException()
{
}

DbMemoryException::DbMemoryException(Dbt *dbt)
:	DbException(memory_err_desc, ENOMEM)
,	dbt_(dbt)
{
}

DbMemoryException::DbMemoryException(const char *description)
:	DbException(description, ENOMEM)
,	dbt_(0)
{
}

DbMemoryException::DbMemoryException(const char *prefix, Dbt *dbt)
:	DbException(prefix, memory_err_desc, ENOMEM)
,	dbt_(dbt)
{
}

DbMemoryException::DbMemoryException(const char *prefix1, const char *prefix2,
    Dbt *dbt)
:	DbException(prefix1, prefix2, ENOMEM)
,	dbt_(dbt)
{
}

DbMemoryException::DbMemoryException(const DbMemoryException &that)
:	DbException(that)
,	dbt_(that.dbt_)
{
}

DbMemoryException
&DbMemoryException::operator =(const DbMemoryException &that)
{
	if (this != &that) {
		DbException::operator=(that);
		dbt_ = that.dbt_;
	}
	return (*this);
}

Dbt *DbMemoryException::get_dbt() const
{
	return (dbt_);
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbDeadlockException                     //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbDeadlockException::~DbDeadlockException()
{
}

DbDeadlockException::DbDeadlockException(const char *description)
:	DbException(description, DB_LOCK_DEADLOCK)
{
}

DbDeadlockException::DbDeadlockException(const DbDeadlockException &that)
:	DbException(that)
{
}

DbDeadlockException
&DbDeadlockException::operator =(const DbDeadlockException &that)
{
	if (this != &that)
		DbException::operator=(that);
	return (*this);
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbLockNotGrantedException               //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbLockNotGrantedException::~DbLockNotGrantedException()
{
	delete lock_;
}

DbLockNotGrantedException::DbLockNotGrantedException(const char *prefix,
    db_lockop_t op, db_lockmode_t mode, const Dbt *obj, const DbLock lock,
    int index)
:	DbException(prefix, DbEnv::strerror(DB_LOCK_NOTGRANTED),
		    DB_LOCK_NOTGRANTED)
,	op_(op)
,	mode_(mode)
,	obj_(obj)
,	index_(index)
{
	lock_ = new DbLock(lock);
}

DbLockNotGrantedException::DbLockNotGrantedException
    (const DbLockNotGrantedException &that)
:	DbException(that)
{
	op_ = that.op_;
	mode_ = that.mode_;
	obj_ = that.obj_;
	lock_ = new DbLock(*that.lock_);
	index_ = that.index_;
}

DbLockNotGrantedException
&DbLockNotGrantedException::operator =(const DbLockNotGrantedException &that)
{
	if (this != &that) {
		DbException::operator=(that);
		op_ = that.op_;
		mode_ = that.mode_;
		obj_ = that.obj_;
		lock_ = new DbLock(*that.lock_);
		index_ = that.index_;
	}
	return (*this);
}

db_lockop_t DbLockNotGrantedException::get_op() const
{
	return op_;
}

db_lockmode_t DbLockNotGrantedException::get_mode() const
{
	return mode_;
}

const Dbt* DbLockNotGrantedException::get_obj() const
{
	return obj_;
}

DbLock* DbLockNotGrantedException::get_lock() const
{
	return lock_;
}

int DbLockNotGrantedException::get_index() const
{
	return index_;
}



////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbRunRecoveryException                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbRunRecoveryException::~DbRunRecoveryException()
{
}

DbRunRecoveryException::DbRunRecoveryException(const char *description)
:	DbException(description, DB_RUNRECOVERY)
{
}

DbRunRecoveryException::DbRunRecoveryException
    (const DbRunRecoveryException &that)
:	DbException(that)
{
}

DbRunRecoveryException
&DbRunRecoveryException::operator =(const DbRunRecoveryException &that)
{
	if (this != &that)
		DbException::operator=(that);
	return (*this);
}
