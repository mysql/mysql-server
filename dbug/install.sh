
#	WARNING -- first line intentionally left blank for sh/csh/ksh
#	compatibility.  Do not remove it!  FNF, UniSoft Systems.
#
#	Usage is:
#			install <from> <to>
#
#	The file <to> is replaced with the file <from>, after first
#	moving <to> to a backup file.  The backup file name is created
#	by prepending the filename (after removing any leading pathname
#	components) with "OLD".
#
#	This script is currently not real robust in the face of signals
#	or permission problems.  It also does not do (by intention) all
#	the things that the System V or BSD install scripts try to do
#

if [ $# -ne 2 ]
then
	echo  "usage: $0 <from> <to>"
	exit 1
fi

# Now extract the dirname and basename components.  Unfortunately, BSD does
# not have dirname, so we do it the hard way.

fd=`expr $1'/' : '\(/\)[^/]*/$' \| $1'/' : '\(.*[^/]\)//*[^/][^/]*//*$' \| .`
ff=`basename $1`
td=`expr $2'/' : '\(/\)[^/]*/$' \| $2'/' : '\(.*[^/]\)//*[^/][^/]*//*$' \| .`
tf=`basename $2`

# Now test to make sure that they are not the same files.

if [ $fd/$ff = $td/$tf ]
then
	echo "install: input and output are same files"
	exit 2
fi

# Save a copy of the "to" file as a backup.

if test -f $td/$tf
then
	if test -f $td/OLD$tf
	then
		rm -f $td/OLD$tf
	fi
	mv $td/$tf $td/OLD$tf
	if [ $? != 0 ]
	then
		exit 3
	fi
fi

# Now do the copy and return appropriate status

cp $fd/$ff $td/$tf
if [ $? != 0 ]
then
	exit 4
else
	exit 0
fi

