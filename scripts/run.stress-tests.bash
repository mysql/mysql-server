#!/bin/bash
# $Id$

DOC=<<EOF

  PARAMETERS

    table size: small (2 000), medium (200 000), large (50 000 000)

    cachetable size: small (num_elements * 50), large (1 000 000 000)

    update threads: 1, random number <= 20

    point query threads: 1, random number <= 20

    recover-test_stress1, recover-test_stress2

  DATA

    currently running tests

    log of success/failure ("./recover-test_stress1.tdb --num_elements blah blah blah      PASS")

    if failed:

      parameters

      corefile

      stdout/stderr

      data directory

EOF

set -e

scriptname=$(basename "$0")
toku_toplevel=$(dirname $(dirname $(readlink -f "$PWD/$0")))
log=/tmp/run.stress-tests.log
savedir=/tmp/run.stress-tests.failures

usage() {
    echo "Usage: $scriptname" 1>&2
    echo "  [--toku_toplevel=<dir>]" 1>&2
    echo "  [--log=<file>]" 1>&2
    echo "  [--savedir=<dir>]" 1>&2
}

# parse the command line
while [ $# -gt 0 ] ; do
    arg=$1; shift
    if [[ $arg =~ --(.*)=(.*) ]] ; then
        ok=no
        for opt in toku_toplevel log savedir
        do
            if [[ ${BASH_REMATCH[1]} = $opt ]]
            then
                ok=yes
            fi
        done
        if [[ $ok = no ]]
        then
            usage; exit 1
        fi
        eval ${BASH_REMATCH[1]}=${BASH_REMATCH[2]}
    else
        usage; exit 1
    fi
done

src_tests="${toku_toplevel}/src/tests"
testnames=(recover-test_stress1.tdb \
           recover-test_stress2.tdb)

save_failure() {
    dir="$1"; shift
    out="$1"; shift
    envdir="$1"; shift
    exec="$1"; shift
    table_size=$1; shift
    cachetable_size=$1; shift
    num_ptquery=$1; shift
    num_update=$1; shift
    phase=$1; shift
    dest="${dir}/${exec}-${table_size}-${cachetable_size}-${num_ptquery}-${num_update}-${phase}-$$"
    mkdir -p "$dest"
    mv $out "${dest}/output.txt"
    mv core* "${dest}/"
    mv $envdir "${dest}/"
}

run_test() {
    exec="$1"; shift
    table_size="$1"; shift
    cachetable_size="$1"; shift
    num_ptquery="$1"; shift
    num_update="$1"; shift
    mylog="$1"; shift
    mysavedir="$1"; shift

    rundir=$(mktemp -d ./rundir.XXXXXXXX)
    tmplog=$(mktemp)

    ulimit -c unlimited
    t0="$(date)"
    t1=""
    t2=""
    envdir="../${exec}-${table_size}-${cachetable_size}-${num_ptquery}-${num_update}-$$.dir"
    cd $rundir
    if ! LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
        ../$exec -v --test --num_seconds 180 --envdir "$envdir" \
        --num_elements $table_size \
        --cachetable_size $cachetable_size \
        --num_ptquery_threads $num_ptquery \
        --num_update_threads $num_update > $tmplog
    then
        rm -f $tmplog
        t1="$(date)"
        if LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
            ../$exec -v --recover --envdir "$envdir" \
            --num_elements $table_size \
            --cachetable_size $cachetable_size > $tmplog
        then
            rm -f $tmplog
            t2="$(date)"
            echo "\"$exec\",$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,PASS" > "$mylog"
        else
            save_failure "$mysavedir" $tmplog $envdir $exec $table_size $cachetable_size $num_ptquery $num_update recover
            echo "\"$exec\",$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" > "$mylog"
        fi
    else
        save_failure "$mysavedir" $tmplog $envdir $exec $table_size $cachetable_size $num_ptquery $num_update test
        echo "\"$exec\",$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" > "$mylog"
    fi
    cd ..
    rm -rf $rundir
}

loop_test() {
    exec="$1"; shift
    table_size="$1"; shift
    cachetable_size="$1"; shift
    mylog="$1"; shift
    mysavedir="$1"; shift

    ptquery_rand=0
    update_rand=0
    while true
    do
        num_ptquery=1
        num_update=1
        if [[ $ptquery_rand -gt 1 ]]
        then
            (( num_ptquery = $RANDOM % 16 ))
        fi
        if [[ $update_rand -gt 0 ]]
        then
            (( num_update = $RANDOM % 16 ))
        fi
        (( ptquery_rand = (ptquery_rand + 1) % 4 ))
        (( update_rand = (update_rand + 1) % 2 ))
        run_test $exec $table_size $cachetable_size $num_ptquery $num_update $mylog $mysavedir
    done
}

cd $src_tests

for exec in ${testnames[@]}
do
    if [[ ! -x $exec ]]
    then
        echo "Please build $exec" 1>&2
        exit 1
    fi
done

mkdir -p $log
mkdir -p $savedir

declare -a pids
i=0

savepid() {
    pids[i]=$1
    (( i = i + 1 ))
}

killchildren() {
    for pid in ${pids[@]}
    do
        kill $pid
    done
}

for exec in ${testnames[@]}
do
    for table_size in 2000 200000 50000000
    do
        (( small_cachetable = table_size * 50 ))
        suffix="${exec}-${table_size}-${small_cachetable}-$$"
        touch "${log}/${suffix}"
        loop_test $exec $table_size $small_cachetable "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!
        tail -f "${log}/${suffix}" & savepid $!
        suffix="${exec}-${table_size}-1000000000-$$"
        touch "${log}/${suffix}"
        loop_test $exec $table_size 1000000000 "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!
        tail -f "${log}/${suffix}" & savepid $!
    done
done

trap killchildren INT TERM EXIT

for pid in ${pids[@]}
do
    wait $pid
done
