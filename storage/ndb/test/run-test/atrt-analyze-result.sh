#!/bin/bash

# Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.
# Use is subject to license terms
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

cores=`find result -name 'core*'`
if [ "$cores" ]
then
    for i in "$cores"
    do
	atrt-backtrace.sh $i
    done
fi

# Log files may be left between test runs so we need to keep some
# state about what results we have seen.

STATEFILE=atrt-analyze-result.state
OLDOK=0

# Checksum of beginning of log file is used to determine if log file
# is reused or not.  If it is reused the count of seen OK reports
# are updated.
if [ -f "${STATEFILE}" ] ; then
  while read file oks chars sum ; do
    chk=`dd 2>/dev/null if="$file" bs="${chars}" count=1 | sum`
    if [ "${chk}" = "${sum}" ] ; then
      OLDOK=`expr ${OLDOK} + ${oks}`
    fi
  done < "${STATEFILE}"
fi

# List of log files with result report.
LOGFILES=`find result/ -name log.out -size +0c | xargs grep -l 'NDBT_ProgramExit: ' /dev/null`

# Save the number of OK reports and checksum  per log file
OK=0
for file in ${LOGFILES} ; do
  oks=`grep -c 'NDBT_ProgramExit: .*OK' "${file}"`
  if [ $oks -gt 0 ] ; then
    OK=`expr ${OK} + ${oks}`
    chars=`wc -c < "${file}" | awk '{ print $1 }'`
    sum=`dd 2>/dev/null if="$file" bs="${chars}" count=1 | sum`
    echo "${file}" "${oks}" "${chars}" "${sum}"
  fi
done > "${STATEFILE}"

# Succeed if found a new OK
[ ${OK} -gt ${OLDOK} ]
