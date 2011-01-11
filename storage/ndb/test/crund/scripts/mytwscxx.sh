#!/bin/bash
# usage: no args/options

touch out.txt
echo "" >> out.txt 2>&1
hwprefs -v cpu_count >> out.txt 2>&1
echo "" >> out.txt 2>&1
./restart_cluster.sh >> out.txt 2>&1
echo "" >> out.txt 2>&1
./load_shema.sh >> out.txt 2>&1
iostat 5 > iostat5.txt 2>&1 &
#vmstat 5 > vmstat5.txt 2>&1 &
pid=$!
echo "" >> out.txt 2>&1
( cd ../tws/tws_cpp/ ; make run.driver ) >> out.txt 2>&1
mkdir -p results/xxx
mv -v [a-z]*.txt results/xxx
mv -v ../tws/tws_cpp/log*.txt results/xxx
cp -v ../tws/*.properties results/xxx
cp -v ../config.ini results/xxx
cp -v ../my.cnf results/xxx
mv -v results/xxx  results/xxx_tws_cpp
sleep 6
kill -9 $pid
