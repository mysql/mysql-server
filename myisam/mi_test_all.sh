silent="-s"
suffix=$MACH
mi_test1$suffix $silent
myisamchk$suffix -se test1
mi_test1$suffix $silent -N -S
myisamchk$suffix -se test1
mi_test1$suffix $silent -P --checksum
myisamchk$suffix -se test1
mi_test1$suffix $silent -P -N -S
myisamchk$suffix -se test1
mi_test1$suffix $silent -B -N -R2
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -k 480 --unique
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -N -S -R1
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -S
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -S -N --unique
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -S -N --key_length=127 --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -S -N --key_length=128
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -S --key_length=480
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -B
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -B --key_length=64  --unique
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -B -k 480 --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -B -k 480 -N  --unique --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -m
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -m -P --unique --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -m -P --key_length=480 --key_cache
myisamchk$suffix -sm test1
mi_test1$suffix $silent -m -p
myisamchk$suffix -sm test1
mi_test1$suffix $silent -w -S --unique
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -w --key_length=64 --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -w -N --key_length=480
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -w -S --key_length=480 --checksum
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -b -N
myisamchk$suffix -sm test1
mi_test1$suffix $silent -a -b --key_length=480
myisamchk$suffix -sm test1
mi_test1$suffix $silent -p -B --key_length=480
myisamchk$suffix -sm test1

mi_test1$suffix $silent --checksum
myisamchk$suffix -se test1
myisamchk$suffix -rs test1
myisamchk$suffix -se test1
myisamchk$suffix -rqs test1
myisamchk$suffix -se test1

# check of myisampack / myisamchk
myisampack$suffix --force -s test1
myisamchk$suffix -es test1
myisamchk$suffix -rqs test1
myisamchk$suffix -es test1
myisamchk$suffix -rs test1
myisamchk$suffix -es test1
myisamchk$suffix -rus test1
myisamchk$suffix -es test1

mi_test1$suffix $silent --checksum -S
myisamchk$suffix -se test1
myisamchk$suffix -ros test1
myisamchk$suffix -rqs test1
myisamchk$suffix -se test1

myisampack$suffix --force -s test1
myisamchk$suffix -rqs test1
myisamchk$suffix -es test1
myisamchk$suffix -rus test1
myisamchk$suffix -es test1

mi_test1$suffix $silent --checksum --unique
myisamchk$suffix -se test1
mi_test1$suffix $silent --unique -S
myisamchk$suffix -se test1


mi_test1$suffix $silent --key_multiple -N -S
myisamchk$suffix -sm test1
mi_test1$suffix $silent --key_multiple -a -p --key_length=480
myisamchk$suffix -sm test1
mi_test1$suffix $silent --key_multiple -a -B --key_length=480
myisamchk$suffix -sm test1
mi_test1$suffix $silent --key_multiple -P -S
myisamchk$suffix -sm test1

mi_test2$suffix $silent -L -K -W -P
mi_test2$suffix $silent -L -K -W -P -A
mi_test2$suffix $silent -L -K -W -P -S -R1 -m500
echo "mi_test2$suffix $silent -L -K -R1 -m2000 ;  Should give error 135"
mi_test2$suffix $silent -L -K -R1 -m2000
mi_test2$suffix $silent -L -K -P -S -R3 -m50 -b1000000
mi_test2$suffix $silent -L -B
mi_test2$suffix $silent -D -B -c
mi_test2$suffix $silent -L -K -W -P -m50 -l
myisamlog$suffix
mi_test2$suffix $silent -L -K -W -P -m50 -l -b100
myisamlog$suffix
time mi_test2$suffix $silent
time mi_test2$suffix $silent -K -B
time mi_test2$suffix $silent -L -B
time mi_test2$suffix $silent -L -K -B
time mi_test2$suffix $silent -L -K -W -B
time mi_test2$suffix $silent -L -K -W -S -B
time mi_test2$suffix $silent -D -K -W -S -B
