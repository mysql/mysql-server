#!/bin/bash 
# measure code coverage of the tokudb tests

tokudb_version=tokudb
tokudb=$tokudb_version
tokudb_checkout_dir=$tokudb

while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ "--tokudb=(.*)" ]] ; then
	tokudb_version=${BASH_REMATCH[1]}
	tokudb="tokudb-$tokudb_version"
	tokudb_checkout_dir="tokudb.branches/$tokudb_version"
    fi
done

echo $tokudb
echo $tokudb_checkout_dir

coveragedir=~/svn.coverage.$tokudb.`date +%Y%m%d`
mkdir $coveragedir
cd $coveragedir
svn co -q https://svn.tokutek.com/tokudb/$tokudb_checkout_dir
if [ $tokudb != "tokudb" ] ; then
    mv $tokudb_version $tokudb
fi
cd $tokudb

# build tokudb with coverage enable
make -k build-coverage

# run the tests
make -k check-coverage
(cd src/tests;make -k all.recover VGRIND="")

# make -k measure-coverage
rm $coveragedir/raw.test.coverage
for d in newbrt src utils cxx src/range_tree src/lock_tree; do
	(cd $d; python ~/bin/gcovsumdir.py -b *.c *.cpp >>$coveragedir/raw.test.coverage)
done
python ~/bin/gcovsumsum.py $coveragedir/raw.test.coverage >$coveragedir/test.coverage

