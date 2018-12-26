#!/bin/sh -
#	$NetBSD: makelist,v 1.16 2010/04/18 21:17:05 christos Exp $
#
# Copyright (c) 1992, 1993
#	The Regents of the University of California.  All rights reserved.
#
# This code is derived from software contributed to Berkeley by
# Christos Zoulas of Cornell University.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	@(#)makelist	5.3 (Berkeley) 6/4/93

# makelist.sh: Automatically generate header files...

AWK=@AWK@
USAGE="Usage: $0 -n|-h|-e|-fc|-fh|-bc|-bh|-m <filenames>"

if [ "x$1" = "x" ]
then
    echo $USAGE 1>&2
    exit 1
fi

FLAG="$1"
shift

FILES="$@"

case $FLAG in

#	generate foo.h file from foo.c
#
-n)
    cat << _EOF
#undef WIDECHAR
#define NARROWCHAR
#include "${FILES}"
_EOF
    ;;
    
-h)
    set - `echo $FILES | sed -e 's/\\./_/g'`
    hdr="_h_`basename $1`"
    cat $FILES | $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#ifndef %s\n#define %s\n", "'$hdr'", "'$hdr'");
	}
	/\(\):/ {
	    pr = substr($2, 1, 2);
	    if (pr == "vi" || pr == "em" || pr == "ed") {
		# XXXMYSQL: support CRLF
		name = substr($2, 1, index($2,"(") - 1);
#
# XXX:	need a space between name and prototype so that -fc and -fh
#	parsing is much easier
#
		printf("protected el_action_t\t%s (EditLine *, Int);\n", name);
	    }
	}
	END {
	    printf("#endif /* %s */\n", "'$hdr'");
	}'
	;;

#	generate help.c from various .c files
#
-bc)
    cat $FILES | $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#include \"config.h\"\n#include \"el.h\"\n");
	    printf("#include \"chartype.h\"\n");
	    printf("private const struct el_bindings_t el_func_help[] = {\n");
	    low = "abcdefghijklmnopqrstuvwxyz_";
	    high = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
	    for (i = 1; i <= length(low); i++)
		tr[substr(low, i, 1)] = substr(high, i, 1);
	}
	/\(\):/ {
	    pr = substr($2, 1, 2);
	    if (pr == "vi" || pr == "em" || pr == "ed") {
		# XXXMYSQL: support CRLF
		name = substr($2, 1, index($2,"(") - 1);
		uname = "";
		fname = "";
		for (i = 1; i <= length(name); i++) {
		    s = substr(name, i, 1);
		    uname = uname tr[s];
		    if (s == "_")
			s = "-";
		    fname = fname s;
		}

		printf("    { %-30.30s %-30.30s\n","STR(\"" fname "\"),", uname ",");
		ok = 1;
	    }
	}
	/^ \*/ {
	    if (ok) {
		printf("      STR(\"");
		for (i = 2; i < NF; i++)
		    printf("%s ", $i);
        # XXXMYSQL: support CRLF
		sub("\r", "", $i);
		printf("%s\") },\n", $i);
		ok = 0;
	    }
	}
	END {
	    printf("};\n");
	    printf("\nprotected const el_bindings_t* help__get()");
	    printf("{ return el_func_help; }\n");
	}'
	;;

#	generate help.h from various .c files
#
-bh)
    $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#ifndef _h_help_c\n#define _h_help_c\n");
	    printf("protected const el_bindings_t *help__get(void);\n");
	    printf("#endif /* _h_help_c */\n");
	}' /dev/null
	;;

#	generate fcns.h from various .h files
#
# XXXMYSQL: use portable tr syntax
-fh)
    cat $FILES | $AWK '/el_action_t/ { print $3 }' | \
    sort | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ | $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#ifndef _h_fcns_c\n#define _h_fcns_c\n");
	    count = 0;
	}
	{
	    printf("#define\t%-30.30s\t%3d\n", $1, count++);
	}
	END {
	    printf("#define\t%-30.30s\t%3d\n", "EL_NUM_FCNS", count);

	    printf("typedef el_action_t (*el_func_t)(EditLine *, Int);");
	    printf("\nprotected const el_func_t* func__get(void);\n");
	    printf("#endif /* _h_fcns_c */\n");
	}'
	;;

#	generate fcns.c from various .h files
#
-fc)
    cat $FILES | $AWK '/el_action_t/ { print $3 }' | sort | $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#include \"config.h\"\n#include \"el.h\"\n");
	    printf("private const el_func_t el_func[] = {");
	    maxlen = 80;
	    needn = 1;
	    len = 0;
	}
	{
	    clen = 25 + 2;
	    len += clen;
	    if (len >= maxlen)
		needn = 1;
	    if (needn) {
		printf("\n    ");
		needn = 0;
		len = 4 + clen;
	    }
	    s = $1 ",";
	    printf("%-26.26s ", s);
	}
	END {
	    printf("\n};\n");
	    printf("\nprotected const el_func_t* func__get() { return el_func; }\n");
	}'
	;;

#	generate editline.c from various .c files
#
-e)
	echo "$FILES" | tr ' ' '\012' | $AWK '
	BEGIN {
	    printf("/* Automatically generated file, do not edit */\n");
	    printf("#define protected static\n");
	    printf("#define SCCSID\n");
	}
	{
	    printf("#include \"%s\"\n", $1);
	}'
	;;

#	generate man page fragment from various .c files
#
-m)
    cat $FILES | $AWK '
	BEGIN {
	    printf(".\\\" Section automatically generated with makelist\n");
	    printf(".Bl -tag -width 4n\n");
	}
	/\(\):/ {
	    pr = substr($2, 1, 2);
	    if (pr == "vi" || pr == "em" || pr == "ed") {
		# XXXMYSQL: support CRLF
		name = substr($2, 1, index($2, "(") - 1);
		fname = "";
		for (i = 1; i <= length(name); i++) {
		    s = substr(name, i, 1);
		    if (s == "_")
			s = "-";
		    fname = fname s;
		}

		printf(".It Ic %s\n", fname);
		ok = 1;
	    }
	}
	/^ \*/ {
	    if (ok) {
		for (i = 2; i < NF; i++)
		    printf("%s ", $i);
		printf("%s.\n", $i);
		ok = 0;
	    }
	}
	END {
	    printf(".El\n");
	    printf(".\\\" End of section automatically generated with makelist\n");
	}'
	;;

*)
    echo $USAGE 1>&2
    exit 1
    ;;

esac
