#!/bin/sh -
#
# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: gen_rec.awk,v 11.70 2002/08/08 15:44:47 bostic Exp $
#

# This awk script generates all the log, print, and read routines for the DB
# logging. It also generates a template for the recovery functions (these
# functions must still be edited, but are highly stylized and the initial
# template gets you a fair way along the path).
#
# For a given file prefix.src, we generate a file prefix_auto.c, and a file
# prefix_auto.h that contains:
#
#	external declarations for the file's functions
# 	defines for the physical record types
#	    (logical types are defined in each subsystem manually)
#	structures to contain the data unmarshalled from the log.
#
# This awk script requires that four variables be set when it is called:
#
#	source_file	-- the C source file being created
#	header_file	-- the C #include file being created
#	template_file	-- the template file being created
#
# And stdin must be the input file that defines the recovery setup.
#
# Within each file prefix.src, we use a number of public keywords (documented
# in the reference guide) as well as the following ones which are private to
# DB:
# 	DBPRIVATE	Indicates that a file will be built as part of DB,
#			rather than compiled independently, and so can use
#			DB-private interfaces (such as DB_NOCOPY).
#	DB		A DB handle.  Logs the dbreg fileid for that handle,
#			and makes the *_log interface take a DB * instead of a
#			DB_ENV *.
#	PGDBT		Just like DBT, only we know it stores a page or page
#			header, so we can byte-swap it (once we write the
#			byte-swapping code, which doesn't exist yet).
#	WRLOCK
#	WRLOCKNZ	An ARG that stores a db_pgno_t, which the getpgnos
#			function should acquire a lock on.  WRLOCK implies
#			that we should always get the lock;  WRLOCKNZ implies
#			that we should do so if and only if the pgno is non-zero
#			(unfortunately, 0 is both PGNO_INVALID and the main
#			metadata page number).

BEGIN {
	if (source_file == "" ||
	    header_file == "" || template_file == "") {
	    print "Usage: gen_rec.awk requires three variables to be set:"
	    print "\tsource_file\t-- the C source file being created"
	    print "\theader_file\t-- the C #include file being created"
	    print "\ttemplate_file\t-- the template file being created"
	    exit
	}
	FS="[\t ][\t ]*"
	CFILE=source_file
	HFILE=header_file
	TFILE=template_file
	dbprivate = 0
}
/^[ 	]*DBPRIVATE/ {
	dbprivate = 1
}
/^[ 	]*PREFIX/ {
	prefix = $2
	num_funcs = 0;

	# Start .c file.
	printf("/* Do not edit: automatically built by gen_rec.awk. */\n") \
	    > CFILE

	# Start .h file, make the entire file conditional.
	printf("/* Do not edit: automatically built by gen_rec.awk. */\n\n") \
	    > HFILE
	printf("#ifndef\t%s_AUTO_H\n#define\t%s_AUTO_H\n", prefix, prefix) \
	    >> HFILE;

	# Write recovery template file headers
	# This assumes we're doing DB recovery.
	printf("#include \"db_config.h\"\n\n") > TFILE
	printf("#ifndef NO_SYSTEM_INCLUDES\n") >> TFILE
	printf("#include <sys/types.h>\n\n") >> TFILE
	printf("#include <string.h>\n") >> TFILE
	printf("#endif\n\n") >> TFILE
	printf("#include \"db_int.h\"\n") >> TFILE
	printf("#include \"dbinc/db_page.h\"\n") >> TFILE
	printf("#include \"dbinc/%s.h\"\n", prefix) >> TFILE
	printf("#include \"dbinc/log.h\"\n\n") >> TFILE
}
/^[ 	]*INCLUDE/ {
	if ($3 == "")
		printf("%s\n", $2) >> CFILE
	else
		printf("%s %s\n", $2, $3) >> CFILE
}
/^[ 	]*(BEGIN|IGNORED)/ {
	if (in_begin) {
		print "Invalid format: missing END statement"
		exit
	}
	in_begin = 1;
	is_dbt = 0;
	has_dbp = 0;
	is_uint = 0;
	need_log_function = ($1 == "BEGIN");
	nvars = 0;

	# number of locks that the getpgnos functions will return
	nlocks = 0;

	thisfunc = $2;
	funcname = sprintf("%s_%s", prefix, $2);

	rectype = $3;

	funcs[num_funcs] = funcname;
	++num_funcs;
}
/^[ 	]*(DB|ARG|DBT|PGDBT|POINTER|WRLOCK|WRLOCKNZ)/ {
	vars[nvars] = $2;
	types[nvars] = $3;
	atypes[nvars] = $1;
	modes[nvars] = $1;
	formats[nvars] = $NF;
	for (i = 4; i < NF; i++)
		types[nvars] = sprintf("%s %s", types[nvars], $i);

	if ($1 == "DB") {
		has_dbp = 1;
	}

	if ($1 == "DB" || $1 == "ARG" || $1 == "WRLOCK" || $1 == "WRLOCKNZ") {
		sizes[nvars] = sprintf("sizeof(u_int32_t)");
		is_uint = 1;
	} else if ($1 == "POINTER")
		sizes[nvars] = sprintf("sizeof(*%s)", $2);
	else { # DBT, PGDBT
		sizes[nvars] = \
		    sprintf("sizeof(u_int32_t) + (%s == NULL ? 0 : %s->size)", \
		    $2, $2);
		is_dbt = 1;
	}
	nvars++;
}
/^[	]*(WRLOCK|WRLOCKNZ)/ {
	nlocks++;

	if ($1 == "WRLOCK") {
		lock_if_zero[nlocks] = 1;
	} else {
		lock_if_zero[nlocks] = 0;
	}

	lock_pgnos[nlocks] = $2;
}
/^[ 	]*END/ {
	if (!in_begin) {
		print "Invalid format: missing BEGIN statement"
		exit;
	}

	# Declare the record type.
	printf("#define\tDB_%s\t%d\n", funcname, rectype) >> HFILE

	# Structure declaration.
	printf("typedef struct _%s_args {\n", funcname) >> HFILE

	# Here are the required fields for every structure
	printf("\tu_int32_t type;\n\tDB_TXN *txnid;\n") >> HFILE
	printf("\tDB_LSN prev_lsn;\n") >>HFILE

	# Here are the specified fields.
	for (i = 0; i < nvars; i++) {
		t = types[i];
		if (modes[i] == "POINTER") {
			ndx = index(t, "*");
			t = substr(types[i], 0, ndx - 2);
		}
		printf("\t%s\t%s;\n", t, vars[i]) >> HFILE
	}
	printf("} %s_args;\n\n", funcname) >> HFILE

	# Output the log, print, read, and getpgnos functions.
	if (need_log_function) {
		log_function();

		# The getpgnos function calls DB-private (__rep_*) functions,
		# so we only generate it for our own logging functions,
		# not application-specific ones.
		if (dbprivate) {
			getpgnos_function();
		}
	}
	print_function();
	read_function();

	# Recovery template
	cmd = sprintf(\
    "sed -e s/PREF/%s/ -e s/FUNC/%s/ < template/rec_ctemp >> %s",
	    prefix, thisfunc, TFILE)
	system(cmd);

	# Done writing stuff, reset and continue.
	in_begin = 0;
}

END {
	# End the conditional for the HFILE
	printf("#endif\n") >> HFILE;

	# Print initialization routine; function prototype
	p[1] = sprintf("int %s_init_print %s%s", prefix,
	    "__P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, ",
	    "db_recops, void *), size_t *));");
	p[2] = "";
	proto_format(p);

	# Create the routine to call __db_add_recovery(print_fn, id)
	printf("int\n%s_init_print(dbenv, dtabp, dtabsizep)\n", \
	    prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;;
	printf("\tint (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *,") >> CFILE;
	printf(" db_recops, void *));\n") >> CFILE;
	printf("\tsize_t *dtabsizep;\n{\n") >> CFILE;
	# If application-specific, the user will need a prototype for
	# __db_add_recovery, since they won't have DB's.
	if (!dbprivate) {
		printf("\tint __db_add_recovery __P((DB_ENV *,\n") >> CFILE;
		printf(\
"\t    int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *),\n") >> CFILE;
		printf("\t    size_t *,\n") >> CFILE;
		printf(\
"\t    int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), u_int32_t));\n") \
		    >> CFILE;
	}

	printf("\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv, ") >> CFILE;
		printf("dtabp, dtabsizep,\n") >> CFILE;
		printf("\t    %s_print, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

	# We only want to generate *_init_{getpgnos,recover} functions
	# if this is a DB-private, rather than application-specific,
	# set of recovery functions.  Application-specific recovery functions
	# should be dispatched using the DB_ENV->set_app_dispatch callback
	# rather than a DB dispatch table ("dtab").
	if (!dbprivate)
		exit

	# Page number initialization routine; function prototype
	p[1] = sprintf("int %s_init_getpgnos %s%s", prefix,
	    "__P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, ",
	    "db_recops, void *), size_t *));");
	p[2] = "";
	proto_format(p);

	# Create the routine to call db_add_recovery(pgno_fn, id)
	printf("int\n%s_init_getpgnos(dbenv, dtabp, dtabsizep)\n", \
	    prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tint (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *,") >> CFILE;
	printf(" db_recops, void *));\n") >> CFILE;
	printf("\tsize_t *dtabsizep;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv, ") >> CFILE;
		printf("dtabp, dtabsizep,\n") >> CFILE;
		printf("\t    %s_getpgnos, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

	# Recover initialization routine
	p[1] = sprintf("int %s_init_recover %s%s", prefix,
	    "__P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, ",
	    "db_recops, void *), size_t *));");
	p[2] = "";
	proto_format(p);

	# Create the routine to call db_add_recovery(func, id)
	printf("int\n%s_init_recover(dbenv, dtabp, dtabsizep)\n", \
	    prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tint (***dtabp)__P((DB_ENV *, DBT *, DB_LSN *,") >> CFILE;
	printf(" db_recops, void *));\n") >> CFILE;
	printf("\tsize_t *dtabsizep;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv, ") >> CFILE;
		printf("dtabp, dtabsizep,\n") >> CFILE;
		printf("\t    %s_recover, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n") >> CFILE;
}

function log_function() {
	# Write the log function; function prototype
	pi = 1;
	p[pi++] = sprintf("int %s_log", funcname);
	p[pi++] = " ";
	if (has_dbp == 1) {
		p[pi++] = "__P((DB *, DB_TXN *, DB_LSN *, u_int32_t";
	} else {
		p[pi++] = "__P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t";
	}
	for (i = 0; i < nvars; i++) {
		if (modes[i] == "DB")
			continue;
		p[pi++] = ", ";
		p[pi++] = sprintf("%s%s%s",
		    (modes[i] == "DBT" || modes[i] == "PGDBT") ? "const " : "",
		    types[i],
		    (modes[i] == "DBT" || modes[i] == "PGDBT") ? " *" : "");
	}
	p[pi++] = "";
	p[pi++] = "));";
	p[pi++] = "";
	proto_format(p);

	# Function declaration
	if (has_dbp == 1) {
		printf("int\n%s_log(dbp, txnid, ret_lsnp, flags", \
		    funcname) >> CFILE;
	} else {
		printf("int\n%s_log(dbenv, txnid, ret_lsnp, flags", \
		    funcname) >> CFILE;
	}
	for (i = 0; i < nvars; i++) {
		if (modes[i] == "DB") {
			# We pass in fileids on the dbp, so if this is one,
			# skip it.
			continue;
		}
		printf(",") >> CFILE;
		if ((i % 6) == 0)
			printf("\n    ") >> CFILE;
		else
			printf(" ") >> CFILE;
		printf("%s", vars[i]) >> CFILE;
	}
	printf(")\n") >> CFILE;

	# Now print the parameters
	if (has_dbp == 1) {
		printf("\tDB *dbp;\n") >> CFILE;
	} else {
		printf("\tDB_ENV *dbenv;\n") >> CFILE;
	}
	printf("\tDB_TXN *txnid;\n\tDB_LSN *ret_lsnp;\n") >> CFILE;
	printf("\tu_int32_t flags;\n") >> CFILE;
	for (i = 0; i < nvars; i++) {
		# We just skip for modes == DB.
		if (modes[i] == "DBT" || modes[i] == "PGDBT")
			printf("\tconst %s *%s;\n", types[i], vars[i]) >> CFILE;
		else if (modes[i] != "DB")
			printf("\t%s %s;\n", types[i], vars[i]) >> CFILE;
	}

	# Function body and local decls
	printf("{\n") >> CFILE;
	printf("\tDBT logrec;\n") >> CFILE;
	if (has_dbp == 1)
		printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tDB_LSN *lsnp, null_lsn;\n") >> CFILE;
	if (is_dbt == 1)
		printf("\tu_int32_t zero;\n") >> CFILE;
	if (is_uint == 1)
		printf("\tu_int32_t uinttmp;\n") >> CFILE;
	printf("\tu_int32_t npad, rectype, txn_num;\n") >> CFILE;
	printf("\tint ret;\n") >> CFILE;
	printf("\tu_int8_t *bp;\n\n") >> CFILE;

	# Initialization
	if (has_dbp == 1)
		printf("\tdbenv = dbp->dbenv;\n") >> CFILE;
	printf("\trectype = DB_%s;\n", funcname) >> CFILE;
	printf("\tnpad = 0;\n\n") >> CFILE;

	printf("\tif (txnid == NULL) {\n") >> CFILE;
	printf("\t\ttxn_num = 0;\n") >> CFILE;
	printf("\t\tnull_lsn.file = 0;\n") >> CFILE;
	printf("\t\tnull_lsn.offset = 0;\n") >> CFILE;
	printf("\t\tlsnp = &null_lsn;\n") >> CFILE;
	printf("\t} else {\n") >> CFILE;
	if (funcname != "__db_debug" && dbprivate) {
		printf(\
		    "\t\tif (TAILQ_FIRST(&txnid->kids) != NULL &&\n") >> CFILE;
		printf("\t\t    (ret = __txn_activekids(") >> CFILE;
		printf("dbenv, rectype, txnid)) != 0)\n") >> CFILE;
		printf("\t\t\treturn (ret);\n") >> CFILE;
	}
	printf("\t\ttxn_num = txnid->txnid;\n") >> CFILE;
	printf("\t\tlsnp = &txnid->last_lsn;\n") >> CFILE;
	printf("\t}\n\n") >> CFILE;

	# Malloc
	printf("\tlogrec.size = sizeof(rectype) + ") >> CFILE;
	printf("sizeof(txn_num) + sizeof(DB_LSN)") >> CFILE;
	for (i = 0; i < nvars; i++)
		printf("\n\t    + %s", sizes[i]) >> CFILE;
	printf(";\n") >> CFILE
	if (dbprivate) {
		printf("\tif (CRYPTO_ON(dbenv)) {\n") >> CFILE;
		printf("\t\tnpad =\n") >> CFILE
		printf(\
"\t\t    ((DB_CIPHER *)dbenv->crypto_handle)->adj_size(logrec.size);\n")\
		    >> CFILE;
		printf("\t\tlogrec.size += npad;\n\t}\n\n") >> CFILE
	}
	write_malloc("logrec.data", "logrec.size", CFILE)
	printf("\tif (npad > 0)\n") >> CFILE;
	printf("\t\tmemset((u_int8_t *)logrec.data + logrec.size ") >> CFILE;
	printf("- npad, 0, npad);\n\n") >> CFILE;

	# Copy args into buffer
	printf("\tbp = logrec.data;\n\n") >> CFILE;
	printf("\tmemcpy(bp, &rectype, sizeof(rectype));\n") >> CFILE;
	printf("\tbp += sizeof(rectype);\n\n") >> CFILE;
	printf("\tmemcpy(bp, &txn_num, sizeof(txn_num));\n") >> CFILE;
	printf("\tbp += sizeof(txn_num);\n\n") >> CFILE;
	printf("\tmemcpy(bp, lsnp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n\n") >> CFILE;

	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "ARG" || modes[i] == "WRLOCK" || \
		    modes[i] == "WRLOCKNZ") {
			printf("\tuinttmp = (u_int32_t)%s;\n", \
			    vars[i]) >> CFILE;
			printf("\tmemcpy(bp, &uinttmp, sizeof(uinttmp));\n") \
			    >> CFILE;
			printf("\tbp += sizeof(uinttmp);\n\n") >> CFILE;
		} else if (modes[i] == "DBT" || modes[i] == "PGDBT") {
			printf("\tif (%s == NULL) {\n", vars[i]) >> CFILE;
			printf("\t\tzero = 0;\n") >> CFILE;
			printf("\t\tmemcpy(bp, &zero, sizeof(u_int32_t));\n") \
				>> CFILE;
			printf("\t\tbp += sizeof(u_int32_t);\n") >> CFILE;
			printf("\t} else {\n") >> CFILE;
			printf("\t\tmemcpy(bp, &%s->size, ", vars[i]) >> CFILE;
			printf("sizeof(%s->size));\n", vars[i]) >> CFILE;
			printf("\t\tbp += sizeof(%s->size);\n", vars[i]) \
			    >> CFILE;
			printf("\t\tmemcpy(bp, %s->data, %s->size);\n", \
			    vars[i], vars[i]) >> CFILE;
			printf("\t\tbp += %s->size;\n\t}\n\n", \
			    vars[i]) >> CFILE;
		} else if (modes[i] == "DB") {
			# We need to log a DB handle.  To do this, we
			# actually just log its fileid;  from that, we'll
			# be able to acquire an open handle at recovery time.
			printf("\tDB_ASSERT(dbp->log_filename != NULL);\n") \
			    >> CFILE;
			printf("\tif (dbp->log_filename->id == ") >> CFILE;
			printf("DB_LOGFILEID_INVALID &&\n\t    ") >> CFILE
			printf("(ret = __dbreg_lazy_id(dbp)) != 0)\n") \
			    >> CFILE;
			printf("\t\treturn (ret);\n\n") >> CFILE;

			printf("\tuinttmp = ") >> CFILE;
			printf("(u_int32_t)dbp->log_filename->id;\n") >> CFILE;
			printf("\tmemcpy(bp, &uinttmp, sizeof(uinttmp));\n") \
			    >> CFILE;
			printf("\tbp += sizeof(uinttmp);\n\n") >> CFILE;
		} else { # POINTER
			printf("\tif (%s != NULL)\n", vars[i]) >> CFILE;
			printf("\t\tmemcpy(bp, %s, %s);\n", vars[i], \
			    sizes[i]) >> CFILE;
			printf("\telse\n") >> CFILE;
			printf("\t\tmemset(bp, 0, %s);\n", sizes[i]) >> CFILE;
			printf("\tbp += %s;\n\n", sizes[i]) >> CFILE;
		}
	}

	# Error checking.  User code won't have DB_ASSERT available, but
	# this is a pretty unlikely assertion anyway, so we just leave it out
	# rather than requiring assert.h.
	if (dbprivate) {
		printf("\tDB_ASSERT((u_int32_t)") >> CFILE;
		printf("(bp - (u_int8_t *)logrec.data) <= logrec.size);\n") \
		    >> CFILE;
	}

	# Issue log call
	# We didn't call the crypto alignment function when we created this
	# log record (because we don't have the right header files to find
	# the function), so we have to copy the log record to make sure the
	# alignment is correct.
	printf(\
	    "\tret = dbenv->log_put(dbenv,\n\t   ret_lsnp, (DBT *)&logrec, ") \
	    >> CFILE;
	if (dbprivate) {
		printf("flags | DB_NOCOPY);\n") >> CFILE;
	} else {
		printf("flags);\n") >> CFILE;
	}

	# Update the transactions last_lsn
	printf("\tif (txnid != NULL && ret == 0)\n") >> CFILE;
	printf("\t\ttxnid->last_lsn = *ret_lsnp;\n") >> CFILE;

	# If out of disk space log writes may fail.  If we are debugging
	# that print out which records did not make it to disk.
	printf("#ifdef LOG_DIAGNOSTIC\n") >> CFILE
	printf("\tif (ret != 0)\n") >> CFILE;
	printf("\t\t(void)%s_print(dbenv,\n", funcname) >> CFILE;
	printf("\t\t    (DBT *)&logrec, ret_lsnp, NULL, NULL);\n") >> CFILE
	printf("#endif\n") >> CFILE

	# Free and return
	write_free("logrec.data", CFILE)
	printf("\treturn (ret);\n}\n\n") >> CFILE;
}

function print_function() {
	# Write the print function; function prototype
	p[1] = sprintf("int %s_print", funcname);
	p[2] = " ";
	p[3] = "__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));";
	p[4] = "";
	proto_format(p);

	# Function declaration
	printf("int\n%s_print(dbenv, ", funcname) >> CFILE;
	printf("dbtp, lsnp, notused2, notused3)\n") >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tDBT *dbtp;\n") >> CFILE;
	printf("\tDB_LSN *lsnp;\n") >> CFILE;
	printf("\tdb_recops notused2;\n\tvoid *notused3;\n{\n") >> CFILE;

	# Locals
	printf("\t%s_args *argp;\n", funcname) >> CFILE;
	for (i = 0; i < nvars; i ++)
		if (modes[i] == "DBT" || modes[i] == "PGDBT") {
			printf("\tu_int32_t i;\n") >> CFILE
			printf("\tint ch;\n") >> CFILE
			break;
		}

	printf("\tint ret;\n\n") >> CFILE;

	# Get rid of complaints about unused parameters.
	printf("\tnotused2 = DB_TXN_ABORT;\n\tnotused3 = NULL;\n\n") >> CFILE;

	# Call read routine to initialize structure
	printf("\tif ((ret = %s_read(dbenv, dbtp->data, &argp)) != 0)\n", \
	    funcname) >> CFILE;
	printf("\t\treturn (ret);\n") >> CFILE;

	# Print values in every record
	printf("\t(void)printf(\n\t    \"[%%lu][%%lu]%s: ", funcname) >> CFILE;
	printf("rec: %%lu txnid %%lx ") >> CFILE;
	printf("prevlsn [%%lu][%%lu]\\n\",\n") >> CFILE;
	printf("\t    (u_long)lsnp->file,\n") >> CFILE;
	printf("\t    (u_long)lsnp->offset,\n") >> CFILE;
	printf("\t    (u_long)argp->type,\n") >> CFILE;
	printf("\t    (u_long)argp->txnid->txnid,\n") >> CFILE;
	printf("\t    (u_long)argp->prev_lsn.file,\n") >> CFILE;
	printf("\t    (u_long)argp->prev_lsn.offset);\n") >> CFILE;

	# Now print fields of argp
	for (i = 0; i < nvars; i ++) {
		printf("\t(void)printf(\"\\t%s: ", vars[i]) >> CFILE;

		if (modes[i] == "DBT" || modes[i] == "PGDBT") {
			printf("\");\n") >> CFILE;
			printf("\tfor (i = 0; i < ") >> CFILE;
			printf("argp->%s.size; i++) {\n", vars[i]) >> CFILE;
			printf("\t\tch = ((u_int8_t *)argp->%s.data)[i];\n", \
			    vars[i]) >> CFILE;
			printf("\t\tprintf(isprint(ch) || ch == 0x0a") >> CFILE;
			printf(" ? \"%%c\" : \"%%#x \", ch);\n") >> CFILE;
			printf("\t}\n\t(void)printf(\"\\n\");\n") >> CFILE;
		} else if (types[i] == "DB_LSN *") {
			printf("[%%%s][%%%s]\\n\",\n", \
			    formats[i], formats[i]) >> CFILE;
			printf("\t    (u_long)argp->%s.file,", \
			    vars[i]) >> CFILE;
			printf(" (u_long)argp->%s.offset);\n", \
			    vars[i]) >> CFILE;
		} else {
			if (formats[i] == "lx")
				printf("0x") >> CFILE;
			printf("%%%s\\n\", ", formats[i]) >> CFILE;
			if (formats[i] == "lx" || formats[i] == "lu")
				printf("(u_long)") >> CFILE;
			if (formats[i] == "ld")
				printf("(long)") >> CFILE;
			printf("argp->%s);\n", vars[i]) >> CFILE;
		}
	}
	printf("\t(void)printf(\"\\n\");\n") >> CFILE;
	write_free("argp", CFILE);
	printf("\treturn (0);\n") >> CFILE;
	printf("}\n\n") >> CFILE;
}

function read_function() {
	# Write the read function; function prototype
	p[1] = sprintf("int %s_read __P((DB_ENV *, void *,", funcname);
	p[2] = " ";
	p[3] = sprintf("%s_args **));", funcname);
	p[4] = "";
	proto_format(p);

	# Function declaration
	printf("int\n%s_read(dbenv, recbuf, argpp)\n", funcname) >> CFILE;

	# Now print the parameters
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tvoid *recbuf;\n") >> CFILE;
	printf("\t%s_args **argpp;\n", funcname) >> CFILE;

	# Function body and local decls
	printf("{\n\t%s_args *argp;\n", funcname) >> CFILE;
	if (is_uint == 1)
		printf("\tu_int32_t uinttmp;\n") >> CFILE;
	printf("\tu_int8_t *bp;\n") >> CFILE;


	if (dbprivate) {
		# We only use dbenv and ret in the private malloc case.
		printf("\tint ret;\n\n") >> CFILE;
	} else {
		printf("\t/* Keep the compiler quiet. */\n") >> CFILE;
		printf("\n\tdbenv = NULL;\n") >> CFILE;
	}

	malloc_size = sprintf("sizeof(%s_args) + sizeof(DB_TXN)", funcname)
	write_malloc("argp", malloc_size, CFILE)

	# Set up the pointers to the txnid.
	printf("\targp->txnid = (DB_TXN *)&argp[1];\n\n") >> CFILE;

	# First get the record type, prev_lsn, and txnid fields.

	printf("\tbp = recbuf;\n") >> CFILE;
	printf("\tmemcpy(&argp->type, bp, sizeof(argp->type));\n") >> CFILE;
	printf("\tbp += sizeof(argp->type);\n\n") >> CFILE;
	printf("\tmemcpy(&argp->txnid->txnid,  bp, ") >> CFILE;
	printf("sizeof(argp->txnid->txnid));\n") >> CFILE;
	printf("\tbp += sizeof(argp->txnid->txnid);\n\n") >> CFILE;
	printf("\tmemcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n\n") >> CFILE;

	# Now get rest of data.
	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "DBT" || modes[i] == "PGDBT") {
			printf("\tmemset(&argp->%s, 0, sizeof(argp->%s));\n", \
			    vars[i], vars[i]) >> CFILE;
			printf("\tmemcpy(&argp->%s.size, ", vars[i]) >> CFILE;
			printf("bp, sizeof(u_int32_t));\n") >> CFILE;
			printf("\tbp += sizeof(u_int32_t);\n") >> CFILE;
			printf("\targp->%s.data = bp;\n", vars[i]) >> CFILE;
			printf("\tbp += argp->%s.size;\n", vars[i]) >> CFILE;
		} else if (modes[i] == "ARG" || modes[i] == "WRLOCK" || \
		    modes[i] == "WRLOCKNZ" || modes[i] == "DB") {
			printf("\tmemcpy(&uinttmp, bp, sizeof(uinttmp));\n") \
			    >> CFILE;
			printf("\targp->%s = (%s)uinttmp;\n", vars[i], \
			    types[i]) >> CFILE;
			printf("\tbp += sizeof(uinttmp);\n") >> CFILE;
		} else { # POINTER
			printf("\tmemcpy(&argp->%s, bp, ", vars[i]) >> CFILE;
			printf(" sizeof(argp->%s));\n", vars[i]) >> CFILE;
			printf("\tbp += sizeof(argp->%s);\n", vars[i]) >> CFILE;
		}
		printf("\n") >> CFILE;
	}

	# Free and return
	printf("\t*argpp = argp;\n") >> CFILE;
	printf("\treturn (0);\n}\n\n") >> CFILE;
}

function getpgnos_function() {
	# Write the getpgnos function;  function prototype
	p[1] = sprintf("int %s_getpgnos", funcname);
	p[2] = " ";
	p[3] = "__P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));";
	p[4] = "";
	proto_format(p);

	# Function declaration
	printf("int\n%s_getpgnos(dbenv, ", funcname) >> CFILE;
	printf("rec, lsnp, notused1, summary)\n") >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tDBT *rec;\n") >> CFILE;
	printf("\tDB_LSN *lsnp;\n") >> CFILE;
	printf("\tdb_recops notused1;\n") >> CFILE;
	printf("\tvoid *summary;\n{\n") >> CFILE;

	# If there are no locks, return this fact.
	if (nlocks == 0) {
		printf("\tTXN_RECS *t;\n") >> CFILE;
		printf("\tint ret;\n") >> CFILE;
		printf("\tCOMPQUIET(rec, NULL);\n") >> CFILE;
		printf("\tCOMPQUIET(notused1, DB_TXN_ABORT);\n") >> CFILE;

		printf("\n\tt = (TXN_RECS *)summary;\n") >> CFILE;
		printf("\n\tif ((ret = __rep_check_alloc(dbenv, ") >> CFILE;
		printf("t, 1)) != 0)\n") >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;

		printf("\n\tt->array[t->npages].flags = LSN_PAGE_NOLOCK;\n") \
			>> CFILE;
		printf("\tt->array[t->npages].lsn = *lsnp;\n") >> CFILE;
		printf("\tt->array[t->npages].fid = DB_LOGFILEID_INVALID;\n") \
			>> CFILE;
		printf("\tmemset(&t->array[t->npages].pgdesc, 0,\n") >> CFILE;
		printf("\t    sizeof(t->array[t->npages].pgdesc));\n") >> CFILE;
		printf("\n\tt->npages++;\n") >> CFILE;

		printf("\n") >> CFILE;
		printf("\treturn (0);\n") >> CFILE;
		printf("}\n\n") >> CFILE;
		return;
	}

	# Locals
	printf("\tDB *dbp;\n") >> CFILE;
	printf("\tTXN_RECS *t;\n") >> CFILE;
	printf("\t%s_args *argp;\n", funcname) >> CFILE;
	printf("\tu_int32_t ret;\n\n") >> CFILE;

	# Shut up compiler.
	printf("\tCOMPQUIET(notused1, DB_TXN_ABORT);\n\n") >> CFILE;

	printf("\targp = NULL;\n") >> CFILE;
	printf("\tt = (TXN_RECS *)summary;\n\n") >> CFILE;

	printf("\tif ((ret = %s_read(dbenv, rec->data, &argp)) != 0)\n", \
		funcname) >> CFILE;
	printf("\t\treturn (ret);\n") >> CFILE;

	# Get file ID.
	printf("\n\tif ((ret = __dbreg_id_to_db(dbenv,\n\t    ") >> CFILE;
	printf("argp->txnid, &dbp, argp->fileid, 0)) != 0)\n") >> CFILE;
	printf("\t\tgoto err;\n") >> CFILE;

	printf("\n\tif ((ret = __rep_check_alloc(dbenv, t, %d)) != 0)\n", \
		nlocks) >> CFILE;
	printf("\t\tgoto err;\n\n") >> CFILE;

	for (i = 1; i <= nlocks; i++) {
		if (lock_if_zero[i]) {
			indent = "\t";
		} else {
			indent = "\t\t";
			printf("\tif (argp->%s != PGNO_INVALID) {\n", \
				lock_pgnos[i]) >> CFILE;
		}
		printf("%st->array[t->npages].flags = 0;\n", indent) >> CFILE;
		printf("%st->array[t->npages].fid = argp->fileid;\n", indent) \
		    >> CFILE;
		printf("%st->array[t->npages].lsn = *lsnp;\n", indent) >> CFILE;
		printf("%st->array[t->npages].pgdesc.pgno = argp->%s;\n", \
			indent, lock_pgnos[i]) >> CFILE;
		printf("%st->array[t->npages].pgdesc.type = DB_PAGE_LOCK;\n", \
			indent) >> CFILE;
		printf("%smemcpy(t->array[t->npages].pgdesc.fileid, ", indent) \
			>> CFILE;
		printf("dbp->fileid,\n%s    DB_FILE_ID_LEN);\n", \
			indent, indent) >> CFILE;
		printf("%st->npages++;\n", indent) >> CFILE;
		if (!lock_if_zero[i]) {
			printf("\t}\n") >> CFILE;
		}
	}

	printf("\nerr:\tif (argp != NULL)\n") >> CFILE;
	write_free("argp", CFILE);

	printf("\treturn (ret);\n") >> CFILE;

	printf("}\n\n") >> CFILE;
}

# proto_format --
#	Pretty-print a function prototype.
function proto_format(p)
{
	printf("/*\n") >> CFILE;

	s = "";
	for (i = 1; i in p; ++i)
		s = s p[i];

	t = " * PUBLIC: "
	if (length(s) + length(t) < 80)
		printf("%s%s", t, s) >> CFILE;
	else {
		split(s, p, "__P");
		len = length(t) + length(p[1]);
		printf("%s%s", t, p[1]) >> CFILE

		n = split(p[2], comma, ",");
		comma[1] = "__P" comma[1];
		for (i = 1; i <= n; i++) {
			if (len + length(comma[i]) > 70) {
				printf("\n * PUBLIC:    ") >> CFILE;
				len = 0;
			}
			printf("%s%s", comma[i], i == n ? "" : ",") >> CFILE;
			len += length(comma[i]) + 2;
		}
	}
	printf("\n */\n") >> CFILE;
	delete p;
}

function write_malloc(ptr, size, file)
{
	if (dbprivate) {
		printf("\tif ((ret = ") >> file;
		printf(\
		    "__os_malloc(dbenv,\n\t    " size ", &" ptr ")) != 0)\n") \
		    >> file
		printf("\t\treturn (ret);\n\n") >> file;
	} else {
		printf("\tif ((" ptr " = malloc(" size ")) == NULL)\n") >> file
		printf("\t\treturn (ENOMEM);\n\n") >> file
	}
}

function write_free(ptr, file)
{
	if (dbprivate) {
		printf("\t__os_free(dbenv, " ptr ");\n") >> file
	} else {
		printf("\tfree(" ptr ");\n") >> file
	}
}
