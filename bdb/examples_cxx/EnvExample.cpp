/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: EnvExample.cpp,v 11.24 2002/01/11 15:52:15 bostic Exp $
 */

#include <sys/types.h>

#include <errno.h>
#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <db_cxx.h>

using std::ostream;
using std::cout;
using std::cerr;

#ifdef macintosh
#define	DATABASE_HOME	":database"
#define	CONFIG_DATA_DIR	":database"
#else
#ifdef DB_WIN32
#define	DATABASE_HOME	"\\tmp\\database"
#define	CONFIG_DATA_DIR	"\\database\\files"
#else
#define	DATABASE_HOME	"/tmp/database"
#define	CONFIG_DATA_DIR	"/database/files"
#endif
#endif

void	db_setup(const char *, const char *, ostream&);
void	db_teardown(const char *, const char *, ostream&);

const char *progname = "EnvExample";			/* Program name. */

//
// An example of a program creating/configuring a Berkeley DB environment.
//
int
main(int, char **)
{
	//
	// Note: it may be easiest to put all Berkeley DB operations in a
	// try block, as seen here.  Alternatively, you can change the
	// ErrorModel in the DbEnv so that exceptions are never thrown
	// and check error returns from all methods.
	//
	try {
		const char *data_dir, *home;

		//
		// All of the shared database files live in /home/database,
		// but data files live in /database.
		//
		home = DATABASE_HOME;
		data_dir = CONFIG_DATA_DIR;

		cout << "Setup env\n";
		db_setup(home, data_dir, cerr);

		cout << "Teardown env\n";
		db_teardown(home, data_dir, cerr);
		return (EXIT_SUCCESS);
	}
	catch (DbException &dbe) {
		cerr << "EnvExample: " << dbe.what() << "\n";
		return (EXIT_FAILURE);
	}
}

// Note that any of the db calls can throw DbException
void
db_setup(const char *home, const char *data_dir, ostream& err_stream)
{
	//
	// Create an environment object and initialize it for error
	// reporting.
	//
	DbEnv *dbenv = new DbEnv(0);
	dbenv->set_error_stream(&err_stream);
	dbenv->set_errpfx(progname);

	//
	// We want to specify the shared memory buffer pool cachesize,
	// but everything else is the default.
	//
	dbenv->set_cachesize(0, 64 * 1024, 0);

	// Databases are in a subdirectory.
	(void)dbenv->set_data_dir(data_dir);

	// Open the environment with full transactional support.
	dbenv->open(home,
    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN, 0);

	// Do something interesting...

	// Close the handle.
	dbenv->close(0);
}

void
db_teardown(const char *home, const char *data_dir, ostream& err_stream)
{
	// Remove the shared database regions.
	DbEnv *dbenv = new DbEnv(0);

	dbenv->set_error_stream(&err_stream);
	dbenv->set_errpfx(progname);

	(void)dbenv->set_data_dir(data_dir);
	dbenv->remove(home, 0);
	delete dbenv;
}
