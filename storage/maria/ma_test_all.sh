#!/bin/sh
#
# Execute some simple basic test on MyISAM libary to check if things
# works at all.

valgrind="valgrind --alignment=8 --leak-check=yes"
silent="-s"

if test -f ma_test1$MACH ; then suffix=$MACH ; else suffix=""; fi
./ma_test1$suffix $silent
./maria_chk$suffix -se test1
./ma_test1$suffix $silent -N -S
./maria_chk$suffix -se test1
./ma_test1$suffix $silent -P --checksum
./maria_chk$suffix -se test1
./ma_test1$suffix $silent -P -N -S
./maria_chk$suffix -se test1
./ma_test1$suffix $silent -B -N -R2
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -k 480 --unique
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -N -S -R1
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -S
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -S -N --unique
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -S -N --key_length=127 --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -S -N --key_length=128
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -S --key_length=480
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -B
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -B --key_length=64  --unique
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -B -k 480 --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -B -k 480 -N  --unique --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -m
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -m -P --unique --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -m -P --key_length=480 --key_cache
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -m -p
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -w -S --unique
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -w --key_length=64 --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -w -N --key_length=480
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -w -S --key_length=480 --checksum
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -b -N
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -a -b --key_length=480
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent -p -B --key_length=480
./maria_chk$suffix -sm test1

./ma_test1$suffix $silent --checksum
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

# check of maria_pack / maria_chk
./maria_pack$suffix --force -s test1
./maria_chk$suffix -es test1
./maria_chk$suffix -rqs test1
./maria_chk$suffix -es test1
./maria_chk$suffix -rs test1
./maria_chk$suffix -es test1
./maria_chk$suffix -rus test1
./maria_chk$suffix -es test1

./ma_test1$suffix $silent --checksum -S
./maria_chk$suffix -se test1
./maria_chk$suffix -ros test1
./maria_chk$suffix -rqs test1
./maria_chk$suffix -se test1

./maria_pack$suffix --force -s test1
./maria_chk$suffix -rqs test1
./maria_chk$suffix -es test1
./maria_chk$suffix -rus test1
./maria_chk$suffix -es test1

./ma_test1$suffix $silent --checksum --unique
./maria_chk$suffix -se test1
./ma_test1$suffix $silent --unique -S
./maria_chk$suffix -se test1


./ma_test1$suffix $silent --key_multiple -N -S
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent --key_multiple -a -p --key_length=480
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent --key_multiple -a -B --key_length=480
./maria_chk$suffix -sm test1
./ma_test1$suffix $silent --key_multiple -P -S
./maria_chk$suffix -sm test1

./ma_test2$suffix $silent -L -K -W -P
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -L -K -W -P -A
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -L -K -W -P -S -R1 -m500
echo "ma_test2$suffix $silent -L -K -R1 -m2000 ;  Should give error 135"
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -L -K -R1 -m2000
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -L -K -P -S -R3 -m50 -b1000000
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -L -B
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -D -B -c
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -m10000 -e8192 -K
./maria_chk$suffix -sm test2
./ma_test2$suffix $silent -m10000 -e16384 -E16384 -K -L
./maria_chk$suffix -sm test2

./ma_test2$suffix $silent -L -K -W -P -m50
./ma_test2$suffix $silent -L -K -W -P -m50 -b100
time ./ma_test2$suffix $silent
time ./ma_test2$suffix $silent -K -B
time ./ma_test2$suffix $silent -L -B
time ./ma_test2$suffix $silent -L -K -B
time ./ma_test2$suffix $silent -L -K -W -B
time ./ma_test2$suffix $silent -L -K -W -S -B
time ./ma_test2$suffix $silent -D -K -W -S -B
