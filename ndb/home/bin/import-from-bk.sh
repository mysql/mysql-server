#! /bin/sh

# XXX does not delete files
# XXX does not handle nested new dirs
# this script screams for perl, no time now
# look for bk2cvs on the net

PATH=/usr/local/bin:$PATH; export PATH
LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH; export LD_LIBRARY_PATH

batch=n
if [ "$1" = "-batch" ]; then
	batch=y
	shift
fi

say() {
	echo "$*"
}

die() {
	case $# in
	0)	set -- "command failed" ;;
	esac
	say "$* -- aborted" >&2
	exit 1
}

usage() {
	die "usage: $0 [-batch] top -- copy from mysql/ndb to another NDB_TOP"
}

doit() {
	cmd="$*"
	if [ $batch = n ]; then
		echo -n "$cmd [y]"
		read junk
		sh -c "$cmd"
		return 0
	else
		echo "$cmd"
		sh -c "$cmd"
		return $?
	fi
}

say "======================"
say "`date`"

case $# in
1)	[ -d $1/src/CVS ] || die "$1 is not an NDB_TOP"
	top=$1 ;;
*)	usage ;;
esac

if ! fgrep ndb_kernel_version.h $top/include/kernel/CVS/Entries >/dev/null 2>&1; then
	die "$top is not an NDB_TOP"
fi

if find $top -path '*/CVS/Tag' -print | grep . >/dev/null; then
	die "$top: contains CVS/Tag files, not accepted"
fi

if [ ! -f include/SCCS/s.ndb_version.h ]; then
	die "current dir ($PWD) is not an NDB_TOP"
fi

doit "bk pull" || exit 1
doit "bk -r clean"
doit "bk -r get -q"

files=`bk -r. sfiles -g |
	fgrep -v ' ' |
	fgrep -v /.cvsignore`

n=0
files2=
for f in $files; do
	if [ ! -f $f ]; then
		die "$f: no such file"
	fi
	if [ -w $f ]; then
		say "$f: is writable, accept anyway"
	fi
	files2="$files2 $f"
	n=$((n+1))
done
files=$files2
say "$n files..."

adddirs= addfiles= updfiles=
for f in $files; do
	d=`dirname $f`
	b=`basename $f`
	if [ ! -f $top/$d/CVS/Entries ]; then
		found=n
		for x in $adddirs; do
			if [ $x = $d ]; then found=y; break; fi
		done
		if [ $found = n ]; then
			say "$d: to create dir"
			adddirs="$adddirs $d"
		fi
		addfiles="$addfiles $f"
		say "$f: to create"
	elif ! fgrep "/$b/" $top/$d/CVS/Entries >/dev/null; then
		addfiles="$addfiles $f"
		say "$f: to create"
	else
		cmp $f $top/$f >/dev/null
		case $? in
		0)	continue ;;
		1)	;;
		*)	die "$f: unknown error" ;;
		esac
		updfiles="$updfiles $f"
		say "$f: to update"
	fi
done

for d in $adddirs; do
	doit "cd $top && mkdir -p $d" || die
done

for f in $addfiles $updfiles; do
	doit "cp -fp $f $top/$f" || die
done

for d in $adddirs; do
	# fix 1 level up
	d2=`dirname $d`
	if [ ! -d $top/$d2/CVS ]; then
		doit "cd $top && cvs add $d2" || die
	fi
	doit "cd $top && cvs add $d" || die
done

for f in $addfiles; do
	kb=
	if echo $f | perl -nle "print(-B $_)" | grep 1 >/dev/null; then
		kb="-kb"
	fi
	doit "cd $top && cvs add $kb $f" || die
done

tag=import_bk_`date +%Y_%m_%d`

doit "cd $top && cvs commit -m $tag" || die
doit "cd $top && cvs tag -F $tag" || die

env="NDB_TOP=$top; export NDB_TOP"
env="$env; USER_FLAGS='-DAPI_TRACE -fmessage-length=0'; export USER_FLAGS"
doit "$env; cd $top && ./configure"
doit "$env; cd $top && sh config/GuessConfig.sh"
doit "$env; cd $top && make clean nuke-deps vim-tags"
doit "$env; cd $top && make" || die

say "imported ok"
