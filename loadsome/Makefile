CFLAGS = -W -Wall -O2
serial.%.50K: makedata-serial
	./makedata-serial $(patsubst serial.%.50K, %, $@) 50000 > $@

serial.%.300K: makedata-serial
	./makedata-serial $(patsubst serial.%.300K, %, $@) 300000 > $@

random.1.300K:
	./makedata-random 1000 600000 1 > random.1.300K

random.2.300K:
	./makedata-random 1000 900000 2 > random.2.300K

random.9.300K:
	./makedata-random 1000 3000000 9 > random.9.300K

random.19.300K:
	./makedata-random 1000 6000000 19 > random.19.300K
