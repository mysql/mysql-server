/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: MpoolExample.cpp,v 11.9 2000/10/27 20:32:01 dda Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include <db_cxx.h>

#define	MPOOL	"mpool"

void init(char *, int, int);
void run(DB_ENV *, int, int, int);

static void usage();

char *progname = "MpoolExample";			// Program name.

class MpoolExample : public DbEnv
{
public:
	MpoolExample();
	void initdb(const char *home, int cachesize);
	void run(int hits, int pagesize, int npages);

private:
	static const char FileName[];

	// no need for copy and assignment
	MpoolExample(const MpoolExample &);
	void operator = (const MpoolExample &);
};

int main(int argc, char *argv[])
{
	int cachesize = 20 * 1024;
	int hits = 1000;
	int npages = 50;
	int pagesize = 1024;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-c") == 0) {
			if ((cachesize = atoi(argv[++i])) < 20 * 1024)
				usage();
		}
		else if (strcmp(argv[i], "-h") == 0) {
			if ((hits = atoi(argv[++i])) <= 0)
				usage();
		}
		else if (strcmp(argv[i], "-n") == 0) {
			if ((npages = atoi(argv[++i])) <= 0)
				usage();
		}
		else if (strcmp(argv[i], "-p") == 0) {
			if ((pagesize = atoi(argv[++i])) <= 0)
				usage();
		}
		else {
			usage();
		}
	}

	// Initialize the file.
	init(MPOOL, pagesize, npages);

	try {
		MpoolExample app;

		cout << progname
		     << ": cachesize: " << cachesize
		     << "; pagesize: " << pagesize
		     << "; N pages: " << npages << "\n";

		app.initdb(NULL, cachesize);
		app.run(hits, pagesize, npages);
		cout << "MpoolExample: completed\n";
		return 0;
	}
	catch (DbException &dbe) {
		cerr << "MpoolExample: " << dbe.what() << "\n";
		return 1;
	}
}

//
// init --
//	Create a backing file.
//
void
init(char *file, int pagesize, int npages)
{
	//
	// Create a file with the right number of pages, and store a page
	// number on each page.
	//
	int fd;
	int flags = O_CREAT | O_RDWR | O_TRUNC;
#ifdef DB_WIN32
	flags |= O_BINARY;
#endif
	if ((fd = open(file, flags, 0666)) < 0) {
		cerr << "MpoolExample: " << file << ": " << strerror(errno) << "\n";
		exit(1);
	}
	char *p = new char[pagesize];
	memset(p, 0, pagesize);

	// The pages are numbered from 0.
	for (int cnt = 0; cnt <= npages; ++cnt) {
		*(db_pgno_t *)p = cnt;
		if (write(fd, p, pagesize) != pagesize) {
			cerr << "MpoolExample: " << file
			     << ": " << strerror(errno) << "\n";
			exit(1);
		}
	}
	delete [] p;
}

static void
usage()
{
	cerr << "usage: MpoolExample [-c cachesize] "
	     << "[-h hits] [-n npages] [-p pagesize]\n";
	exit(1);
}

// Note: by using DB_CXX_NO_EXCEPTIONS, we get explicit error returns
// from various methods rather than exceptions so we can report more
// information with each error.
//
MpoolExample::MpoolExample()
:	DbEnv(DB_CXX_NO_EXCEPTIONS)
{
}

void MpoolExample::initdb(const char *home, int cachesize)
{
	set_error_stream(&cerr);
	set_errpfx("MpoolExample");
	set_cachesize(0, cachesize, 0);

	open(home, DB_CREATE | DB_INIT_MPOOL, 0);
}

//
// run --
//	Get a set of pages.
//
void
MpoolExample::run(int hits, int pagesize, int npages)
{
	db_pgno_t pageno;
	int cnt;
	void *p;

	// Open the file in the pool.
	DbMpoolFile *dbmfp;

	DbMpoolFile::open(this, MPOOL, 0, 0, pagesize, NULL, &dbmfp);

	cout << "retrieve " << hits << " random pages... ";

	srand((unsigned int)time(NULL));
	for (cnt = 0; cnt < hits; ++cnt) {
		pageno = (rand() % npages) + 1;
		if ((errno = dbmfp->get(&pageno, 0, &p)) != 0) {
			cerr << "MpoolExample: unable to retrieve page "
			     << (unsigned long)pageno << ": "
			     << strerror(errno) << "\n";
			exit(1);
		}
		if (*(db_pgno_t *)p != pageno) {
			cerr << "MpoolExample: wrong page retrieved ("
			     << (unsigned long)pageno << " != "
			     << *(int *)p << ")\n";
			exit(1);
		}
		if ((errno = dbmfp->put(p, 0)) != 0) {
			cerr << "MpoolExample: unable to return page "
			     << (unsigned long)pageno << ": "
			     << strerror(errno) << "\n";
			exit(1);
		}
	}

	cout << "successful.\n";

	// Close the pool.
	if ((errno = close(0)) != 0) {
		cerr << "MpoolExample: " << strerror(errno) << "\n";
		exit(1);
	}
}
