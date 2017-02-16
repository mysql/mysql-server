#!/bin/bash

./hstest test=7 key_mask=9999 host=localhost port=11211 num=10000 \
	num_threads=10 timelimit=10 op=R $@
./hstest test=7 key_mask=9999 host=localhost port=11211 num=1000000 \
	num_threads=100 timelimit=10 op=G $@

