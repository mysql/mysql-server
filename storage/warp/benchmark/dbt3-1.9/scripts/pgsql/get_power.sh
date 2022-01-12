#!/bin/bash

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2003 Jenny Zhang & Open Source Development Lab, Inc.
#

#
# May 26, 2005
# Mark Wong
# Rewritten from perl to bash.
#

usage()
{
	echo "usage: get_power.sh -i <q_time.out> -p <perfrun> -s <scale factor> [-v]"
}

while getopts "i:p:s:v" OPT; do
	case ${OPT} in
	i)
		INFILE=${OPTARG}
		;;
	p)
		PERFNUM=${OPTARG}
		;;
	s)
		SCALE_FACTOR=${OPTARG}
		;;
	v)
		set -x
		SHELL="${SHELL} +x"
		;;
	esac
done

if [ -z ${INFILE} ] || [ -z ${PERFNUM} ] || [ -z ${SCALE_FACTOR} ]; then
	usage
	exit 1
fi

#
# Get execution time for each of the power queries.
#
for i in `seq 1 22`; do
	VAL=`grep "PERF${PERFNUM}.POWER.Q${i} " ${INFILE} | awk '{ print $11 }'`
	if [ -z ${VAL} ]; then
		echo "No power data retrieved for 'PERF${PERFNUM}.POWER.Q${i}'."
		exit 1
	fi
	VALUES="${VALUES} ${VAL}"
done 

#
# Get execution time for the each of the refresh functions.
#
for i in `seq 1 2`; do
	VAL=`grep "PERF${PERFNUM}.POWER.RF${i}" ${INFILE} | awk '{ print $11 }'`
	if [ -z ${VAL} ]; then
		echo "No power data retrieved for 'PERF${PERFNUM}.POWER.RF${i}'."
		exit 1
	fi
	#
	# in case the refresh functions finished within 1 second
	#
	if [ "x${VAL}" = "x0" ]; then
		VAL=1
	fi
	VALUES="${VALUES} ${VAL}"
done 

${POWER} ${SCALE_FACTOR} ${VALUES} || exit 1

exit 0
