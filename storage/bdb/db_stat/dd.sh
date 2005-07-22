#! /bin/sh
#	$Id: dd.sh,v 1.3 2004/05/04 15:51:45 bostic Exp $
#
# Display environment's deadlocks based on "db_stat -Co" output.

t1=__a
t2=__b

trap 'rm -f $t1 $t2; exit 0' 0 1 2 3 13 15

if [ $# -ne 1 ]; then
	echo "Usage: dd.sh [db_stat -Co output]"
	exit 1
fi

if `egrep '\<WAIT\>.*\<page\>' $1 > /dev/null`; then
	n=`egrep '\<WAIT\>.*\<page\>' $1 | wc -l | awk '{print $1}'`
	echo "dd.sh: $1: $n page locks in a WAIT state."
else
	echo "dd.sh: $1: No page locks in a WAIT state found."
	exit 1
fi

# Print out list of node wait states, and output cycles in the graph.
egrep '\<WAIT\>.*\<page\>' $1 | awk '{print $1 " " $7}' |
while read l p; do
	p=`egrep "\<HELD\>.*\<page\>[	 ][	 ]*$p$" $1 | awk '{print $1}'`
	echo "$l $p"
done | tsort > /dev/null 2>$t1

# Display the locks in a single cycle.
display_one() {
	if [ -s $1 ]; then
		echo "Deadlock #$c ============"
		c=`expr $c + 1`
		cat $1 | sort -n +6
		:> $1
	fi
}

# Display the locks in all of the cycles.
#
# Requires tsort output some text before each list of nodes in the cycle,
# and the actual node displayed on the line be the second (white-space)
# separated item on the line.  For example:
#
#	tsort: cycle in data
#	tsort: 8000177f
#	tsort: 80001792
#	tsort: 80001774
#	tsort: cycle in data
#	tsort: 80001776
#	tsort: 80001793
#	tsort: cycle in data
#	tsort: 8000176a
#	tsort: 8000178a
#
# XXX
# Currently, db_stat doesn't display the implicit wait relationship between
# parent and child transactions, where the parent won't release a lock until
# the child commits/aborts.  This means the deadlock where parent holds a
# lock, thread A waits on parent, child waits on thread A won't be shown.
if [ -s $t1 ]; then
	c=1
	:>$t2
	while read a b; do
		case $b in
		[0-9]*)
			egrep $b $1 >> $t2;;
		*)
			display_one $t2;;
		esac
	done < $t1
	display_one $t2
else
	echo 'No deadlocks found.'
fi

exit 0
