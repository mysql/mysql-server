
# Warning - first line left blank for sh/csh/ksh compatibility.  Do not
# remove it.  fnf@Unisoft

# mklintlib --- make a lint library, under either System V or 4.2 BSD
#
# usage:  mklintlib <infile> <outfile>
#

if test $# -ne 2
then
	echo "usage: mklintlib <infile> <outfile>"
	exit 1
fi

if grep SIGTSTP /usr/include/signal.h >/dev/null
then							# BSD
	if test -r /usr/include/whoami.h		# 4.1
	then
		/lib/cpp -C -Dlint $1 >hlint
		(/usr/lib/lint/lint1 <hlint >$2) 2>&1 | grep -v warning
	else						# 4.2
		lint -Cxxxx $1
		mv llib-lxxxx.ln $2
	fi
else							# USG
	cc -E -C -Dlint $1 | /usr/lib/lint1 -vx -Hhlint >$2
	rm -f hlint
fi
exit 0							# don't kill make
