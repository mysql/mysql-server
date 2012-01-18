#!/bin/bash
# $Id: run.stress-tests.bash 38773 2012-01-13 20:35:00Z leifwalsh $

set -e

scriptname=$(basename "$0")
toku_toplevel=$(dirname $(dirname $(readlink -f "$PWD/$0")))

src_tests="${toku_toplevel}/src/tests"
testnames=(test_stress1.tdb \
           test_stress5.tdb \
           test_stress6.tdb)

declare -a pids=(0)
i=0

savepid() {
    pids[$i]=$1
    (( i = i + 1 ))
}

killchildren() {
    kill ${pids[@]} || true
    for exec in ${testnames[@]}
    do
        pkill -f $exec || true
    done
}

trap killchildren INT TERM EXIT

run_test() {
    exec="$1"; shift
    table_size="$1"; shift
    cachetable_size="$1"; shift
    num_ptquery="$1"; shift
    num_update="$1"; shift

    rundir=$(mktemp -d ./rundir.XXXXXXXX)
    tmplog=$(mktemp)

    ulimit -c unlimited
    t0="$(date)"
    t1=""
    envdir="../${exec}-${table_size}-${cachetable_size}-${num_ptquery}-${num_update}-forever-$$.dir"
    cd $rundir
    if LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
        ../$exec -v --only_create --envdir "$envdir" \
        --num_elements $table_size \
        --cachetable_size $cachetable_size &> $tmplog
    then
        rm -f $tmplog
        t1="$(date)"
        echo "Running $exec -v --only_stress --num_seconds 0 --envdir \"$envdir\" --num_elements $table_size --cachetable_size $cachetable_size --num_ptquery_threads $num_ptquery --no-crash_on_update_failure --num_update_threads $num_update &> $tmplog in $rundir."
        (LD_LIBRARY_PATH=../../../lib:$LD_LIBRARY_PATH \
            ../$exec -v --only_stress --num_seconds 0 --envdir "$envdir" \
            --num_elements $table_size \
            --cachetable_size $cachetable_size \
            --num_ptquery_threads $num_ptquery \
            --no-crash_on_update_failure \
            --num_update_threads $num_update &> $tmplog) & mypid=$!
        savepid $mypid
        while true
        do
            sleep 10s
            cpu=$(ps -o pcpu -p $mypid h)
            if [[ -z $cpu ]]
            then
                echo "Process $mypid must have crashed: $exec,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,FAIL" 1>&2
                echo "Check rundir $rundir, envdir $envdir, corefile core.$mypid." 1>&2
                return
            fi
            if expr $cpu == 0.0 &>/dev/null
            then
                echo "Deadlock detected in process $mypid: $exec,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,FAIL" 1>&2
                echo "Check rundir $rundir, envdir $envdir, corefile core.$mypid." 1>&2
                return
            fi
        done
    else
        echo "Create phase failed: $exec,$table_size,$cachetable_size,$num_ptquery,$num_update,$t0,$t1,FAIL" 1>&2
    fi
    cd ..
    rm -rf $rundir "$envdir"
}

cd $src_tests
for exec in ${testnames[@]}
do
    for table_size in 2000 200000 50000000
    do
        (( small_cachetable = table_size * 50 ))
        run_test $exec $table_size $small_cachetable 4 4 & savepid $!
    done
done

wait ${pids[@]} || true
