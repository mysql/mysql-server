#
# $Id: gen_rpc.awk,v 11.25 2001/01/02 20:04:55 sue Exp $
# Awk script for generating client/server RPC code.
#
# This awk script generates most of the RPC routines for DB client/server
# use. It also generates a template for server and client procedures.  These
# functions must still be edited, but are highly stylized and the initial
# template gets you a fair way along the path).
#
# This awk script requires that these variables be set when it is called:
#
#	client_file	-- the C source file being created for client code
#	cproto_file	-- the header file create for client prototypes
#	ctmpl_file	-- the C template file being created for client code
#	sed_file	-- the sed file created to alter server proc code
#	server_file	-- the C source file being created for server code
#	sproto_file	-- the header file create for server prototypes
#	stmpl_file	-- the C template file being created for server code
#	xdr_file	-- the XDR message file created
#
# And stdin must be the input file that defines the RPC setup.
BEGIN {
	if (client_file == "" || cproto_file == "" || ctmpl_file == "" ||
	    sed_file == "" || server_file == "" ||
	    sproto_file == "" || stmpl_file == "" || xdr_file == "") {
		print "Usage: gen_rpc.awk requires these variables be set:"
		print "\tclient_file\t-- the client C source file being created"
		print "\tcproto_file\t-- the client prototype header created"
		print "\tctmpl_file\t-- the client template file being created"
		print "\tsed_file\t-- the sed command file being created"
		print "\tserver_file\t-- the server C source file being created"
		print "\tsproto_file\t-- the server prototype header created"
		print "\tstmpl_file\t-- the server template file being created"
		print "\txdr_file\t-- the XDR message file being created"
		error = 1; exit
	}

	FS="\t\t*"
	CFILE=client_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > CFILE

	CHFILE=cproto_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > CHFILE

	TFILE = ctmpl_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > TFILE

	SFILE = server_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > SFILE

	SHFILE=sproto_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > SHFILE

	# Server procedure template and a sed file to massage an existing
	# template source file to change args.
	# SEDFILE should be same name as PFILE but .c
	#
	PFILE = stmpl_file
	SEDFILE = sed_file
	printf("") > SEDFILE
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > PFILE

	XFILE = xdr_file
	printf("/* Do not edit: automatically built by gen_rpc.awk. */\n") \
	    > XFILE
	nendlist = 1;
}
END {
	printf("#endif /* HAVE_RPC */\n") >> CFILE
	printf("#endif /* HAVE_RPC */\n") >> TFILE
	printf("program DB_SERVERPROG {\n") >> XFILE
	printf("\tversion DB_SERVERVERS {\n") >> XFILE

	for (i = 1; i < nendlist; ++i)
		printf("\t\t%s;\n", endlist[i]) >> XFILE

	printf("\t} = 1;\n") >> XFILE
	printf("} = 351457;\n") >> XFILE
}

/^[	 ]*BEGIN/ {
	name = $2;
	msgid = $3;
	nofunc_code = 0;
	funcvars = 0;
	gen_code = 1;
	ret_code = 0;
	if ($4 == "NOCLNTCODE")
		gen_code = 0;
	if ($4 == "NOFUNC")
		nofunc_code = 1;
	if ($4 == "RETCODE")
		ret_code = 1;

	nvars = 0;
	rvars = 0;
	newvars = 0;
	db_handle = 0;
	env_handle = 0;
	dbc_handle = 0;
	txn_handle = 0;
	mp_handle = 0;
	dbt_handle = 0;
	xdr_free = 0;
}
/^[	 ]*ARG/ {
	rpc_type[nvars] = $2;
	c_type[nvars] = $3;
	pr_type[nvars] = $3;
	args[nvars] = $4;
	func_arg[nvars] = 0;
	if (rpc_type[nvars] == "LIST") {
		list_type[nvars] = $5;
	} else
		list_type[nvars] = 0;

	if (c_type[nvars] == "DBT *")
		dbt_handle = 1;

	if (c_type[nvars] == "DB_ENV *") {
		ctp_type[nvars] = "CT_ENV";
		env_handle = 1;
		env_idx = nvars;
	}

	if (c_type[nvars] == "DB *") {
		ctp_type[nvars] = "CT_DB";
		db_handle = 1;
		db_idx = nvars;
	}

	if (c_type[nvars] == "DBC *") {
		ctp_type[nvars] = "CT_CURSOR";
		dbc_handle = 1;
		dbc_idx = nvars;
	}

	if (c_type[nvars] == "DB_TXN *") {
		ctp_type[nvars] = "CT_TXN";
		txn_handle = 1;
		txn_idx = nvars;
	}

	if (c_type[nvars] == "DB_MPOOLFILE *") {
		mp_handle = 1;
		mp_idx = nvars;
	}

	++nvars;
}
/^[	 ]*FUNCPROT/ {
	pr_type[nvars] = $2;
}
/^[	 ]*FUNCARG/ {
	rpc_type[nvars] = "IGNORE";
	c_type[nvars] = $2;
	args[nvars] = sprintf("func%d", funcvars);
	func_arg[nvars] = 1;
	++funcvars;
	++nvars;
}
/^[	 ]*RET/ {
	ret_type[rvars] = $2;
	retc_type[rvars] = $3;
	retargs[rvars] = $4;
	if (ret_type[rvars] == "LIST" || ret_type[rvars] == "DBT") {
		xdr_free = 1;
	}
	if (ret_type[rvars] == "LIST") {
		retlist_type[rvars] = $5;
	} else
		retlist_type[rvars] = 0;

	++rvars;
}
/^[	 ]*END/ {
	#
	# =====================================================
	# Generate Client Nofunc code first if necessary
	# NOTE:  This code must be first, because we don't want any
	# other code other than this function, so before we write
	# out to the XDR and server files, we just generate this
	# and move on if this is all we are doing.
	#
	if (nofunc_code == 1) {
		#
		# First time through, put out the general illegal function
		#
		if (first_nofunc == 0) {
			printf("int __dbcl_rpc_illegal ") >> CHFILE
			printf("__P((DB_ENV *, char *));\n") >> CHFILE
			printf("int\n__dbcl_rpc_illegal(dbenv, name)\n") \
				>> CFILE
			printf("\tDB_ENV *dbenv;\n\tchar *name;\n") >> CFILE
			printf("{\n\t__db_err(dbenv,\n") >> CFILE
			printf("\t    \"%%s method meaningless in RPC") >> CFILE
			printf(" environment\", name);\n") >> CFILE
			printf("\treturn (__db_eopnotsup(dbenv));\n") >> CFILE
			printf("}\n\n") >> CFILE
			first_nofunc = 1
		}
		#
		# If we are doing a list, spit out prototype decl.
		#
		for (i = 0; i < nvars; i++) {
			if (rpc_type[i] != "LIST")
				continue;
			printf("static int __dbcl_%s_%slist __P((", \
			    name, args[i]) >> CFILE
			printf("__%s_%slist **, ", name, args[i]) >> CFILE
			if (list_type[i] == "STRING")
				printf("%s));\n", c_type[i]) >> CFILE
			if (list_type[i] == "INT")
				printf("u_int32_t));\n") >> CFILE
			if (list_type[i] == "ID")
				printf("%s));\n", c_type[i]) >> CFILE
			printf("static void __dbcl_%s_%sfree __P((", \
			    name, args[i]) >> CFILE
			printf("__%s_%slist **));\n", name, args[i]) >> CFILE
		}
		#
		# Spit out PUBLIC prototypes.
		#
		printf("int __dbcl_%s __P((",name) >> CHFILE
		sep = "";
		for (i = 0; i < nvars; ++i) {
			printf("%s%s", sep, pr_type[i]) >> CHFILE
			sep = ", ";
		}
		printf("));\n") >> CHFILE
		#
		# Spit out function name/args.
		#
		printf("int\n") >> CFILE
		printf("__dbcl_%s(", name) >> CFILE
		sep = "";
		for (i = 0; i < nvars; ++i) {
			printf("%s%s", sep, args[i]) >> CFILE
			sep = ", ";
		}
		printf(")\n") >> CFILE

		for (i = 0; i < nvars; ++i)
			if (func_arg[i] == 0)
				printf("\t%s %s;\n", c_type[i], args[i]) \
				    >> CFILE
			else
				printf("\t%s;\n", c_type[i]) >> CFILE

		#
		# Call error function and return EINVAL
		#
		printf("{\n") >> CFILE

		#
		# If we don't have a local env, set one.
		#
		if (env_handle == 0) {
			printf("\tDB_ENV *dbenv;\n\n") >> CFILE
			if (db_handle)
				printf("\tdbenv = %s->dbenv;\n", \
				    args[db_idx]) >> CFILE
			else if (dbc_handle)
				printf("\tdbenv = %s->dbp->dbenv;\n", \
				    args[dbc_idx]) >> CFILE
			else if (txn_handle)
				printf("\tdbenv = %s->mgrp->dbenv;\n", \
				    args[txn_idx]) >> CFILE
			else if (mp_handle)
				printf("\tdbenv = %s->dbmp->dbenv;\n", \
				    args[mp_idx]) >> CFILE
			else
				printf("\tdbenv = NULL;\n") >> CFILE
		}
		#
		# Quiet the compiler for all variables.
		#
		# NOTE:  Index 'i' starts at 1, not 0.  Our first arg is
		# the handle we need to get to the env, and we do not want
		# to COMPQUIET that one.
		for (i = 1; i < nvars; ++i) {
			if (rpc_type[i] == "CONST" || rpc_type[i] == "DBT" ||
			    rpc_type[i] == "LIST" || rpc_type[i] == "STRING") {
				printf("\tCOMPQUIET(%s, NULL);\n", args[i]) \
				    >> CFILE
			}
			if (rpc_type[i] == "INT" || rpc_type[i] == "IGNORE" ||
			    rpc_type[i] == "ID") {
				printf("\tCOMPQUIET(%s, 0);\n", args[i]) \
				    >> CFILE
			}
		}

		if (!env_handle) {
			printf("\treturn (__dbcl_rpc_illegal(dbenv, ") >> CFILE
			printf("\"%s\"));\n", name) >> CFILE
		} else
			printf("\treturn (__dbcl_rpc_illegal(%s, \"%s\"));\n", \
			    args[env_idx], name) >> CFILE
		printf("}\n\n") >> CFILE

		next;
	}

	#
	# =====================================================
	# XDR messages.
	#
	printf("\n") >> XFILE
	#
	# If there are any lists, generate the structure to contain them.
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "LIST") {
			printf("struct __%s_%slist {\n", name, args[i]) >> XFILE
			printf("\topaque ent<>;\n") >> XFILE
			printf("\t__%s_%slist *next;\n", name, args[i]) >> XFILE
			printf("};\n\n") >> XFILE
		}
	}
	printf("struct __%s_msg {\n", name) >> XFILE
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("\tunsigned int %scl_id;\n", args[i]) >> XFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("\tstring %s<>;\n", args[i]) >> XFILE
		}
		if (rpc_type[i] == "INT") {
			printf("\tunsigned int %s;\n", args[i]) >> XFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("\tunsigned int %sdlen;\n", args[i]) >> XFILE
			printf("\tunsigned int %sdoff;\n", args[i]) >> XFILE
			printf("\tunsigned int %sflags;\n", args[i]) >> XFILE
			printf("\topaque %sdata<>;\n", args[i]) >> XFILE
		}
		if (rpc_type[i] == "LIST") {
			printf("\t__%s_%slist *%slist;\n", \
			    name, args[i], args[i]) >> XFILE
		}
	}
	printf("};\n") >> XFILE

	printf("\n") >> XFILE
	#
	# If there are any lists, generate the structure to contain them.
	#
	for (i = 0; i < rvars; ++i) {
		if (ret_type[i] == "LIST") {
			printf("struct __%s_%sreplist {\n", \
			    name, retargs[i]) >> XFILE
			printf("\topaque ent<>;\n") >> XFILE
			printf("\t__%s_%sreplist *next;\n", \
			    name, retargs[i]) >> XFILE
			printf("};\n\n") >> XFILE
		}
	}
	#
	# Generate the reply message
	#
	printf("struct __%s_reply {\n", name) >> XFILE
	printf("\tunsigned int status;\n") >> XFILE
	for (i = 0; i < rvars; ++i) {
		if (ret_type[i] == "ID") {
			printf("\tunsigned int %scl_id;\n", retargs[i]) >> XFILE
		}
		if (ret_type[i] == "STRING") {
			printf("\tstring %s<>;\n", retargs[i]) >> XFILE
		}
		if (ret_type[i] == "INT") {
			printf("\tunsigned int %s;\n", retargs[i]) >> XFILE
		}
		if (ret_type[i] == "DBL") {
			printf("\tdouble %s;\n", retargs[i]) >> XFILE
		}
		if (ret_type[i] == "DBT") {
			printf("\topaque %sdata<>;\n", retargs[i]) >> XFILE
		}
		if (ret_type[i] == "LIST") {
			printf("\t__%s_%sreplist *%slist;\n", \
			    name, retargs[i], retargs[i]) >> XFILE
		}
	}
	printf("};\n") >> XFILE

	endlist[nendlist] = \
	    sprintf("__%s_reply __DB_%s(__%s_msg) = %d", \
		name, name, name, nendlist);
	nendlist++;

	#
	# =====================================================
	# File headers, if necessary.
	#
	if (first == 0) {
		printf("#include \"db_config.h\"\n") >> CFILE
		printf("\n") >> CFILE
		printf("#ifdef HAVE_RPC\n") >> CFILE
		printf("#ifndef NO_SYSTEM_INCLUDES\n") >> CFILE
		printf("#include <sys/types.h>\n") >> CFILE
		printf("#include <rpc/rpc.h>\n") >> CFILE
		printf("#include <rpc/xdr.h>\n") >> CFILE
		printf("\n") >> CFILE
		printf("#include <errno.h>\n") >> CFILE
		printf("#include <string.h>\n") >> CFILE
		printf("#endif\n") >> CFILE
		printf("#include \"db_server.h\"\n") >> CFILE
		printf("\n") >> CFILE
		printf("#include \"db_int.h\"\n") >> CFILE
		printf("#include \"db_page.h\"\n") >> CFILE
		printf("#include \"db_ext.h\"\n") >> CFILE
		printf("#include \"mp.h\"\n") >> CFILE
		printf("#include \"rpc_client_ext.h\"\n") >> CFILE
		printf("#include \"txn.h\"\n") >> CFILE
		printf("\n") >> CFILE
		n = split(CHFILE, hpieces, "/");
		printf("#include \"%s\"\n", hpieces[n]) >> CFILE
		printf("\n") >> CFILE

		printf("#include \"db_config.h\"\n") >> TFILE
		printf("\n") >> TFILE
		printf("#ifdef HAVE_RPC\n") >> TFILE
		printf("#ifndef NO_SYSTEM_INCLUDES\n") >> TFILE
		printf("#include <sys/types.h>\n") >> TFILE
		printf("#include <rpc/rpc.h>\n") >> TFILE
		printf("\n") >> TFILE
		printf("#include <errno.h>\n") >> TFILE
		printf("#include <string.h>\n") >> TFILE
		printf("#endif\n") >> TFILE
		printf("#include \"db_server.h\"\n") >> TFILE
		printf("\n") >> TFILE
		printf("#include \"db_int.h\"\n") >> TFILE
		printf("#include \"db_page.h\"\n") >> TFILE
		printf("#include \"db_ext.h\"\n") >> TFILE
		printf("#include \"txn.h\"\n") >> TFILE
		printf("\n") >> TFILE
		n = split(CHFILE, hpieces, "/");
		printf("#include \"%s\"\n", hpieces[n]) >> TFILE
		printf("\n") >> TFILE

		printf("#include \"db_config.h\"\n") >> SFILE
		printf("\n") >> SFILE
		printf("#ifndef NO_SYSTEM_INCLUDES\n") >> SFILE
		printf("#include <sys/types.h>\n") >> SFILE
		printf("\n") >> SFILE
		printf("#include <rpc/rpc.h>\n") >> SFILE
		printf("#include <rpc/xdr.h>\n") >> SFILE
		printf("\n") >> SFILE
		printf("#include <errno.h>\n") >> SFILE
		printf("#include <string.h>\n") >> SFILE
		printf("#endif\n") >> SFILE
		printf("#include \"db_server.h\"\n") >> SFILE
		printf("\n") >> SFILE
		printf("#include \"db_int.h\"\n") >> SFILE
		printf("#include \"db_server_int.h\"\n") >> SFILE
		printf("#include \"rpc_server_ext.h\"\n") >> SFILE
		printf("\n") >> SFILE
		n = split(SHFILE, hpieces, "/");
		printf("#include \"%s\"\n", hpieces[n]) >> SFILE
		printf("\n") >> SFILE

		printf("#include \"db_config.h\"\n") >> PFILE
		printf("\n") >> PFILE
		printf("#ifndef NO_SYSTEM_INCLUDES\n") >> PFILE
		printf("#include <sys/types.h>\n") >> PFILE
		printf("\n") >> PFILE
		printf("#include <rpc/rpc.h>\n") >> PFILE
		printf("\n") >> PFILE
		printf("#include <errno.h>\n") >> PFILE
		printf("#include <string.h>\n") >> PFILE
		printf("#include \"db_server.h\"\n") >> PFILE
		printf("#endif\n") >> PFILE
		printf("\n") >> PFILE
		printf("#include \"db_int.h\"\n") >> PFILE
		printf("#include \"db_server_int.h\"\n") >> PFILE
		printf("#include \"rpc_server_ext.h\"\n") >> PFILE
		printf("\n") >> PFILE
		n = split(SHFILE, hpieces, "/");
		printf("#include \"%s\"\n", hpieces[n]) >> PFILE
		printf("\n") >> PFILE

		first = 1;
	}

	#
	# =====================================================
	# Server functions.
	#
	# If we are doing a list, send out local list prototypes.
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] != "LIST")
			continue;
		if (list_type[i] != "STRING" && list_type[i] != "INT" &&
		    list_type[i] != "ID")
			continue;
		printf("int __db_%s_%slist __P((", name, args[i]) >> SFILE
		printf("__%s_%slist *, ", name, args[i]) >> SFILE
		if (list_type[i] == "STRING") {
			printf("char ***));\n") >> SFILE
		}
		if (list_type[i] == "INT" || list_type[i] == "ID") {
			printf("u_int32_t **));\n") >> SFILE
		}
		printf("void __db_%s_%sfree __P((", name, args[i]) >> SFILE
		if (list_type[i] == "STRING")
			printf("char **));\n\n") >> SFILE
		if (list_type[i] == "INT" || list_type[i] == "ID")
			printf("u_int32_t *));\n\n") >> SFILE

	}
	#
	# First spit out PUBLIC prototypes for server functions.
	#
	printf("__%s_reply * __db_%s_%d __P((__%s_msg *));\n", \
	    name, name, msgid, name) >> SHFILE

	printf("__%s_reply *\n", name) >> SFILE
	printf("__db_%s_%d(req)\n", name, msgid) >> SFILE
	printf("\t__%s_msg *req;\n", name) >> SFILE;
	printf("{\n") >> SFILE
	doing_list = 0;
	#
	# If we are doing a list, decompose it for server proc we'll call.
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] != "LIST")
			continue;
		doing_list = 1;
		if (list_type[i] == "STRING")
			printf("\tchar **__db_%slist;\n", args[i]) >> SFILE
		if (list_type[i] == "ID" || list_type[i] == "INT")
			printf("\tu_int32_t *__db_%slist;\n", args[i]) >> SFILE
	}
	if (doing_list)
		printf("\tint ret;\n") >> SFILE
	printf("\tstatic __%s_reply reply; /* must be static */\n", \
	    name) >> SFILE
	if (xdr_free) {
		printf("\tstatic int __%s_free = 0; /* must be static */\n\n", \
		    name) >> SFILE
		printf("\tif (__%s_free)\n", name) >> SFILE
		printf("\t\txdr_free((xdrproc_t)xdr___%s_reply, (void *)&reply);\n", \
		    name) >> SFILE
		printf("\t__%s_free = 0;\n", name) >> SFILE
		printf("\n\t/* Reinitialize allocated fields */\n") >> SFILE
		for (i = 0; i < rvars; ++i) {
			if (ret_type[i] == "LIST") {
				printf("\treply.%slist = NULL;\n", \
				    retargs[i]) >> SFILE
			}
			if (ret_type[i] == "DBT") {
				printf("\treply.%sdata.%sdata_val = NULL;\n", \
				    retargs[i], retargs[i]) >> SFILE
			}
		}
	}

	need_out = 0;
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "LIST") {
			printf("\n\tif ((ret = __db_%s_%slist(", \
			    name, args[i]) >> SFILE
			printf("req->%slist, &__db_%slist)) != 0)\n", \
			    args[i], args[i]) >> SFILE
			printf("\t\tgoto out;\n") >> SFILE
			need_out = 1;
		}
	}

	#
	# Compose server proc to call.  Decompose message components as args.
	#
	printf("\n\t__%s_%d_proc(", name, msgid) >> SFILE
	sep = "";
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("%sreq->%scl_id", sep, args[i]) >> SFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("%s(*req->%s == '\\0') ? NULL : req->%s", \
			    sep, args[i], args[i]) >> SFILE
		}
		if (rpc_type[i] == "INT") {
			printf("%sreq->%s", sep, args[i]) >> SFILE
		}
		if (rpc_type[i] == "LIST") {
			printf("%s__db_%slist", sep, args[i]) >> SFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("%sreq->%sdlen", sep, args[i]) >> SFILE
			sep = ",\n\t    ";
			printf("%sreq->%sdoff", sep, args[i]) >> SFILE
			printf("%sreq->%sflags", sep, args[i]) >> SFILE
			printf("%sreq->%sdata.%sdata_val", \
			    sep, args[i], args[i]) >> SFILE
			printf("%sreq->%sdata.%sdata_len", \
			    sep, args[i], args[i]) >> SFILE
		}
		sep = ",\n\t    ";
	}
	printf("%s&reply", sep) >> SFILE
	if (xdr_free)
		printf("%s&__%s_free);\n", sep, name) >> SFILE
	else
		printf(");\n\n") >> SFILE
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "LIST") {
			printf("\t__db_%s_%sfree(__db_%slist);\n", \
			    name, args[i], args[i]) >> SFILE
		}
	}
	if (need_out) {
		printf("\nout:\n") >> SFILE
	}
	printf("\treturn (&reply);\n") >> SFILE
	printf("}\n\n") >> SFILE

	#
	# If we are doing a list, write list functions for this op.
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] != "LIST")
			continue;
		if (list_type[i] != "STRING" && list_type[i] != "INT" &&
		    list_type[i] != "ID")
			continue;
		printf("int\n") >> SFILE
		printf("__db_%s_%slist(locp, ppp)\n", name, args[i]) >> SFILE
		printf("\t__%s_%slist *locp;\n", name, args[i]) >> SFILE
		if (list_type[i] == "STRING") {
			printf("\tchar ***ppp;\n{\n") >> SFILE
			printf("\tchar **pp;\n") >> SFILE
		}
		if (list_type[i] == "INT" || list_type[i] == "ID") {
			printf("\tu_int32_t **ppp;\n{\n") >> SFILE
			printf("\tu_int32_t *pp;\n") >> SFILE
		}
		printf("\tint cnt, ret, size;\n") >> SFILE
		printf("\t__%s_%slist *nl;\n\n", name, args[i]) >> SFILE
		printf("\tfor (cnt = 0, nl = locp;") >> SFILE
		printf(" nl != NULL; cnt++, nl = nl->next)\n\t\t;\n\n") >> SFILE
		printf("\tif (cnt == 0) {\n") >> SFILE
		printf("\t\t*ppp = NULL;\n") >> SFILE
		printf("\t\treturn (0);\n\t}\n") >> SFILE
		printf("\tsize = sizeof(*pp) * (cnt + 1);\n") >> SFILE
		printf("\tif ((ret = __os_malloc(NULL, size, ") >> SFILE
		printf("NULL, ppp)) != 0)\n") >> SFILE
		printf("\t\treturn (ret);\n") >> SFILE
		printf("\tmemset(*ppp, 0, size);\n") >> SFILE
		printf("\tfor (pp = *ppp, nl = locp;") >> SFILE
		printf(" nl != NULL; nl = nl->next, pp++) {\n") >> SFILE
		if (list_type[i] == "STRING") {
			printf("\t\tif ((ret = __os_malloc(NULL ,") >> SFILE
			printf("nl->ent.ent_len + 1, NULL, pp)) != 0)\n") \
			    >> SFILE
			printf("\t\t\tgoto out;\n") >> SFILE
			printf("\t\tif ((ret = __os_strdup(NULL, ") >> SFILE
			printf("(char *)nl->ent.ent_val, pp)) != 0)\n") >> SFILE
			printf("\t\t\tgoto out;\n") >> SFILE
		}
		if (list_type[i] == "INT" || list_type[i] == "ID")
			printf("\t\t*pp = *(u_int32_t *)nl->ent.ent_val;\n") \
			    >> SFILE
		printf("\t}\n") >> SFILE
		printf("\treturn (0);\n") >> SFILE
		if (list_type[i] == "STRING") {
			printf("out:\n") >> SFILE
			printf("\t__db_%s_%sfree(*ppp);\n", \
			    name, args[i]) >> SFILE
			printf("\treturn (ret);\n") >> SFILE
		}
		printf("}\n\n") >> SFILE

		printf("void\n") >> SFILE
		printf("__db_%s_%sfree(pp)\n", name, args[i]) >> SFILE

		if (list_type[i] == "STRING")
			printf("\tchar **pp;\n") >> SFILE
		if (list_type[i] == "INT" || list_type[i] == "ID")
			printf("\tu_int32_t *pp;\n") >> SFILE

		printf("{\n") >> SFILE
		printf("\tsize_t size;\n") >> SFILE

		if (list_type[i] == "STRING")
			printf("\tchar **p;\n\n") >> SFILE
		if (list_type[i] == "INT" || list_type[i] == "ID")
			printf("\tu_int32_t *p;\n\n") >> SFILE

		printf("\tif (pp == NULL)\n\t\treturn;\n") >> SFILE
		printf("\tsize = sizeof(*p);\n") >> SFILE
		printf("\tfor (p = pp; *p != 0; p++) {\n") >> SFILE
		printf("\t\tsize += sizeof(*p);\n") >> SFILE

		if (list_type[i] == "STRING")
			printf("\t\t__os_free(*p, strlen(*p)+1);\n") >> SFILE
		printf("\t}\n") >> SFILE
		printf("\t__os_free(pp, size);\n") >> SFILE
		printf("}\n\n") >> SFILE
	}

	#
	# =====================================================
	# Generate Procedure Template Server code
	#
	# Produce SED file commands if needed at the same time
	#
	# Start with PUBLIC prototypes
	#
	printf("void __%s_%d_proc __P((", name, msgid) >> SHFILE
	sep = "";
	argcount = 0;
	for (i = 0; i < nvars; ++i) {
		argcount++;
		split_lines(1);
		if (argcount == 0) {
			sep = "";
		}
		if (rpc_type[i] == "IGNORE")
			continue;
		if (rpc_type[i] == "ID") {
			printf("%slong", sep) >> SHFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("%schar *", sep) >> SHFILE
		}
		if (rpc_type[i] == "INT") {
			printf("%su_int32_t", sep) >> SHFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "STRING") {
			printf("%schar **", sep) >> SHFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "INT") {
			printf("%su_int32_t *", sep) >> SHFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "ID") {
			printf("%su_int32_t *", sep) >> SHFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("%su_int32_t", sep) >> SHFILE
			sep = ", ";
			argcount++;
			split_lines(1);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%su_int32_t", sep) >> SHFILE
			argcount++;
			split_lines(1);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%su_int32_t", sep) >> SHFILE
			argcount++;
			split_lines(1);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%svoid *", sep) >> SHFILE
			argcount++;
			split_lines(1);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%su_int32_t", sep) >> SHFILE
		}
		sep = ", ";
	}
	printf("%s__%s_reply *", sep, name) >> SHFILE
	if (xdr_free) {
		printf("%sint *));\n", sep) >> SHFILE
	} else {
		printf("));\n") >> SHFILE
	}
	#
	# Spit out function name and arg list
	#
	printf("/^\\/\\* BEGIN __%s_%d_proc/,/^\\/\\* END __%s_%d_proc/c\\\n", \
	    name, msgid, name, msgid) >> SEDFILE

	printf("/* BEGIN __%s_%d_proc */\n", name, msgid) >> PFILE
	printf("/* BEGIN __%s_%d_proc */\\\n", name, msgid) >> SEDFILE
	printf("void\n") >> PFILE
	printf("void\\\n") >> SEDFILE
	printf("__%s_%d_proc(", name, msgid) >> PFILE
	printf("__%s_%d_proc(", name, msgid) >> SEDFILE
	sep = "";
	argcount = 0;
	for (i = 0; i < nvars; ++i) {
		argcount++;
		split_lines(0);
		if (argcount == 0) {
			sep = "";
		}
		if (rpc_type[i] == "IGNORE") 
			continue;
		if (rpc_type[i] == "ID") {
			printf("%s%scl_id", sep, args[i]) >> PFILE
			printf("%s%scl_id", sep, args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("%s%s", sep, args[i]) >> PFILE
			printf("%s%s", sep, args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "INT") {
			printf("%s%s", sep, args[i]) >> PFILE
			printf("%s%s", sep, args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "LIST") {
			printf("%s%slist", sep, args[i]) >> PFILE
			printf("%s%slist", sep, args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("%s%sdlen", sep, args[i]) >> PFILE
			printf("%s%sdlen", sep, args[i]) >> SEDFILE
			sep = ", ";
			argcount++;
			split_lines(0);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%s%sdoff", sep, args[i]) >> PFILE
			printf("%s%sdoff", sep, args[i]) >> SEDFILE
			argcount++;
			split_lines(0);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%s%sflags", sep, args[i]) >> PFILE
			printf("%s%sflags", sep, args[i]) >> SEDFILE
			argcount++;
			split_lines(0);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%s%sdata", sep, args[i]) >> PFILE
			printf("%s%sdata", sep, args[i]) >> SEDFILE
			argcount++;
			split_lines(0);
			if (argcount == 0) {
				sep = "";
			} else {
				sep = ", ";
			}
			printf("%s%ssize", sep, args[i]) >> PFILE
			printf("%s%ssize", sep, args[i]) >> SEDFILE
		}
		sep = ", ";
	}
	printf("%sreplyp",sep) >> PFILE
	printf("%sreplyp",sep) >> SEDFILE
	if (xdr_free) {
		printf("%sfreep)\n",sep) >> PFILE
		printf("%sfreep)\\\n",sep) >> SEDFILE
	} else {
		printf(")\n") >> PFILE
		printf(")\\\n") >> SEDFILE
	}
	#
	# Spit out arg types/names;
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("\tlong %scl_id;\n", args[i]) >> PFILE
			printf("\\\tlong %scl_id;\\\n", args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("\tchar *%s;\n", args[i]) >> PFILE
			printf("\\\tchar *%s;\\\n", args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "INT") {
			printf("\tu_int32_t %s;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t %s;\\\n", args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "STRING") {
			printf("\tchar ** %slist;\n", args[i]) >> PFILE
			printf("\\\tchar ** %slist;\\\n", args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "INT") {
			printf("\tu_int32_t * %slist;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t * %slist;\\\n", \
			    args[i]) >> SEDFILE
		}
		if (rpc_type[i] == "LIST" && list_type[i] == "ID") {
			printf("\tu_int32_t * %slist;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t * %slist;\\\n", args[i]) \
			    >> SEDFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("\tu_int32_t %sdlen;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t %sdlen;\\\n", args[i]) >> SEDFILE
			printf("\tu_int32_t %sdoff;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t %sdoff;\\\n", args[i]) >> SEDFILE
			printf("\tu_int32_t %sflags;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t %sflags;\\\n", args[i]) >> SEDFILE
			printf("\tvoid *%sdata;\n", args[i]) >> PFILE
			printf("\\\tvoid *%sdata;\\\n", args[i]) >> SEDFILE
			printf("\tu_int32_t %ssize;\n", args[i]) >> PFILE
			printf("\\\tu_int32_t %ssize;\\\n", args[i]) >> SEDFILE
		}
	}
	printf("\t__%s_reply *replyp;\n",name) >> PFILE
	printf("\\\t__%s_reply *replyp;\\\n",name) >> SEDFILE
	if (xdr_free) {
		printf("\tint * freep;\n") >> PFILE
		printf("\\\tint * freep;\\\n") >> SEDFILE
	}

	printf("/* END __%s_%d_proc */\n", name, msgid) >> PFILE
	printf("/* END __%s_%d_proc */\n", name, msgid) >> SEDFILE

	#
	# Function body
	#
	printf("{\n") >> PFILE
	printf("\tint ret;\n") >> PFILE
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("\t%s %s;\n", c_type[i], args[i]) >> PFILE
			printf("\tct_entry *%s_ctp;\n", args[i]) >> PFILE
		}
	}
	printf("\n") >> PFILE
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("\tACTIVATE_CTP(%s_ctp, %scl_id, %s);\n", \
			    args[i], args[i], ctp_type[i]) >> PFILE
			printf("\t%s = (%s)%s_ctp->ct_anyp;\n", \
			    args[i], c_type[i], args[i]) >> PFILE
		}
	}
	printf("\n\t/*\n\t * XXX Code goes here\n\t */\n\n") >> PFILE
	printf("\treplyp->status = ret;\n") >> PFILE
	printf("\treturn;\n") >> PFILE
	printf("}\n\n") >> PFILE

	#
	# If we don't want client code generated, go on to next.
	#
	if (gen_code == 0)
		next;

	#
	# =====================================================
	# Generate Client code
	#
	# If we are doing a list, spit out prototype decl.
	#
	for (i = 0; i < nvars; i++) {
		if (rpc_type[i] != "LIST")
			continue;
		printf("static int __dbcl_%s_%slist __P((", \
		    name, args[i]) >> CFILE
		printf("__%s_%slist **, ", name, args[i]) >> CFILE
		if (list_type[i] == "STRING")
			printf("%s));\n", c_type[i]) >> CFILE
		if (list_type[i] == "INT")
			printf("u_int32_t));\n") >> CFILE
		if (list_type[i] == "ID")
			printf("%s));\n", c_type[i]) >> CFILE
		printf("static void __dbcl_%s_%sfree __P((", \
		    name, args[i]) >> CFILE
		printf("__%s_%slist **));\n", name, args[i]) >> CFILE
	}
	#
	# Spit out PUBLIC prototypes.
	#
	printf("int __dbcl_%s __P((",name) >> CHFILE
	sep = "";
	for (i = 0; i < nvars; ++i) {
		printf("%s%s", sep, pr_type[i]) >> CHFILE
		sep = ", ";
	}
	printf("));\n") >> CHFILE
	#
	# Spit out function name/args.
	#
	printf("int\n") >> CFILE
	printf("__dbcl_%s(", name) >> CFILE
	sep = "";
	for (i = 0; i < nvars; ++i) {
		printf("%s%s", sep, args[i]) >> CFILE
		sep = ", ";
	}
	printf(")\n") >> CFILE

	for (i = 0; i < nvars; ++i)
		if (func_arg[i] == 0)
			printf("\t%s %s;\n", c_type[i], args[i]) >> CFILE
		else
			printf("\t%s;\n", c_type[i]) >> CFILE

	printf("{\n") >> CFILE
	printf("\tCLIENT *cl;\n") >> CFILE
	printf("\t__%s_msg req;\n", name) >> CFILE
	printf("\tstatic __%s_reply *replyp = NULL;\n", name) >> CFILE;
	printf("\tint ret;\n") >> CFILE
	if (!env_handle)
		printf("\tDB_ENV *dbenv;\n") >> CFILE

	printf("\n") >> CFILE
	printf("\tret = 0;\n") >> CFILE
	if (!env_handle) {
		printf("\tdbenv = NULL;\n") >> CFILE
		if (db_handle)
			printf("\tdbenv = %s->dbenv;\n", args[db_idx]) >> CFILE
		else if (dbc_handle)
			printf("\tdbenv = %s->dbp->dbenv;\n", \
			    args[dbc_idx]) >> CFILE
		else if (txn_handle)
			printf("\tdbenv = %s->mgrp->dbenv;\n", \
			    args[txn_idx]) >> CFILE
		printf("\tif (dbenv == NULL || dbenv->cl_handle == NULL) {\n") \
		    >> CFILE
		printf("\t\t__db_err(dbenv, \"No server environment.\");\n") \
		    >> CFILE
	} else {
		printf("\tif (%s == NULL || %s->cl_handle == NULL) {\n", \
		    args[env_idx], args[env_idx]) >> CFILE
		printf("\t\t__db_err(%s, \"No server environment.\");\n", \
		    args[env_idx]) >> CFILE
	}
	printf("\t\treturn (DB_NOSERVER);\n") >> CFILE
	printf("\t}\n") >> CFILE
	printf("\n") >> CFILE

	#
	# Free old reply if there was one.
	#
	printf("\tif (replyp != NULL) {\n") >> CFILE
	printf("\t\txdr_free((xdrproc_t)xdr___%s_reply, (void *)replyp);\n", \
	    name) >> CFILE
	printf("\t\treplyp = NULL;\n\t}\n") >> CFILE
	if (!env_handle)
		printf("\tcl = (CLIENT *)dbenv->cl_handle;\n") >> CFILE
	else
		printf("\tcl = (CLIENT *)%s->cl_handle;\n", \
		    args[env_idx]) >> CFILE

	printf("\n") >> CFILE

	#
	# If there is a function arg, check that it is NULL
	#
	for (i = 0; i < nvars; ++i) {
		if (func_arg[i] != 1)
			continue;
		printf("\tif (%s != NULL) {\n", args[i]) >> CFILE
		printf("\t\t__db_err(%s, ", args[env_idx]) >> CFILE
		printf("\"User functions not supported in RPC.\");\n") >> CFILE
		printf("\t\treturn (EINVAL);\n\t}\n") >> CFILE
	}

	#
	# Compose message components
	#
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "ID") {
			printf("\tif (%s == NULL)\n", args[i]) >> CFILE
			printf("\t\treq.%scl_id = 0;\n\telse\n", \
			    args[i]) >> CFILE
			if (c_type[i] == "DB_TXN *") {
				printf("\t\treq.%scl_id = %s->txnid;\n", \
				    args[i], args[i]) >> CFILE
			} else {
				printf("\t\treq.%scl_id = %s->cl_id;\n", \
				    args[i], args[i]) >> CFILE
			}
		}
		if (rpc_type[i] == "INT") {
			printf("\treq.%s = %s;\n", args[i], args[i]) >> CFILE
		}
		if (rpc_type[i] == "STRING") {
			printf("\tif (%s == NULL)\n", args[i]) >> CFILE
			printf("\t\treq.%s = \"\";\n", args[i]) >> CFILE
			printf("\telse\n") >> CFILE
			printf("\t\treq.%s = (char *)%s;\n", \
			    args[i], args[i]) >> CFILE
		}
		if (rpc_type[i] == "DBT") {
			printf("\treq.%sdlen = %s->dlen;\n", \
			    args[i], args[i]) >> CFILE
			printf("\treq.%sdoff = %s->doff;\n", \
			    args[i], args[i]) >> CFILE
			printf("\treq.%sflags = %s->flags;\n", \
			    args[i], args[i]) >> CFILE
			printf("\treq.%sdata.%sdata_val = %s->data;\n", \
			    args[i], args[i], args[i]) >> CFILE
			printf("\treq.%sdata.%sdata_len = %s->size;\n", \
			    args[i], args[i], args[i]) >> CFILE
		}
		if (rpc_type[i] == "LIST") {
			printf("\tif ((ret = __dbcl_%s_%slist(", \
			    name, args[i]) >> CFILE
			printf("&req.%slist, %s)) != 0)\n", \
			    args[i], args[i]) >> CFILE
			printf("\t\tgoto out;\n") >> CFILE
		}
	}

	printf("\n") >> CFILE
	printf("\treplyp = __db_%s_%d(&req, cl);\n", name, msgid) >> CFILE
	printf("\tif (replyp == NULL) {\n") >> CFILE
	if (!env_handle) {
		printf("\t\t__db_err(dbenv, ") >> CFILE
		printf("clnt_sperror(cl, \"Berkeley DB\"));\n") >> CFILE
	} else {
		printf("\t\t__db_err(%s, ", args[env_idx]) >> CFILE
		printf("clnt_sperror(cl, \"Berkeley DB\"));\n") >> CFILE
	}
	printf("\t\tret = DB_NOSERVER;\n") >> CFILE
	printf("\t\tgoto out;\n") >> CFILE
	printf("\t}\n") >> CFILE

	if (ret_code == 0) {
		printf("\tret = replyp->status;\n") >> CFILE
	} else {
		for (i = 0; i < nvars; ++i) {
			if (rpc_type[i] == "LIST") {
				printf("\t__dbcl_%s_%sfree(&req.%slist);\n", \
				    name, args[i], args[i]) >> CFILE
			}
		}
		printf("\treturn (__dbcl_%s_ret(", name) >> CFILE
		sep = "";
		for (i = 0; i < nvars; ++i) {
			printf("%s%s", sep, args[i]) >> CFILE
			sep = ", ";
		}
		printf("%sreplyp));\n", sep) >> CFILE
	}
	printf("out:\n") >> CFILE
	for (i = 0; i < nvars; ++i) {
		if (rpc_type[i] == "LIST") {
			printf("\t__dbcl_%s_%sfree(&req.%slist);\n", \
			    name, args[i], args[i]) >> CFILE
		}
	}
	printf("\treturn (ret);\n") >> CFILE
	printf("}\n\n") >> CFILE

	#
	# If we are doing a list, write list functions for op.
	#
	for (i = 0; i < nvars; i++) {
		if (rpc_type[i] != "LIST")
			continue;
		printf("int\n__dbcl_%s_%slist(locp, pp)\n", \
		    name, args[i]) >> CFILE
		printf("\t__%s_%slist **locp;\n", name, args[i]) >> CFILE
		if (list_type[i] == "STRING")
			printf("\t%s pp;\n{\n\t%s p;\n", \
			    c_type[i], c_type[i]) >> CFILE
		if (list_type[i] == "INT")
			printf("\tu_int32_t *pp;\n{\n\tu_int32_t *p, *q;\n") \
			    >> CFILE
		if (list_type[i] == "ID")
			printf("\t%s pp;\n{\n\t%s p;\n\tu_int32_t *q;\n", \
			    c_type[i], c_type[i]) >> CFILE

		printf("\tint ret;\n") >> CFILE
		printf("\t__%s_%slist *nl, **nlp;\n\n", name, args[i]) >> CFILE
		printf("\t*locp = NULL;\n") >> CFILE
		printf("\tif (pp == NULL)\n\t\treturn (0);\n") >> CFILE
		printf("\tnlp = locp;\n") >> CFILE
		printf("\tfor (p = pp; *p != 0; p++) {\n") >> CFILE
		printf("\t\tif ((ret = __os_malloc(NULL, ") >> CFILE
		printf("sizeof(*nl), NULL, nlp)) != 0)\n") >> CFILE
		printf("\t\t\tgoto out;\n") >> CFILE
		printf("\t\tnl = *nlp;\n") >> CFILE
		printf("\t\tnl->next = NULL;\n") >> CFILE
		printf("\t\tnl->ent.ent_val = NULL;\n") >> CFILE
		printf("\t\tnl->ent.ent_len = 0;\n") >> CFILE
		if (list_type[i] == "STRING") {
			printf("\t\tif ((ret = __os_strdup(NULL, ") >> CFILE
			printf("*p, &nl->ent.ent_val)) != 0)\n") >> CFILE
			printf("\t\t\tgoto out;\n") >> CFILE
			printf("\t\tnl->ent.ent_len = strlen(*p)+1;\n") >> CFILE
		}
		if (list_type[i] == "INT") {
			printf("\t\tif ((ret = __os_malloc(NULL, ") >> CFILE
			printf("sizeof(%s), NULL, &nl->ent.ent_val)) != 0)\n", \
			    c_type[i]) >> CFILE
			printf("\t\t\tgoto out;\n") >> CFILE
			printf("\t\tq = (u_int32_t *)nl->ent.ent_val;\n") \
			    >> CFILE
			printf("\t\t*q = *p;\n") >> CFILE
			printf("\t\tnl->ent.ent_len = sizeof(%s);\n", \
			    c_type[i]) >> CFILE
		}
		if (list_type[i] == "ID") {
			printf("\t\tif ((ret = __os_malloc(NULL, ") >> CFILE
			printf("sizeof(u_int32_t),") >> CFILE
			printf(" NULL, &nl->ent.ent_val)) != 0)\n") >> CFILE
			printf("\t\t\tgoto out;\n") >> CFILE
			printf("\t\tq = (u_int32_t *)nl->ent.ent_val;\n") \
			    >> CFILE
			printf("\t\t*q = (*p)->cl_id;\n") >> CFILE
			printf("\t\tnl->ent.ent_len = sizeof(u_int32_t);\n") \
			    >> CFILE
		}
		printf("\t\tnlp = &nl->next;\n") >> CFILE
		printf("\t}\n") >> CFILE
		printf("\treturn (0);\n") >> CFILE
		printf("out:\n") >> CFILE
		printf("\t__dbcl_%s_%sfree(locp);\n", name, args[i]) >> CFILE
		printf("\treturn (ret);\n") >> CFILE

		printf("}\n\n") >> CFILE

		printf("void\n__dbcl_%s_%sfree(locp)\n", name, args[i]) >> CFILE
		printf("\t__%s_%slist **locp;\n", name, args[i]) >> CFILE
		printf("{\n") >> CFILE
		printf("\t__%s_%slist *nl, *nl1;\n\n", name, args[i]) >> CFILE
		printf("\tif (locp == NULL)\n\t\treturn;\n") >> CFILE
		printf("\tfor (nl = *locp; nl != NULL; nl = nl1) {\n") >> CFILE
		printf("\t\tnl1 = nl->next;\n") >> CFILE
		printf("\t\tif (nl->ent.ent_val)\n") >> CFILE
		printf("\t\t\t__os_free(nl->ent.ent_val, nl->ent.ent_len);\n") \
		    >> CFILE
		printf("\t\t__os_free(nl, sizeof(*nl));\n") >> CFILE
		printf("\t}\n}\n\n") >> CFILE
	}
	#
	# Generate Client Template code
	#
	if (ret_code) {
		#
		# If we are doing a list, write prototypes
		#
		for (i = 0; i < rvars; ++i) {
			if (ret_type[i] != "LIST")
				continue;
			if (retlist_type[i] != "STRING" &&
			    retlist_type[i] != "INT" && list_type[i] != "ID")
				continue;
			printf("int __db_%s_%sreplist __P((", \
			    name, retargs[i]) >> TFILE
			printf("__%s_%sreplist, ", \
			    name, retargs[i]) >> TFILE
			if (retlist_type[i] == "STRING") {
				printf("char ***));\n") >> TFILE
			}
			if (retlist_type[i] == "INT" ||
			    retlist_type[i] == "ID") {
				printf("u_int32_t **));\n") >> TFILE
			}
			printf("void __db_%s_%sfree __P((", \
			    name, retargs[i]) >> TFILE
			if (retlist_type[i] == "STRING")
				printf("char **));\n") >> TFILE
			if (retlist_type[i] == "INT" || retlist_type[i] == "ID")
				printf("u_int32_t *));\n\n") >> TFILE
		}

		printf("int __dbcl_%s_ret __P((", name) >> CHFILE
		sep = "";
		for (i = 0; i < nvars; ++i) {
			printf("%s%s", sep, pr_type[i]) >> CHFILE
			sep = ", ";
		}
		printf("%s__%s_reply *));\n", sep, name) >> CHFILE

		printf("int\n") >> TFILE
		printf("__dbcl_%s_ret(", name) >> TFILE
		sep = "";
		for (i = 0; i < nvars; ++i) {
			printf("%s%s", sep, args[i]) >> TFILE
			sep = ", ";
		}
		printf("%sreplyp)\n",sep) >> TFILE

		for (i = 0; i < nvars; ++i)
			if (func_arg[i] == 0)
				printf("\t%s %s;\n", c_type[i], args[i]) \
				    >> TFILE
			else
				printf("\t%s;\n", c_type[i]) >> TFILE
		printf("\t__%s_reply *replyp;\n", name) >> TFILE;
		printf("{\n") >> TFILE
		printf("\tint ret;\n") >> TFILE
		#
		# Local vars in template
		#
		for (i = 0; i < rvars; ++i) {
			if (ret_type[i] == "ID" || ret_type[i] == "STRING" ||
			    ret_type[i] == "INT" || ret_type[i] == "DBL") {
				printf("\t%s %s;\n", \
				    retc_type[i], retargs[i]) >> TFILE
			} else if (ret_type[i] == "LIST") {
				if (retlist_type[i] == "STRING")
					printf("\tchar **__db_%slist;\n", \
					    retargs[i]) >> TFILE
				if (retlist_type[i] == "ID" ||
				    retlist_type[i] == "INT")
					printf("\tu_int32_t *__db_%slist;\n", \
					    retargs[i]) >> TFILE
			} else {
				printf("\t/* %s %s; */\n", \
				    ret_type[i], retargs[i]) >> TFILE
			}
		}
		#
		# Client return code
		#
		printf("\n") >> TFILE
		printf("\tif (replyp->status != 0)\n") >> TFILE
		printf("\t\treturn (replyp->status);\n") >> TFILE
		for (i = 0; i < rvars; ++i) {
			varname = "";
			if (ret_type[i] == "ID") {
				varname = sprintf("%scl_id", retargs[i]);
			}
			if (ret_type[i] == "STRING") {
				varname =  retargs[i];
			}
			if (ret_type[i] == "INT" || ret_type[i] == "DBL") {
				varname =  retargs[i];
			}
			if (ret_type[i] == "DBT") {
				varname = sprintf("%sdata", retargs[i]);
			}
			if (ret_type[i] == "ID" || ret_type[i] == "STRING" ||
			    ret_type[i] == "INT" || ret_type[i] == "DBL") {
				printf("\t%s = replyp->%s;\n", \
				    retargs[i], varname) >> TFILE
			} else if (ret_type[i] == "LIST") {
				printf("\n\tif ((ret = __db_%s_%slist(", \
				    name, retargs[i]) >> TFILE
				printf("replyp->%slist, &__db_%slist)) != 0)", \
				    retargs[i], retargs[i]) >> TFILE
				printf("\n\t\treturn (ret);\n") >> TFILE
				printf("\n\t/*\n") >> TFILE
				printf("\t * XXX Handle list\n") >> TFILE
				printf("\t */\n\n") >> TFILE
				printf("\t__db_%s_%sfree(__db_%slist);\n", \
				    name, retargs[i], retargs[i]) >> TFILE
			} else {
				printf("\t/* Handle replyp->%s; */\n", \
				    varname) >> TFILE
			}
		}
		printf("\n\t/*\n\t * XXX Code goes here\n\t */\n\n") >> TFILE
		printf("\treturn (replyp->status);\n") >> TFILE
		printf("}\n\n") >> TFILE
		#
		# If we are doing a list, write list functions for this op.
		#
		for (i = 0; i < rvars; ++i) {
			if (ret_type[i] != "LIST")
				continue;
			if (retlist_type[i] != "STRING" &&
			    retlist_type[i] != "INT" && list_type[i] != "ID")
				continue;
			printf("int\n") >> TFILE
			printf("__db_%s_%sreplist(locp, ppp)\n", \
			    name, retargs[i]) >> TFILE
			printf("\t__%s_%sreplist *locp;\n", \
			    name, retargs[i]) >> TFILE
			if (retlist_type[i] == "STRING") {
				printf("\tchar ***ppp;\n{\n") >> TFILE
				printf("\tchar **pp;\n") >> TFILE
			}
			if (retlist_type[i] == "INT" ||
			    retlist_type[i] == "ID") {
				printf("\tu_int32_t **ppp;\n{\n") >> TFILE
				printf("\tu_int32_t *pp;\n") >> TFILE
			}

			printf("\tint cnt, ret, size;\n") >> TFILE
			printf("\t__%s_%sreplist *nl;\n\n", \
			    name, retargs[i]) >> TFILE
			printf("\tfor (cnt = 0, nl = locp; ") >> TFILE
			printf("nl != NULL; cnt++, nl = nl->next)\n\t\t;\n\n") \
			    >> TFILE
			printf("\tif (cnt == 0) {\n") >> TFILE
			printf("\t\t*ppp = NULL;\n") >> TFILE
			printf("\t\treturn (0);\n\t}\n") >> TFILE
			printf("\tsize = sizeof(*pp) * cnt;\n") >> TFILE
			printf("\tif ((ret = __os_malloc(NULL, ") >> TFILE
			printf("size, NULL, ppp)) != 0)\n") >> TFILE
			printf("\t\treturn (ret);\n") >> TFILE
			printf("\tmemset(*ppp, 0, size);\n") >> TFILE
			printf("\tfor (pp = *ppp, nl = locp; ") >> TFILE
			printf("nl != NULL; nl = nl->next, pp++) {\n") >> TFILE
			if (retlist_type[i] == "STRING") {
				printf("\t\tif ((ret = __os_malloc(NULL, ") \
				    >> TFILE
				printf("nl->ent.ent_len + 1, NULL,") >> TFILE
				printf(" pp)) != 0)\n") >> TFILE
				printf("\t\t\tgoto out;\n") >> TFILE
				printf("\t\tif ((ret = __os_strdup(") >> TFILE
				printf("NULL, (char *)nl->ent.ent_val,") \
				    >> TFILE
				printf(" pp)) != 0)\n") >> TFILE
				printf("\t\t\tgoto out;\n") >> TFILE
			}
			if (retlist_type[i] == "INT" ||
			    retlist_type[i] == "ID") {
				printf("\t\t*pp = *(u_int32_t *)") >> TFILE
				printf("nl->ent.ent_val;\n") >> TFILE
			}
			printf("\t}\n") >> TFILE
			printf("\treturn (0);\n") >> TFILE
			printf("out:\n") >> TFILE
			printf("\t__db_%s_%sfree(*ppp);\n", \
			    name, retargs[i]) >> TFILE
			printf("\treturn (ret);\n") >> TFILE
			printf("}\n\n") >> TFILE

			printf("void\n") >> TFILE
			printf("__db_%s_%sfree(pp)\n", \
			    name, retargs[i]) >> TFILE

			if (retlist_type[i] == "STRING")
				printf("\tchar **pp;\n") >> TFILE
			if (retlist_type[i] == "INT" || retlist_type[i] == "ID")
				printf("\tu_int32_t *pp;\n") >> TFILE

			printf("{\n") >> TFILE
			printf("\tsize_t size;\n") >> TFILE

			if (retlist_type[i] == "STRING")
				printf("\tchar **p;\n\n") >> TFILE
			if (retlist_type[i] == "INT" || retlist_type[i] == "ID")
				printf("\tu_int32_t *p;\n\n") >> TFILE

			printf("\tif (pp == NULL)\n\t\treturn;\n") >> TFILE
			printf("\tsize = sizeof(*p);\n") >> TFILE
			printf("\tfor (p = pp; *p != 0; p++) {\n") >> TFILE
			printf("\t\tsize += sizeof(*p);\n") >> TFILE

			if (retlist_type[i] == "STRING")
				printf("\t\t__os_free(*p, strlen(*p)+1);\n") \
				    >> TFILE
			printf("\t}\n") >> TFILE
			printf("\t__os_free(pp, size);\n") >> TFILE
			printf("}\n\n") >> TFILE
		}
	}
}

#
# split_lines --
#	Add line separators to pretty-print the output.
function split_lines(is_public) {
	if (argcount > 3) {
		# Reset the counter, remove any trailing whitespace from
		# the separator.
		argcount = 0;
		sub("[ 	]$", "", sep)

		if (is_public) {
			printf("%s\n\t", sep) >> SHFILE
		} else {
			printf("%s\n\t\t", sep) >> PFILE
			printf("%s\\\n\\\t\\\t", sep) >> SEDFILE
		}
	}
}
