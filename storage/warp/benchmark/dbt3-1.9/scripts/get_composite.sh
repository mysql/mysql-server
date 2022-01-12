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

while getopts "i:n:o:p:s:v" OPT; do
	case ${OPT} in
	i)
		INFILE=${OPTARG}
		;;
	n)
		STREAMS=${OPTARG}
		;;
	o)
		OUTFILE=${OPTARG}
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

SCRIPT_DIR="/home/justin/warp/storage/warp/benchmark/dbt3-1.9/scripts/pgsql";

POWER=`${SHELL} ${SCRIPT_DIR}/get_power.sh -i ${INFILE} -p ${PERFNUM} -s ${SCALE_FACTOR}`
THROUGHPUT=`${SHELL} ${SCRIPT_DIR}/get_throughput.sh -i ${INFILE} -p ${PERFNUM} -s ${SCALE_FACTOR} -n ${STREAMS}`
COMPOSITE=`echo "scale=2; sqrt(${POWER} * ${THROUGHPUT})" | bc -l`

echo "power = ${POWER}" > ${OUTFILE}
echo "throughput = ${THROUGHPUT}" >> ${OUTFILE}
echo "composite = ${COMPOSITE}" >> ${OUTFILE}

exit 0
