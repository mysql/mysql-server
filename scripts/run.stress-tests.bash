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

. /opt/intel/bin/compilervars.sh intel64

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
testnames=(test_stress1.tdb \
           test_stress5.tdb \
           test_stress6.tdb)
recover_testnames=(recover-test_stress1.tdb \
                   recover-test_stress2.tdb \
                   recover-test_stress3.tdb)

save_failure() {
    dir="$1"; shift
    out="$1"; shift
    envdir="$1"; shift
    rev=$1; shift
    exec="$1"; shift
    table_size=$1; shift
    cachetable_size=$1; shift
    num_ptquery=$1; shift
    num_update=$1; shift
    phase=$1; shift
    dest="${dir}/${exec}-${table_size}-${cachetable_size}-${num_ptquery}-${num_update}-${phase}-${rev}-$$"
    mkdir -p "$dest"
    mv $out "${dest}/output.txt"
    mv core* "${dest}/"
    mv $envdir "${dest}/"
}

running=no

run_test() {
    rev=$1; shift
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
    if LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
        ../$exec -v --only_create --num_seconds 600 --envdir "$envdir" \
        --num_elements $table_size \
        --cachetable_size $cachetable_size &> $tmplog
    then
        rm -f $tmplog
        t1="$(date)"
        if LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
            ../$exec -v --only_stress --num_seconds 600 --no-crash_on_update_failure --envdir "$envdir" \
            --num_elements $table_size \
            --cachetable_size $cachetable_size \
            --num_ptquery_threads $num_ptquery \
            --num_update_threads $num_update &> $tmplog
        then
            rm -f $tmplog
            t2="$(date)"
            echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,PASS" | tee -a "$mylog"
        else
            save_failure "$mysavedir" $tmplog $envdir $rev $exec $table_size $cachetable_size $num_ptquery $num_update stress
            echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" | tee -a "$mylog"
        fi
    else
        save_failure "$mysavedir" $tmplog $envdir $rev $exec $table_size $cachetable_size $num_ptquery $num_update create
        echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" | tee -a "$mylog"
    fi
    cd ..
    rm -rf $rundir "$envdir"
}

loop_test() {
    rev=$1; shift
    exec="$1"; shift
    table_size="$1"; shift
    cachetable_size="$1"; shift
    mylog="$1"; shift
    mysavedir="$1"; shift

    ptquery_rand=0
    update_rand=0
    while [[ $running = "yes" ]]
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
        run_test $rev $exec $table_size $cachetable_size $num_ptquery $num_update $mylog $mysavedir
    done
}

run_recover_test() {
    rev=$1; shift
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
        ../$exec -v --test --num_seconds 600 --no-crash_on_update_failure --envdir "$envdir" \
        --num_elements $table_size \
        --cachetable_size $cachetable_size \
        --num_ptquery_threads $num_ptquery \
        --num_update_threads $num_update &> $tmplog
    then
        rm -f $tmplog
        t1="$(date)"
        if LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
            ../$exec -v --recover --envdir "$envdir" \
            --num_elements $table_size \
            --cachetable_size $cachetable_size &> $tmplog
        then
            rm -f $tmplog
            t2="$(date)"
            echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,PASS" | tee -a "$mylog"
        else
            save_failure "$mysavedir" $tmplog $envdir $rev $exec $table_size $cachetable_size $num_ptquery $num_update recover
            echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" | tee -a "$mylog"
        fi
    else
        save_failure "$mysavedir" $tmplog $envdir $rev $exec $table_size $cachetable_size $num_ptquery $num_update test
        echo "\"$exec\",$rev,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,$t2,FAIL" | tee -a "$mylog"
    fi
    cd ..
    rm -rf $rundir "$envdir"
}

loop_recover_test() {
    rev=$1; shift
    exec="$1"; shift
    table_size="$1"; shift
    cachetable_size="$1"; shift
    mylog="$1"; shift
    mysavedir="$1"; shift

    ptquery_rand=0
    update_rand=0
    while [[ $running = "yes" ]]
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
        run_recover_test $rev $exec $table_size $cachetable_size $num_ptquery $num_update $mylog $mysavedir
    done
}

declare -a pids=(0)
i=0

savepid() {
    pids[$i]=$1
    (( i = i + 1 ))
}

killchildren() {
    kill ${pids[@]} || true
    for exec in ${testnames[@]} ${recover_testnames[@]}
    do
        pkill -f $exec || true
    done
}

trap killchildren INT TERM EXIT

mkdir -p $log
mkdir -p $savedir

while true
do
    (cd $toku_toplevel; \
        svn update; \
        make CC=icc DEBUG=0 HAVE_CILK=0 clean fastbuild; \
        make CC=icc DEBUG=0 HAVE_CILK=0 -C src/tests ${testnames[@]} ${recover_testnames[@]})

    cd $src_tests

    rev=$(svn info ../.. | awk '/Revision/ { print $2 }')

    running=yes

    for exec in ${testnames[@]}
    do
        for table_size in 2000 200000 50000000
        do
            (( small_cachetable = table_size * 50 ))
            suffix="${exec}-${table_size}-${small_cachetable}-$$"
            touch "${log}/${suffix}"
            loop_test $rev $exec $table_size $small_cachetable "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!

            suffix="${exec}-${table_size}-1000000000-$$"
            touch "${log}/${suffix}"
            loop_test $rev $exec $table_size 1000000000 "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!
        done
    done

    for exec in ${recover_testnames[@]}
    do
        for table_size in 2000 200000 50000000
        do
            (( small_cachetable = table_size * 50 ))
            suffix="${exec}-${table_size}-${small_cachetable}-$$"
            touch "${log}/${suffix}"
            loop_recover_test $rev $exec $table_size $small_cachetable "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!

            suffix="${exec}-${table_size}-1000000000-$$"
            touch "${log}/${suffix}"
            loop_recover_test $rev $exec $table_size 1000000000 "${log}/${suffix}" "${savedir}/${suffix}" & savepid $!
        done
    done

    sleep 1d

    running=no

    killchildren

    wait ${pids[@]} || true

    idx=0
    for pid in ${pids[@]}
    do
        pids[$idx]=0
        (( idx = idx + 1 ))
    done
done
