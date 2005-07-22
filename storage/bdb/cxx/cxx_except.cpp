/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: cxx_except.cpp,v 11.28 2004/09/22 03:34:48 bostic Exp $
 */

#include "db_config.h"

#include <string.h>
#include <errno.h>

#include "db_cxx.h"
#include "dbinc/cxx_int.h"

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

DbException::~DbException() throw()
{
	delete [] what_;
}

DbException::DbException(int err)
:	err_(err)
,	env_(0)
{
	describe(0, 0);
}

DbException::DbException(const char *description)
:	err_(0)
,	env_(0)
{
	describe(0, description);
}

DbException::DbException(const char *description, int err)
:	err_(err)
,	env_(0)
{
	describe(0, description);
}

DbException::DbException(const char *prefix, const char *description, int err)
:	err_(err)
,	env_(0)
{
	describe(prefix, description);
}

DbException::DbException(const DbException &that)
:	__DB_STD(exception)()
,	what_(dupString(that.what_))
,	err_(that.err_)
,	env_(0)
{
}

DbException &DbException::operator = (const DbException &that)
{
	if (this != &that) {
		err_ = that.err_;
		delete [] what_;
		what_ = dupString(that.what_);
	}
	return (*this);
}

void DbException::describe(const char *prefix, const char *description)
{
	char msgbuf[1024], *p, *end;

	p = msgbuf;
	end = msgbuf + sizeof(msgbuf) - 1;

	if (prefix != NULL) {
		strncpy(p, prefix, (p < end) ? end - p: 0);
		p += strlen(prefix);
		strncpy(p, ": ", (p < end) ? end - p: 0);
		p += 2;
	}
	if (description != NULL) {
		strncpy(p, description, (p < end) ? end - p: 0);
		p += strlen(description);
		if (err_ != 0) {
			strncpy(p, ": ", (p < end) ? end - p: 0);
			p += 2;
		}
	}
	if (err_ != 0) {
		strncpy(p, db_strerror(err_), (p < end) ? end - p: 0);
		p += strlen(db_strerror(err_));
	}

	/*
	 * If the result was too long, the buffer will not be null-terminated,
	 * so we need to fix that here before duplicating it.
	 */
	if (p >= end)
		*end = '\0';

	what_ = dupString(msgbuf);
}

int DbException::get_errno() const
{
	return (err_);
}

const char *DbException::what() const throw()
{
	return (what_);
}

DbEnv *DbException::get_env() const
{
	return env_;
}

void DbException::set_env(DbEnv *env)
{
	env_= env;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMemoryException                       //
//                                                                    //
////////////////////////////////////////////////////////////////////////

static const char *memory_err_desc = "Dbt not large enough for available data";
DbMemoryException::~DbMemoryException() throw()
{
}

DbMemoryException::DbMemoryException(Dbt *dbt)
:	DbException(memory_err_desc, ENOMEM)
,	dbt_(dbt)
{
}

DbMemoryException::DbMemoryException(const char *prefix, Dbt *dbt)
:	DbException(prefix, memory_err_desc, ENOMEM)
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

DbDeadlockException::~DbDeadlockException() throw()
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

DbLockNotGrantedException::~DbLockNotGrantedException() throw()
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
,	lock_(new DbLock(lock))
,	index_(index)
{
}

DbLockNotGrantedException::DbLockNotGrantedException(const char *description)
:	DbException(description, DB_LOCK_NOTGRANTED)
,	op_(DB_LOCK_GET)
,	mode_(DB_LOCK_NG)
,	obj_(NULL)
,	lock_(NULL)
,	index_(0)
{
}

DbLockNotGrantedException::DbLockNotGrantedException
    (const DbLockNotGrantedException &that)
:	DbException(that)
{
	op_ = that.op_;
	mode_ = that.mode_;
	obj_ = that.obj_;
	lock_ = (that.lock_ != NULL) ? new DbLock(*that.lock_) : NULL;
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
		lock_ = (that.lock_ != NULL) ? new DbLock(*that.lock_) : NULL;
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

DbRunRecoveryException::~DbRunRecoveryException() throw()
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
