#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2005 Mark Wong & Open Source Development Labs, Inc.
#

AUTOMAKE_OPTIONS = no-dependencies

# Can't include dbgen until we can figure out how to conver the Makefile for
# automake.
#SUBDIRS = dbgen

INCLUDES = -Iinclude

bin_PROGRAMS = parse_query gtime power

parse_query_SOURCES = main.c get_statement.c

gtime_SOURCES = get_time.c

power_SOURCES = power.c

EXTRA_DIST = \
             include/common.h \
             include/db.h \
             dbgen/BUGS \
             dbgen/CHANGES \
             dbgen/HISTORY \
             dbgen/PORTING.NOTES \
             dbgen/README \
             dbgen/bcd2.c \
             dbgen/bcd2.h \
             dbgen/bm_utils.c \
             dbgen/build.c \
             dbgen/config.h \
             dbgen/dists.dss \
             dbgen/driver.c \
             dbgen/dss.ddl \
             dbgen/dss.h \
             dbgen/dss.ri \
             dbgen/dsstypes.h \
             dbgen/history.html \
             dbgen/load_stub.c \
             dbgen/makefile.suite \
             dbgen/permute.c \
             dbgen/permute.h \
             dbgen/print.c \
             dbgen/qgen.c \
             dbgen/rnd.c \
             dbgen/rnd.h \
             dbgen/shared.h \
             dbgen/speed_seed.c \
             dbgen/text.c \
             dbgen/tpcd.h \
             dbgen/varsub.c \
             dbgen/Makefile
