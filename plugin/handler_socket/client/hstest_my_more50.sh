#!/bin/bash
exec ./hstest test=9 key_mask=9999 host=localhost port=3306 num=1000000 \
	num_threads=100 verbose=1 timelimit=10 moreflds=50 $@
