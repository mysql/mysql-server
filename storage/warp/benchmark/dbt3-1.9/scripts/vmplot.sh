#!/bin/sh

#
# This file is released under the terms of the Artistic License.
# Please see the file LICENSE, included in this package, for details.
#
# Copyright (C) 2004 Mark Wong & Open Source Development Lab, Inc.
#

INFILE="vmstat.out"
OUTDIR="/tmp"
DATAFILE="vmstat.data"

X_UNITS="Minutes"

while getopts "i:o:x:" opt; do
	case $opt in
		i)
			INFILE=$OPTARG
			;;
		o)
			OUTDIR=$OPTARG
			;;
		x)
			X_UNITS=$OPTARG
			;;
	esac
done

if [ ! -f "$INFILE" ]; then
	echo "$INFILE does not exist."
	exit 1
fi

# Blindly create the output directory.
mkdir -p $OUTDIR

# This is based off vmstat with a header like:

#procs -----------memory---------- ---swap-- -----io---- --system-- ----cpu----
# r  b   swpd   free   buff  cache   si   so    bi    bo   in    cs us sy id wa

# Make 0 the first point for each graph
# Also add another column at the end to represent total processor utilization.
echo "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" > ${OUTDIR}/vmstat.data
cat ${INFILE} | grep -v '^procs ' | grep -v '^ r  b ' | awk '{ print NR, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $13+$14+$16 }' >> ${OUTDIR}/${DATAFILE}

# Plot the procs information.
NAME="procs"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:2 title \"waiting for run time\" with lines, \\" > ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:3 title \"in uninterruptible sleep\" with lines" >> ${OUTDIR}/${INPUT_FILE}
echo "set title \"Procs\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"Count\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the memory information.
NAME="memory"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:4 title \"Swapped\" with lines, \\" > ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:5 title \"Free\" with lines, \\" >> ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:6 title \"Buffers\" with lines, \\" >> ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:7 title \"Cache\" with lines" >> ${OUTDIR}/${INPUT_FILE}
echo "set title \"Memory\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"Kilobytes\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the swap information.
NAME="swap"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:8 title \"in from disk\" with lines, \\" > ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:9 title \"out to disk\" with lines" >> ${OUTDIR}/${INPUT_FILE}
echo "set title \"Swap\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"Kilobytes / Second\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the i/o information.
NAME="io"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:10 title \"received from device\" with lines, \\" > ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:11 title \"sent to device\" with lines" >> ${OUTDIR}/${INPUT_FILE}
echo "set title \"IO\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"Blocks per Second\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the interrupt.
NAME="in"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:12 title \"interrupts\" with lines" > ${OUTDIR}/${INPUT_FILE}
echo "set title \"Interrupts\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"# of Interrupts / Second\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the interrupt.
NAME="cs"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:13 title \"context switches\" with lines" > ${OUTDIR}/${INPUT_FILE}
echo "set title \"Context Switches\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"# of Context Switches / Second\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

# Plot the processor utilization.
NAME="cpu"
INPUT_FILE="${NAME}.input"
PNG_FILE="${NAME}.png"
echo "plot \"${DATAFILE}\" using 1:18 title \"total\" with lines, \\" > ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:14 title \"user\" with lines, \\" >> ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:15 title \"system\" with lines, \\" >> ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:16 title \"idle\" with lines, \\" >> ${OUTDIR}/${INPUT_FILE}
echo "     \"${DATAFILE}\" using 1:17 title \"wait\" with lines" >> ${OUTDIR}/${INPUT_FILE}
echo "set title \"System Processor Utilization\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set grid xtics ytics" >> ${OUTDIR}/${INPUT_FILE}
echo "set xlabel \"Elapsed Time (${X_UNITS})\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set ylabel \"% Utilized\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set term png small" >> ${OUTDIR}/${INPUT_FILE}
echo "set output \"${PNG_FILE}\"" >> ${OUTDIR}/${INPUT_FILE}
echo "set yrange [0:100]" >> ${OUTDIR}/${INPUT_FILE}
echo "replot" >> ${OUTDIR}/${INPUT_FILE}
(cd ${OUTDIR}; gnuplot ${INPUT_FILE})

exit 0
