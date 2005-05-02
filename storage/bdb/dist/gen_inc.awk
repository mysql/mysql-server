# This awk script parses C input files looking for lines marked "PUBLIC:"
# and "EXTERN:".  (PUBLIC lines are DB internal function prototypes and
# #defines, EXTERN are DB external function prototypes and #defines.)
#
# PUBLIC lines are put into two versions of per-directory include files:
# one file that contains the prototypes, and one file that contains a
# #define for the name to be processed during configuration when creating
# unique names for every global symbol in the DB library.
#
# The EXTERN lines are put into two files: one of which contains prototypes
# which are always appended to the db.h file, and one of which contains a
# #define list for use when creating unique symbol names.
#
# Four arguments:
#	e_dfile		list of EXTERN #defines
#	e_pfile		include file that contains EXTERN prototypes
#	i_dfile		list of internal (PUBLIC) #defines
#	i_pfile		include file that contains internal (PUBLIC) prototypes
/PUBLIC:/ {
	sub("^.*PUBLIC:[	 ][	 ]*", "")
	if ($0 ~ /^#(if|ifdef|ifndef|else|endif)/) {
		print $0 >> i_pfile
		print $0 >> i_dfile
		next
	}
	pline = sprintf("%s %s", pline, $0)
	if (pline ~ /\)\);/) {
		sub("^[	 ]*", "", pline)
		print pline >> i_pfile
		if (pline !~ db_version_unique_name) {
			sub("[	 ][	 ]*__P.*", "", pline)
			sub("^.*[	 ][*]*", "", pline)
			printf("#define	%s %s@DB_VERSION_UNIQUE_NAME@\n",
			    pline, pline) >> i_dfile
		}
		pline = ""
	}
}

# When we switched to methods in 4.0, we guessed txn_{abort,begin,commit}
# were the interfaces applications would likely use and not be willing to
# change, due to the sheer volume of the calls.  Provide wrappers -- we
# could do txn_abort and txn_commit using macros, but not txn_begin, as
# the name of the field is txn_begin, we didn't want to modify it.
#
# The issue with txn_begin hits us in another way.  If configured with the
# --with-uniquename option, we use #defines to re-define DB's interfaces
# to unique names.  We can't do that for these functions because txn_begin
# is also a field name in the DB_ENV structure, and the #defines we use go
# at the end of the db.h file -- we get control too late to #define a field
# name.  So, modify the script that generates the unique names #defines to
# not generate them for these three functions, and don't include the three
# functions in libraries built with that configuration option.
/EXTERN:/ {
	sub("^.*EXTERN:[	 ][	 ]*", "")
	if ($0 ~ /^#(if|ifdef|ifndef|else|endif)/) {
		print $0 >> e_pfile
		print $0 >> e_dfile
		next
	}
	eline = sprintf("%s %s", eline, $0)
	if (eline ~ /\)\);/) {
		sub("^[	 ]*", "", eline)
		print eline >> e_pfile
		if (eline !~ db_version_unique_name && eline !~ /^int txn_/) {
			sub("[	 ][	 ]*__P.*", "", eline)
			sub("^.*[	 ][*]*", "", eline)
			printf("#define	%s %s@DB_VERSION_UNIQUE_NAME@\n",
			    eline, eline) >> e_dfile
		}
		eline = ""
	}
}
