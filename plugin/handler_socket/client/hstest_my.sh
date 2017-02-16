#!/bin/bash
exec ./hstest test=9 tablesize=9999 host=localhost mysqlport=3306 num=1000000 \
	num_threads=100 verbose=1 timelimit=10 $@
