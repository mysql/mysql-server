#!/bin/bash

exec ./hstest test=10 key_mask=9999 host=localhost port=9998 num=10000000 \
	num_threads=100 timelimit=10 moreflds=50 $@
