# NAME
#   safe, safe_eval, die, rawdie, syndie, msg, errmsg,
#   rawmsg, rawerrmsg, trace, errtrace, is_wordmatch 
#   - functions for safe execution and convenient printing and tracing
#
#   abspath - make a path absolute
# 
# SYNOPSIS
#   .  funcs.sh
#
#   is_wordmatch requires perl.
#
# DESCRIPTION
#   Funcs.sh is a collection of somewhat related functions.
#   The main categories and their respective functions are:
#     Controlled execution 		- safe, safe_eval
#     Exiting with a message		- die, rawdie, syndie 
#     Printing messages			- msg, errmsg, rawmsg, rawerrmsg
#     Tracing				- trace, errtrace
#     Pattern matching			- is_wordmatch
#   
#   
# ENVIRONMENT
#   These variables are not exported, but they are still visible
#   to, and used by, these functions.
#
#   	progname	basename of $0
#	verbose		empty or non-emtpy, used for tracing
#	synopsis	string describing the syntax of $progname 
#
# VERSION
#   2.0
#
# AUTHOR
#   Jonas Mvlsd
#   Jonas Oreland - added abspath





# Safely executes the given command and exits 
# with the given commands exit code if != 0,
# else the return value ("the functions exit 
# code") is 0. Eg: safely cd $install_dir
#
safely ()
{
    "$@"
    safely_code__=$?
    [ $safely_code__ -ne 0 ]  &&  
        { errmsg "Command failed: $@. Exit code: $safely_code__.";
	  exit $safely_code__; }
	  
    : # return "exit code" 0 from function
}




# Safely_eval executes "eval command" and exits 
# with the given commands exit code if != 0,
# else the return value (the functions "exit 
# code") is 0.
#
# Safely_eval is just like like safely, but safely_eval does 
# "eval command" instead of just "command"
#
# Safely_eval even works with pipes etc., but you have to quote
# the special characters. Eg:  safely_eval  ls \|  wc \> tst.txt 2\>\&1
#
#
safely_eval ()
{
    eval "$@"
    safely_eval_code__=$?
    [ $safely_eval_code__ -ne 0 ]  &&  
        { errmsg "Command failed: $@. Exit code: $safely_eval_code__.";
	  exit $safely_eval_code__; }
	  
    : # return "exit code" 0 from function
}






#
# safe and safe_eval are deprecated, use safely and safely_eval instead
#

# Safe executes the given command and exits 
# with the given commands exit code if != 0,
# else the return value ("the functions exit 
# code") is 0.
#
safe ()
{
    "$@"
    safe_code__=$?
    [ $safe_code__ -ne 0 ]  &&  
        { errmsg "Command failed: $@. Exit code: $safe_code__.";
	  exit $safe_code__; }
	  
    : # return "exit code" 0 from function
}




# Safe_eval executes "eval command" and exits 
# with the given commands exit code if != 0,
# else the return value (the functions "exit 
# code") is 0.
#
# Safe_eval is just like like safe, but safe_eval does 
# "eval command" instead of just "command"
#
# Safe_eval even works with pipes etc., but you have to quote
# the special characters. Eg:  safe_eval  ls \|  wc \> tst.txt 2\>\&1
#
#
safe_eval ()
{
    eval "$@"
    safe_eval_code__=$?
    [ $safe_eval_code__ -ne 0 ]  &&  
        { errmsg "Command failed: $@. Exit code: $safe_eval_code__.";
	  exit $safe_eval_code__; }
	  
    : # return "exit code" 0 from function
}






# die prints the supplied message to stderr,
# prefixed with the program name, and exits 
# with the exit code given by "-e num" or 
# 1, if no -e option is present.
#
die ()
{
        die_code__=1
	[ "X$1" = X-e ]  &&  { die_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	errmsg "$@"
	exit $die_code__
}



# rawdie prints the supplied message to stderr.
# It then exits with the exit code given with "-e num"
# or 1, if no -e option is present.
#
rawdie ()
{
        rawdie_code__=1
	[ "X$1" = X-e ]  &&  { rawdie_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	rawerrmsg "$@"
	exit $rawdie_code__
}




# Syndie prints the supplied message (if present) to stderr,
# prefixed with the program name, on the first line.
# On the second line, it prints $synopsis.
# It then exits with the exit code given with "-e num"
# or 1, if no -e option is present.
#
syndie ()
{
        syndie_code__=1
	[ "X$1" = X-e ]  &&  { syndie_code__=$2; shift 2; }
	[ "X$1" = X-- ]  &&  shift 
	[ -n "$*" ] && msg "$*"
	rawdie -e $syndie_code__  "Synopsis: $synopsis"
}




# msg prints the supplied message to stdout,
# prefixed with the program name.
#
msg ()
{
	echo "${progname:-<no program name set>}:" "$@" 
}



# msg prints the supplied message to stderr,
# prefixed with the program name.
#
errmsg ()
{
	echo "${progname:-<no program name set>}:" "$@" >&2
}



rawmsg () { echo "$*"; }  	# print the supplied message to stdout
rawerrmsg () { echo "$*" >&2; } # print the supplied message to stderr



# trace prints the supplied message to stdout if verbose is non-null
#
trace ()
{
    [ -n "$verbose" ]  &&  msg "$@"
}


# errtrace prints the supplied message to stderr if verbose is non-null
#
errtrace ()
{
    [ -n "$verbose" ]  &&  msg "$@" >&2
}



# SYNTAX  
#   is_wordmatch candidatelist wordlist
#
# DESCRIPTION
#   is_wordmatch returns true if any of the words (candidates)
#   in candidatelist is present in wordlist, otherwise it
#   returns false. 
#
# EXAMPLES
#   is_wordmatch "tuareg nixdorf low content"  "xx yy zz low fgj turn roff sd"
#   returns true, since "low" in candidatelist is present in wordlist.
#
#   is_wordmatch "tuareg nixdorf low content"  "xx yy zz slow fgj turn roff sd"
#   returns false, since none of the words in candidatelist occurs in wordlist.
#
#   is_wordmatch "tuareg nixdorf low content"  "xx yy zz low fgj tuareg roff"
#   returns true, since "low" and "tuareg" in candidatelist occurs in wordlist.
#
is_wordmatch ()
{
    is_wordmatch_pattern__=`echo $1    | 
                sed 's/^/\\\\b/;
                     s/[ 	][ 	]*/\\\\b|\\\\b/g;
                     s/$/\\\\b/;'`
    shift 
    echo "$*" | 
    perl -lne "m/$is_wordmatch_pattern__/ || exit 1" 
}

#
# abspath
#
#   Stolen from http://oase-shareware.org/shell/shelltips/script_programmer.html
#
abspath()
{
    __abspath_D=`dirname "$1"`
    __abspath_B=`basename "$1"`
    echo "`cd \"$__abspath_D\" 2>/dev/null && pwd || echo \"$__abspath_D\"`/$__abspath_B"
}

#
#
# NdbExit
#
#
NdbExit()
{
    echo "NdbExit: $1"
    exit $1
}

NdbGetExitCode()
{
    __res__=`echo $* | awk '{if($1=="NdbExit:") print $2;}'`
    if [ -n $__res__ ]
    then
	echo $__res__
    else
	echo 255
    fi
}

