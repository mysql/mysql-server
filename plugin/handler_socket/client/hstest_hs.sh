#!/bin/bash

exec ./hstest test=10 tablesize=10000 host=localhost hsport=9998 num=10000000 \
	num_threads=100 timelimit=10 $@
