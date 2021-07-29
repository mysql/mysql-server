/* 
 *                       Copyright (C) 2009
 *                    Luca Deri <deri@ntop.org>
 *             Valeria Lorenzetti <lorenzetti@ntop.org>
 *
 *                     http://www.ntop.org/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */


#include "ibis.h"	// FastBit ibis header file

/* Operating System header files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // getopt

#include <memory>

#if defined(HAVE_DIRENT_H) || defined(unix) || defined(__HOS_AIX__) || defined(__APPLE__) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)
#include <dirent.h>
#ifndef HAVE_DIRENT_H
#define HAVE_DIRENT_H 2
#endif
#elif defined(_WIN32) && defined(_MSC_VER)
#define snprintf _snprintf
#define access _access
#endif

uint32_t tot_records = 0;
bool dump_mode = false;

/* ******************************************* */

static void help () {
    printf("Usage: fbmerge [-h] [-d] -i <input dir> -o <output dir>\n\n");
    printf("Merge FastBit directories enclosed in the root directory\n"
	   "specified with -i and saves merged data into the specified\n"
	   "output directory\n");
    exit(0);
}

/* ******************************************* */

int mergeDir(char *input_dir, char *output_dir) {
    unsigned int i, ret = 0;
    ibis::part part(input_dir, static_cast<const char*>(0));
    ibis::bitvector *bv;
    ibis::tablex *tablex = NULL;

    /* Scan colums */
    if (part.nRows() == 0) return(0);

    printf("Found %u records on directory %s\n", part.nRows(), input_dir);

    tot_records += part.nRows(); 

    bv = new ibis::bitvector();
    bv->appendFill(1, part.nRows()); /* Set all bitmaps to 1 */

    if (!dump_mode) {
	tablex = ibis::tablex::create();
    }

    for(i=0; i<part.nColumns(); i++) {
	unsigned char *s8;
	uint16_t *s16;
	uint32_t *s32;
	uint64_t *s64;
	ibis::column *c;
	char path[256];
	FILE *fd;

	c = part.getColumn(i);
	snprintf(path, sizeof(path), "%s/%s", output_dir, c->name());

	switch(c->elementSize()) {
	case 1: {
	    std::unique_ptr< ibis::array_t<unsigned char> >
		tmp(part.selectUBytes(c->name(), *bv));
	    s8 = tmp->begin();
	    if (dump_mode) {
		if ((fd = fopen(path, "a")) != NULL) {
		    for(unsigned j=0; j<part.nRows()-1; j++)
			fprintf(fd, "%u\n", s8[j]);
		    fclose(fd);
		}
	    } else {
		tablex->addColumn(c->name(), ibis::BYTE);
		tablex->append(c->name(), 0, part.nRows()-1, s8);
	    }
	    break;}
	case 2: {
	    std::unique_ptr< ibis::array_t<uint16_t> >
		tmp(part.selectUShorts(c->name(), *bv));
	    s16 = tmp->begin();
	    if (dump_mode) {
		if ((fd = fopen(path, "a")) != NULL) {
		    for(unsigned j=0; j<part.nRows()-1; j++)
			fprintf(fd, "%u\n", s16[j]);
		    fclose(fd);
		}
	    } else {
		tablex->addColumn(c->name(), ibis::SHORT);
		tablex->append(c->name(), 0, part.nRows()-1, s16);
	    }
	    break;}
	case 4: {
	    std::unique_ptr< ibis::array_t<uint32_t> >
		tmp(part.selectUInts(c->name(), *bv));
	    s32 = tmp->begin(); 
	    if (dump_mode) {
		if ((fd = fopen(path, "a")) != NULL) {
		    for(unsigned j=0; j<part.nRows()-1; j++)
			fprintf(fd, "%u\n", s32[j]);
		    fclose(fd);
		}
	    } else {
		tablex->addColumn(c->name(), ibis::INT);
		tablex->append(c->name(), 0, part.nRows()-1, s32);
	    }
	    break;}
	case 8: {
	    std::unique_ptr< ibis::array_t<uint64_t> >
		tmp(part.selectULongs(c->name(), *bv));
	    s64 = tmp->begin();
	    if (dump_mode) {
		if ((fd = fopen(path, "a")) != NULL) {
		    for(unsigned j=0; j<part.nRows()-1; j++)
			fprintf(fd, "%llu\n", s64[j]);
		    fclose(fd);
		}
	    } else {
		tablex->addColumn(c->name(), ibis::LONG);
		tablex->append(c->name(), 0, part.nRows()-1, s64);
	    }
	    break;}
	}
    }
  
    if (!dump_mode)
	tablex->write(output_dir, 0, 0);
    delete tablex;

    return(ret);
}

/* ******************************************* */

int walkDirs(char *input_dir, char *output_dir) {
    char partname[256];
    struct stat stats;

    printf("Processing directory %s\n", input_dir);
#ifdef HAVE_DIRENT_H
    DIR *dir;
    struct dirent *dirent;

    if ((dir = opendir(input_dir)) != NULL) {
	while ((dirent = readdir(dir))) {
	    char dirname[256];

	    if (dirent->d_name[0] == '.') continue;
	    snprintf(dirname, sizeof(dirname), "%s/%s", input_dir,
		     dirent->d_name);

	    if (stat(dirname, &stats) == 0) {
		if (S_ISDIR(stats.st_mode)) {
		    walkDirs(dirname, output_dir);
		}
	    }
	}

	closedir(dir);
    }
#endif

    snprintf(partname, sizeof(partname), "%s/-part.txt", input_dir);
    if (stat(partname, &stats) == 0) {
	if (access(partname, 4) == 0) // have read access
	    mergeDir(input_dir, output_dir);
	else
	    printf("WARNING: skipping unreadable directory %s\n", partname);
    }

    return(0);
}

/* ******************************************* */

int main (int argc, char * argv []) {
    char c, *input_dir = NULL, *output_dir = NULL;

#if defined(_WIN32) && defined(_MSC_VER)
    // don't have getopt on windows
    input_dir  = argv[1];
    output_dir = argv[2];
#else
    while ((c = getopt (argc, argv, "hvi:o:d")) != -1) {
	switch(c) {
	case 'd':
	    dump_mode = true;
	    break;

	case 'h':
	    help();
	    break;

	case 'i':
	    input_dir = strdup(optarg);
	    break;

	case 'o':
	    output_dir = strdup(optarg);
	    break;
	}
    }
#endif
    if ((!input_dir) || (!output_dir))
	help();

    printf("Searching FastBit dirs on %s...\n", input_dir);
    walkDirs(input_dir, output_dir);
    printf("Merged %u records into directory %s...\n", tot_records, output_dir);
    printf("Leaving...\n");
    exit(0);
}
