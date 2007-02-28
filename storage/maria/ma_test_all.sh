#!/bin/sh
#
# Execute some simple basic test on MyISAM libary to check if things
# works at all.

valgrind="valgrind --alignment=8 --leak-check=yes"
silent="-s"
suffix=""
#set -x -v -e

run_tests()
{
  row_type=$1
  #
  # First some simple tests
  #
  ./ma_test1$suffix $silent $row_type
  ./maria_chk$suffix -se test1
  ./ma_test1$suffix $silent -N $row_type
  ./maria_chk$suffix -se test1
  ./ma_test1$suffix $silent -P --checksum $row_type
  ./maria_chk$suffix -se test1
  ./ma_test1$suffix $silent -P -N $row_type
  ./maria_chk$suffix -se test1
  ./ma_test1$suffix $silent -B -N -R2 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -k 480 --unique $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -N -R1 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p -N --unique $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p -N --key_length=127 --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p -N --key_length=128 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -B $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -B --key_length=64  --unique $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -B -k 480 --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -B -k 480 -N  --unique --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -m $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -m -P --unique --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -m -P --key_length=480 --key_cache $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -m -p $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -w --unique $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -w --key_length=64 --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -w -N --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -w --key_length=480 --checksum $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -b -N $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -a -b --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent -p -B --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent --checksum --unique $row_type
  ./maria_chk$suffix -se test1
  ./ma_test1$suffix $silent --unique $row_type
  ./maria_chk$suffix -se test1
    
  ./ma_test1$suffix $silent --key_multiple -N -S $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent --key_multiple -a -p --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent --key_multiple -a -B --key_length=480 $row_type
  ./maria_chk$suffix -sm test1
  ./ma_test1$suffix $silent --key_multiple -P -S $row_type
  ./maria_chk$suffix -sm test1
 
  ./maria_pack$suffix --force -s test1
  ./maria_chk$suffix -ess test1
  
  ./ma_test2$suffix $silent -L -K -W -P $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -L -K -W -P -A $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -L -K -P -R3 -m50 -b1000000 $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -L -B $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -D -B -c $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -m10000 -e4096 -K $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -m10000 -e8192 -K $row_type
  ./maria_chk$suffix -sm test2
  ./ma_test2$suffix $silent -m10000 -e16384 -E16384 -K -L $row_type
  ./maria_chk$suffix -sm test2
}

run_repair_tests()
{
  row_type=$1
  ./ma_test1$suffix $silent --checksum $row_type
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -rs test1
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -rqs test1
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -rs --correct-checksum test1
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -rqs --correct-checksum test1
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -ros --correct-checksum test1
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -rqos --correct-checksum test1
  ./maria_chk$suffix -se test1
}

run_pack_tests()
{
  row_type=$1
  # check of maria_pack / maria_chk
  ./ma_test1$suffix $silent --checksum $row_type
  ./maria_pack$suffix --force -s test1
  ./maria_chk$suffix -ess test1
  ./maria_chk$suffix -rqs test1
  ./maria_chk$suffix -es test1
  ./maria_chk$suffix -rs test1
  ./maria_chk$suffix -es test1
  ./maria_chk$suffix -rus test1
  ./maria_chk$suffix -es test1
  
  ./ma_test1$suffix $silent --checksum -S $row_type
  ./maria_chk$suffix -se test1
  ./maria_chk$suffix -ros test1
  ./maria_chk$suffix -rqs test1
  ./maria_chk$suffix -se test1
  
  ./maria_pack$suffix --force -s test1
  ./maria_chk$suffix -rqs test1
  ./maria_chk$suffix -es test1
  ./maria_chk$suffix -rus test1
  ./maria_chk$suffix -es test1
}

echo "Running tests with dynamic row format"
run_tests ""
run_repair_tests ""
run_pack_tests ""

echo "Running tests with static row format"
run_tests -S
run_repair_tests -S
run_pack_tests -S

echo "Running tests with block row format"
run_tests -M

#
# Tests that gives warnings
#

./ma_test2$suffix $silent -L -K -W -P -S -R1 -m500
./maria_chk$suffix -sm test2
echo "ma_test2$suffix $silent -L -K -R1 -m2000 ;  Should give error 135"
./ma_test2$suffix $silent -L -K -R1 -m2000
echo "./maria_chk$suffix -sm test2 will warn that 'Datafile is almost full'"
./maria_chk$suffix -sm test2
./maria_chk$suffix -ssm test2

#
# Some timing tests
#
time ./ma_test2$suffix $silent
time ./ma_test2$suffix $silent -S
time ./ma_test2$suffix $silent -M
time ./ma_test2$suffix $silent -B
time ./ma_test2$suffix $silent -L
time ./ma_test2$suffix $silent -K
time ./ma_test2$suffix $silent -K -B
time ./ma_test2$suffix $silent -L -B
time ./ma_test2$suffix $silent -L -K -B
time ./ma_test2$suffix $silent -L -K -W -B
time ./ma_test2$suffix $silent -L -K -W -B -S
time ./ma_test2$suffix $silent -L -K -W -B -M
time ./ma_test2$suffix $silent -D -K -W -B -S
