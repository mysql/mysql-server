#!/bin/sh -
#
# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998, 1999, 2000
#	Sleepycat Software.  All rights reserved.
#
# $Id: gen_rec.awk,v 11.26 2001/01/08 21:06:46 bostic Exp $
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
# This awk script requires that five variables be set when it is called:
#
#	source_file	-- the C source file being created
#	subsystem	-- the subsystem prefix, e.g., "db"
#	header_file	-- the C #include file being created
#	template_file	-- the template file being created
#	template_dir	-- the directory to find the source template
#
# And stdin must be the input file that defines the recovery setup.

BEGIN {
	if (source_file == "" || subsystem == "" ||
	    header_file == "" || template_file == "" || template_dir == "") {
	    print "Usage: gen_rec.awk requires five variables to be set:"
	    print "\tsource_file\t-- the C source file being created"
	    print "\tsubsystem\t-- the subsystem prefix, e.g., \"db\""
	    print "\theader_file\t-- the C #include file being created"
	    print "\ttemplate_file\t-- the template file being created"
	    print "\ttemplate_dir\t-- the directory to find the source template"
	    exit
	}
	FS="[\t ][\t ]*"
	CFILE=source_file
	NAME=subsystem
	HFILE=header_file
	TFILE=template_file
	TDIR=template_dir
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
	printf("#include \"db_page.h\"\n") >> TFILE
	printf("#include \"%s.h\"\n", prefix) >> TFILE
	printf("#include \"log.h\"\n\n") >> TFILE
}
/^[ 	]*INCLUDE/ {
	if ($3 == "")
		printf("%s\n", $2) >> CFILE
	else
		printf("%s %s\n", $2, $3) >> CFILE
}
/^[ 	]*(BEGIN|DEPRECATED)/ {
	if (in_begin) {
		print "Invalid format: missing END statement"
		exit
	}
	in_begin = 1;
	is_dbt = 0;
	is_deprecated = ($1 == "DEPRECATED");
	nvars = 0;

	thisfunc = $2;
	funcname = sprintf("%s_%s", prefix, $2);

	rectype = $3;

	funcs[num_funcs] = funcname;
	funcs_dep[num_funcs] = is_deprecated;
	++num_funcs;
}
/^[ 	]*(ARG|DBT|POINTER)/ {
	vars[nvars] = $2;
	types[nvars] = $3;
	atypes[nvars] = $1;
	modes[nvars] = $1;
	formats[nvars] = $NF;
	for (i = 4; i < NF; i++)
		types[nvars] = sprintf("%s %s", types[nvars], $i);

	if ($1 == "ARG")
		sizes[nvars] = sprintf("sizeof(%s)", $2);
	else if ($1 == "POINTER")
		sizes[nvars] = sprintf("sizeof(*%s)", $2);
	else { # DBT
		sizes[nvars] = \
		    sprintf("sizeof(u_int32_t) + (%s == NULL ? 0 : %s->size)", \
		    $2, $2);
		is_dbt = 1;
	}
	nvars++;
}
/^[ 	]*END/ {
	if (!in_begin) {
		print "Invalid format: missing BEGIN statement"
		exit;
	}

	# Declare the record type.
	printf("\n#define\tDB_%s\t%d\n", funcname, rectype) >> HFILE

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
	printf("} __%s_args;\n\n", funcname) >> HFILE

	# Output the log, print and read functions.
	if (!is_deprecated)
		log_function();
	print_function();
	read_function();

	# Recovery template
	cmd = sprintf("sed -e s/PREF/%s/ -e s/FUNC/%s/ < %s/rec_ctemp >> %s",
	    prefix, thisfunc, TDIR, TFILE)
	system(cmd);

	# Done writing stuff, reset and continue.
	in_begin = 0;
}

END {
	# Print initialization routine; function prototype
	printf("int __%s_init_print __P((DB_ENV *));\n", prefix) >> HFILE;

	# Create the routine to call db_add_recovery(print_fn, id)
	printf("int\n__%s_init_print(dbenv)\n", prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv,\n") >> CFILE;
		printf("\t    __%s_print, DB_%s)) != 0)\n", \
		    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

	# Recover initialization routine
	printf("int __%s_init_recover __P((DB_ENV *));\n", prefix) >> HFILE;

	# Create the routine to call db_add_recovery(func, id)
	printf("int\n__%s_init_recover(dbenv)\n", prefix) >> CFILE;
	printf("\tDB_ENV *dbenv;\n{\n\tint ret;\n\n") >> CFILE;
	for (i = 0; i < num_funcs; i++) {
		printf("\tif ((ret = __db_add_recovery(dbenv,\n") >> CFILE;
		if (funcs_dep[i] == 1)
			printf("\t    __deprecated_recover, DB_%s)) != 0)\n", \
			    funcs[i]) >> CFILE;
		else
			printf("\t    __%s_recover, DB_%s)) != 0)\n", \
			    funcs[i], funcs[i]) >> CFILE;
		printf("\t\treturn (ret);\n") >> CFILE;
	}
	printf("\treturn (0);\n}\n\n") >> CFILE;

	# End the conditional for the HFILE
	printf("#endif\n") >> HFILE;
}

function log_function() {
	# Write the log function; function prototype
	printf("int __%s_log __P((", funcname) >> HFILE;
	printf("DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t") >> HFILE;
	for (i = 0; i < nvars; i++) {
		printf(", ") >> HFILE;
		if (modes[i] == "DBT")
			printf("const ") >> HFILE;
		printf("%s", types[i]) >> HFILE;
		if (modes[i] == "DBT")
			printf(" *") >> HFILE;
	}
	printf("));\n") >> HFILE;

	# Function declaration
	printf("int\n__%s_log(dbenv, txnid, ret_lsnp, flags", \
	    funcname) >> CFILE;
	for (i = 0; i < nvars; i++) {
		printf(",") >> CFILE;
		if ((i % 6) == 0)
			printf("\n\t") >> CFILE;
		else
			printf(" ") >> CFILE;
		printf("%s", vars[i]) >> CFILE;
	}
	printf(")\n") >> CFILE;

	# Now print the parameters
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tDB_TXN *txnid;\n\tDB_LSN *ret_lsnp;\n") >> CFILE;
	printf("\tu_int32_t flags;\n") >> CFILE;
	for (i = 0; i < nvars; i++) {
		if (modes[i] == "DBT")
			printf("\tconst %s *%s;\n", types[i], vars[i]) >> CFILE;
		else
			printf("\t%s %s;\n", types[i], vars[i]) >> CFILE;
	}

	# Function body and local decls
	printf("{\n") >> CFILE;
	printf("\tDBT logrec;\n") >> CFILE;
	printf("\tDB_LSN *lsnp, null_lsn;\n") >> CFILE;
	if (is_dbt == 1)
		printf("\tu_int32_t zero;\n") >> CFILE;
	printf("\tu_int32_t rectype, txn_num;\n") >> CFILE;
	printf("\tint ret;\n") >> CFILE;
	printf("\tu_int8_t *bp;\n\n") >> CFILE;

	# Initialization
	printf("\trectype = DB_%s;\n", funcname) >> CFILE;
	printf("\tif (txnid != NULL &&\n") >> CFILE;
	printf("\t    TAILQ_FIRST(&txnid->kids) != NULL &&\n") >> CFILE;
	printf("\t    (ret = __txn_activekids(dbenv, rectype, txnid)) != 0)\n")\
	    >> CFILE;
	printf("\t\treturn (ret);\n") >> CFILE;
	printf("\ttxn_num = txnid == NULL ? 0 : txnid->txnid;\n") >> CFILE;
	printf("\tif (txnid == NULL) {\n") >> CFILE;
	printf("\t\tZERO_LSN(null_lsn);\n") >> CFILE;
	printf("\t\tlsnp = &null_lsn;\n") >> CFILE;
	printf("\t} else\n\t\tlsnp = &txnid->last_lsn;\n") >> CFILE;

	# Malloc
	printf("\tlogrec.size = sizeof(rectype) + ") >> CFILE;
	printf("sizeof(txn_num) + sizeof(DB_LSN)") >> CFILE;
	for (i = 0; i < nvars; i++)
		printf("\n\t    + %s", sizes[i]) >> CFILE;
	printf(";\n\tif ((ret = ") >> CFILE;
	printf(\
	    "__os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)\n")\
	    >> CFILE;
	printf("\t\treturn (ret);\n\n") >> CFILE;

	# Copy args into buffer
	printf("\tbp = logrec.data;\n") >> CFILE;
	printf("\tmemcpy(bp, &rectype, sizeof(rectype));\n") >> CFILE;
	printf("\tbp += sizeof(rectype);\n") >> CFILE;
	printf("\tmemcpy(bp, &txn_num, sizeof(txn_num));\n") >> CFILE;
	printf("\tbp += sizeof(txn_num);\n") >> CFILE;
	printf("\tmemcpy(bp, lsnp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n") >> CFILE;

	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "ARG") {
			printf("\tmemcpy(bp, &%s, %s);\n", \
			    vars[i], sizes[i]) >> CFILE;
			printf("\tbp += %s;\n", sizes[i]) >> CFILE;
		} else if (modes[i] == "DBT") {
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
			printf("\t\tbp += %s->size;\n\t}\n", vars[i]) >> CFILE;
		} else { # POINTER
			printf("\tif (%s != NULL)\n", vars[i]) >> CFILE;
			printf("\t\tmemcpy(bp, %s, %s);\n", vars[i], \
			    sizes[i]) >> CFILE;
			printf("\telse\n") >> CFILE;
			printf("\t\tmemset(bp, 0, %s);\n", sizes[i]) >> CFILE;
			printf("\tbp += %s;\n", sizes[i]) >> CFILE;
		}
	}

	# Error checking
	printf("\tDB_ASSERT((u_int32_t)") >> CFILE;
	printf("(bp - (u_int8_t *)logrec.data) == logrec.size);\n") >> CFILE;

	# Issue log call
	# The logging system cannot call the public log_put routine
	# due to mutual exclusion constraints.  So, if we are
	# generating code for the log subsystem, use the internal
	# __log_put.
	if (prefix == "log")
		printf("\tret = __log_put\(dbenv, ret_lsnp, ") >> CFILE;
	else
		printf("\tret = log_put(dbenv, ret_lsnp, ") >> CFILE;
	printf("(DBT *)&logrec, flags);\n") >> CFILE;

	# Update the transactions last_lsn
	printf("\tif (txnid != NULL)\n") >> CFILE;
	printf("\t\ttxnid->last_lsn = *ret_lsnp;\n") >> CFILE;

	# Free and return
	printf("\t__os_free(logrec.data, logrec.size);\n") >> CFILE;
	printf("\treturn (ret);\n}\n\n") >> CFILE;
}

function print_function() {
	# Write the print function; function prototype
	printf("int __%s_print", funcname) >> HFILE;
	printf(" __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));\n") \
	    >> HFILE;

	# Function declaration
	printf("int\n__%s_print(dbenv, ", funcname) >> CFILE;
	printf("dbtp, lsnp, notused2, notused3)\n") >> CFILE;
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tDBT *dbtp;\n") >> CFILE;
	printf("\tDB_LSN *lsnp;\n") >> CFILE;
	printf("\tdb_recops notused2;\n\tvoid *notused3;\n{\n") >> CFILE;

	# Locals
	printf("\t__%s_args *argp;\n", funcname) >> CFILE;
	printf("\tu_int32_t i;\n\tu_int ch;\n\tint ret;\n\n") >> CFILE;

	# Get rid of complaints about unused parameters.
	printf("\ti = 0;\n\tch = 0;\n") >> CFILE;
	printf("\tnotused2 = DB_TXN_ABORT;\n\tnotused3 = NULL;\n\n") >> CFILE;

	# Call read routine to initialize structure
	printf("\tif ((ret = __%s_read(dbenv, dbtp->data, &argp)) != 0)\n", \
	    funcname) >> CFILE;
	printf("\t\treturn (ret);\n") >> CFILE;

	# Print values in every record
	printf("\tprintf(\"[%%lu][%%lu]%s: ", funcname) >> CFILE;
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
		printf("\tprintf(\"\\t%s: ", vars[i]) >> CFILE;

		if (modes[i] == "DBT") {
			printf("\");\n") >> CFILE;
			printf("\tfor (i = 0; i < ") >> CFILE;
			printf("argp->%s.size; i++) {\n", vars[i]) >> CFILE;
			printf("\t\tch = ((u_int8_t *)argp->%s.data)[i];\n", \
			    vars[i]) >> CFILE;
			printf("\t\tif (isprint(ch) || ch == 0xa)\n") >> CFILE;
			printf("\t\t\tputchar(ch);\n") >> CFILE;
			printf("\t\telse\n") >> CFILE;
			printf("\t\t\tprintf(\"%%#x \", ch);\n") >> CFILE;
			printf("\t}\n\tprintf(\"\\n\");\n") >> CFILE;
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
	printf("\tprintf(\"\\n\");\n") >> CFILE;
	printf("\t__os_free(argp, 0);\n") >> CFILE;
	printf("\treturn (0);\n") >> CFILE;
	printf("}\n\n") >> CFILE;
}

function read_function() {
	# Write the read function; function prototype
	printf("int __%s_read __P((DB_ENV *, void *, ", funcname) >> HFILE;
	printf("__%s_args **));\n", funcname) >> HFILE;

	# Function declaration
	printf("int\n__%s_read(dbenv, recbuf, argpp)\n", funcname) >> CFILE;

	# Now print the parameters
	printf("\tDB_ENV *dbenv;\n") >> CFILE;
	printf("\tvoid *recbuf;\n") >> CFILE;
	printf("\t__%s_args **argpp;\n", funcname) >> CFILE;

	# Function body and local decls
	printf("{\n\t__%s_args *argp;\n", funcname) >> CFILE;
	printf("\tu_int8_t *bp;\n") >> CFILE;
	printf("\tint ret;\n") >> CFILE;

	printf("\n\tret = __os_malloc(dbenv, sizeof(") >> CFILE;
	printf("__%s_args) +\n\t    sizeof(DB_TXN), NULL, &argp);\n", \
	    funcname) >> CFILE;
	printf("\tif (ret != 0)\n\t\treturn (ret);\n") >> CFILE;

	# Set up the pointers to the txnid and the prev lsn
	printf("\targp->txnid = (DB_TXN *)&argp[1];\n") >> CFILE;

	# First get the record type, prev_lsn, and txnid fields.

	printf("\tbp = recbuf;\n") >> CFILE;
	printf("\tmemcpy(&argp->type, bp, sizeof(argp->type));\n") >> CFILE;
	printf("\tbp += sizeof(argp->type);\n") >> CFILE;
	printf("\tmemcpy(&argp->txnid->txnid,  bp, ") >> CFILE;
	printf("sizeof(argp->txnid->txnid));\n") >> CFILE;
	printf("\tbp += sizeof(argp->txnid->txnid);\n") >> CFILE;
	printf("\tmemcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));\n") >> CFILE;
	printf("\tbp += sizeof(DB_LSN);\n") >> CFILE;

	# Now get rest of data.
	for (i = 0; i < nvars; i ++) {
		if (modes[i] == "DBT") {
			printf("\tmemset(&argp->%s, 0, sizeof(argp->%s));\n", \
			    vars[i], vars[i]) >> CFILE;
			printf("\tmemcpy(&argp->%s.size, ", vars[i]) >> CFILE;
			printf("bp, sizeof(u_int32_t));\n") >> CFILE;
			printf("\tbp += sizeof(u_int32_t);\n") >> CFILE;
			printf("\targp->%s.data = bp;\n", vars[i]) >> CFILE;
			printf("\tbp += argp->%s.size;\n", vars[i]) >> CFILE;
		} else if (modes[i] == "ARG") {
			printf("\tmemcpy(&argp->%s, bp, %s%s));\n", \
			    vars[i], "sizeof(argp->", vars[i]) >> CFILE;
			printf("\tbp += sizeof(argp->%s);\n", vars[i]) >> CFILE;
		} else { # POINTER
			printf("\tmemcpy(&argp->%s, bp, ", vars[i]) >> CFILE;
			printf(" sizeof(argp->%s));\n", vars[i]) >> CFILE;
			printf("\tbp += sizeof(argp->%s);\n", vars[i]) >> CFILE;
		}
	}

	# Free and return
	printf("\t*argpp = argp;\n") >> CFILE;
	printf("\treturn (0);\n}\n\n") >> CFILE;
}
