#!/usr/bin/env bash

# for all tokudb binary tarballs, verify that we can create and run the tarball using the MySQL sandbox.

function expand() {
    echo $* | tr ,: " "
}

let n=0
for f in *.md5; do
    if [[ $f =~ (.*).tar.gz.md5 ]] ; then
        mysqlbuild=${BASH_REMATCH[1]}
    else
        exit 1
    fi

    md5sum --check $f
    if [ $? != 0 ] ; then exit 1; fi
    make_sandbox --add_prefix=test$n- $mysqlbuild.tar.gz -- --sandbox_directory=test$n
    if [ $? != 0 ] ; then exit 1; fi
    pushd $HOME/sandboxes
    if [ $? = 0 ] ; then
        ./use_all 'show engines'
        ./use_all 'create table test.t (a int primary key, b bigint, c varchar(256), d blob(500000), clustering key(b))'
        ./use_all 'show create table test.t'
	./stop_all	
	popd
    fi
    let n=n+1
done


