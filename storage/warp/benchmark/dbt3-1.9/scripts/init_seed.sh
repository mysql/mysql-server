#!/bin/sh

#generate the initial seed according to cluse 2.1.3.3
#the initial seed is the time stamp of the end of the database load time
#expressed in the format mmddhhmms.

date +%-m%d%H%M%S
