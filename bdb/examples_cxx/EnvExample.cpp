/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: EnvExample.cpp,v 11.12 2000/10/27 20:32:00 dda Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <iostream.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db_cxx.h>

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

void	db_setup(char *, char *, ostream&);
void	db_teardown(char *, char *, ostream&);

char *progname = "EnvExample";			/* Program name. */

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
		char *data_dir, *home;

		//
		// All of the shared database files live in /home/database,
		// but data files live in /database.
		//
		home = DATABASE_HOME;
		data_dir = CONFIG_DATA_DIR;

		cout << "Setup env\n";
		db_setup(DATABASE_HOME, data_dir, cerr);

		cout << "Teardown env\n";
		db_teardown(DATABASE_HOME, data_dir, cerr);
		return 0;
	}
	catch (DbException &dbe) {
		cerr << "AccessExample: " << dbe.what() << "\n";
		return 1;
	}
}

// Note that any of the db calls can throw DbException
void
db_setup(char *home, char *data_dir, ostream& err_stream)
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
	dbenv->open(DATABASE_HOME,
    DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN, 0);

	// Do something interesting...

	// Close the handle.
	dbenv->close(0);
}

void
db_teardown(char *home, char *data_dir, ostream& err_stream)
{
	// Remove the shared database regions.
	DbEnv *dbenv = new DbEnv(0);

	dbenv->set_error_stream(&err_stream);
	dbenv->set_errpfx(progname);

	(void)dbenv->set_data_dir(data_dir);
	dbenv->remove(home, 0);
	delete dbenv;
}
