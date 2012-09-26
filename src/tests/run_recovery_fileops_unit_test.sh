#!/usr/bin/env bash

set -e
test $# -ge 2

bin=$1; shift
abortcode=$1; shift

rm -rf dir.recovery_fileops_unit.c.errors
mkdir dir.recovery_fileops_unit.c.errors
Oset="c d r"
aset="0 1"
bset="0 1"
cset="0 1 2"
fset="0 1"
count=0
for O in $Oset
do
    if test $O = c
    then
        gset="0"
        hset="0"
    else
        gset="0 1 2 3 4 5"
        hset="0 1"
    fi
    for a in $aset
    do
        for b in $bset
        do
            if test $b -eq 0
            then
                dset="0"
                eset="0"
            else
                dset="0 1"
                eset="0 1"
            fi
            for c in $cset
            do
                for d in $dset
                do
                    for e in $eset
                    do
                        for f in $fset
                        do
                            for g in $gset
                            do
                                for h in $hset
                                do
                                    if [[ "$O" != "c" && $c -eq 0 && ( $b -eq 0 || $e -eq 0 || $d -eq 1 ) ]]
                                    then
                                        iset="0 1"
                                    else
                                        iset="0"
                                    fi
                                    for i in $iset
                                    do
                                        errorfile=dir.recovery_fileops_unit.c.errors/crash.$O.$a.$b.$c.$d.$e.$f.$g.$h.$i
                                        combination="-O $O -A $a -B $b -C $c -D $d -E $e -F $f -G $g -H $h -I $i"
                                        set +e
                                        $bin -c $combination 2> $errorfile
                                        test $? -eq $abortcode || { cat $errorfile; echo Error: no crash in $errorfile; exit 1; }
                                        set -e
                                        grep -q 'HAPPY CRASH' $errorfile || { cat $errorfile; echo Error: incorrect crash in $errorfile; exit 1; }
                                        $bin -r $combination 2>> $errorfile || { cat $errorfile; echo Error: during recovery in $errorfile; exit 1; }
                                        count=$(($count + 1))
                                    done
                                done
                            done
                        done
                    done
                done
            done
        done
    done
done
echo $count tests passed
