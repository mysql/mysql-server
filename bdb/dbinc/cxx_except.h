/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: cxx_except.h,v 11.5 2002/08/01 23:32:34 mjc Exp $
 */

#ifndef _CXX_EXCEPT_H_
#define	_CXX_EXCEPT_H_

#include "cxx_common.h"

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Forward declarations
//

class DbDeadlockException;                       // forward
class DbException;                               // forward
class DbLockNotGrantedException;                 // forward
class DbLock;                                    // forward
class DbMemoryException;                         // forward
class DbRunRecoveryException;                    // forward
class Dbt;                                       // forward

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Exception classes
//

// Almost any error in the DB library throws a DbException.
// Every exception should be considered an abnormality
// (e.g. bug, misuse of DB, file system error).
//
// NOTE: We would like to inherit from class exception and
//       let it handle what(), but there are
//       MSVC++ problems when <exception> is included.
//
class _exported DbException
{
public:
	virtual ~DbException();
	DbException(int err);
	DbException(const char *description);
	DbException(const char *prefix, int err);
	DbException(const char *prefix1, const char *prefix2, int err);
	int get_errno() const;
	virtual const char *what() const;

	DbException(const DbException &);
	DbException &operator = (const DbException &);

private:
	char *what_;
	int err_;                   // errno
};

//
// A specific sort of exception that occurs when
// an operation is aborted to resolve a deadlock.
//
class _exported DbDeadlockException : public DbException
{
public:
	virtual ~DbDeadlockException();
	DbDeadlockException(const char *description);

	DbDeadlockException(const DbDeadlockException &);
	DbDeadlockException &operator = (const DbDeadlockException &);
};

//
// A specific sort of exception that occurs when
// a lock is not granted, e.g. by lock_get or lock_vec.
// Note that the Dbt is only live as long as the Dbt used
// in the offending call.
//
class _exported DbLockNotGrantedException : public DbException
{
public:
	virtual ~DbLockNotGrantedException();
	DbLockNotGrantedException(const char *prefix, db_lockop_t op,
	    db_lockmode_t mode, const Dbt *obj, const DbLock lock, int index);
	DbLockNotGrantedException(const DbLockNotGrantedException &);
	DbLockNotGrantedException &operator =
	    (const DbLockNotGrantedException &);

	db_lockop_t get_op() const;
	db_lockmode_t get_mode() const;
	const Dbt* get_obj() const;
	DbLock *get_lock() const;
	int get_index() const;

private:
	db_lockop_t op_;
	db_lockmode_t mode_;
	const Dbt *obj_;
	DbLock *lock_;
	int index_;
};

//
// A specific sort of exception that occurs when
// user declared memory is insufficient in a Dbt.
//
class _exported DbMemoryException : public DbException
{
public:
	virtual ~DbMemoryException();
	DbMemoryException(Dbt *dbt);
	DbMemoryException(const char *description);
	DbMemoryException(const char *prefix, Dbt *dbt);
	DbMemoryException(const char *prefix1, const char *prefix2, Dbt *dbt);
	Dbt *get_dbt() const;

	DbMemoryException(const DbMemoryException &);
	DbMemoryException &operator = (const DbMemoryException &);

private:
	Dbt *dbt_;
};

//
// A specific sort of exception that occurs when
// recovery is required before continuing DB activity.
//
class _exported DbRunRecoveryException : public DbException
{
public:
	virtual ~DbRunRecoveryException();
	DbRunRecoveryException(const char *description);

	DbRunRecoveryException(const DbRunRecoveryException &);
	DbRunRecoveryException &operator = (const DbRunRecoveryException &);
};

#endif /* !_CXX_EXCEPT_H_ */
