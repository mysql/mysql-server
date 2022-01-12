#!/bin/bash

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2003 Jenny Zhang & Open Source Development Lab, Inc.
#

#
# May 27, 2005
# Mark Wong
# Rewritten from perl to bash.
#

usage()
{
	echo "usage: get_throughput.sh -i <q_time.out> -n <streams> -p <perfrun>"
	echo "           -s <scale factor> [-v]"
}

while getopts "i:n:p:s:v" OPT; do
	case ${OPT} in
	i)
		INFILE=${OPTARG}
		;;
	n)
		STREAMS=${OPTARG}
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

if [ -z ${INFILE} ] || [ -z ${STREAMS} ] || [ -z ${PERFNUM} ] || [ -z ${SCALE_FACTOR} ]; then
	usage
	exit 1
fi

#
# Get the execution time for the throughput test.
#
THROUGHPUT=`grep "PERF${PERFNUM}.THRUPUT " ${INFILE} | awk '{ print $11 }'`
if [ -z ${THROUGHPUT} ]; then
	echo "No throughput data retrieved from database."
	exit 1
fi

THROUGHPUT=`echo "scale=2; 22 * 3600 * ${STREAMS} * ${SCALE_FACTOR} / ${THROUGHPUT}" | bc -l`
echo ${THROUGHPUT}

exit 0
