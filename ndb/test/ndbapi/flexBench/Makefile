include .defs.mk

TYPE := ndbapitest

BIN_TARGET := flexBench

# Source files of non-templated classes (.C files)
SOURCES = flexBench.cpp

include $(NDB_TOP)/Epilogue.mk

# run automated tests
test:
	@echo "Results test-scripts flexScan." > report.txt
	@echo "==============================" >> report.txt
	@echo >> report.txt
	@echo "User : \t\t\c" >> report.txt
	@whoami >> report.txt
	@echo "Date : \t\t\c" >> report.txt
	@date >> report.txt
	@echo >> report.txt
	@echo "Host : \t\t\c" >> report.txt
	@uname -n >> report.txt
	@echo "\nStarting test 1."
	@echo "\n==============================\n" >> report.txt
	@test1.sh
	@echo "\nStarting test 2."
	@echo "\n==============================\n" >> report.txt
	@test2.sh
	@echo "\nStarting test 3."
	@echo "\n==============================\n" >> report.txt
	@test3.sh

# run basic tests
run_test :
	flexBench
	flexBench -t 32 -o 5000
	flexBench -simple
	flexBench -simple -t 32
	flexBench -dirty
	flexBench -dirty -t 32
	flexBench -write 
	flexBench -write -t 32
	flexBench -dirty -write
	flexBench -dirty -write -t 32
	flexBench -c 16 -t 32 -o 5000
	flexBench -t 16 -c 30 -o 10000
