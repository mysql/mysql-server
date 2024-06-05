#!/bin/bash

# Copyright (c) 2003, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

find result -name 'core*' -print0 | while read -d $'\0' core; do
  "$(dirname $0)/atrt-backtrace.sh" "${core}"
done

# Log files may be left between test runs so we need to keep some
# state about what results we have seen.

STATEFILE=atrt-analyze-result.state
OLDOK=0
OLDSKIP=0
OLDFAIL=0

# Checksum of beginning of log file is used to determine if log file
# is reused or not.  If it is reused the count of seen OK reports
# and FAILED reports are updated.
if [ -f "${STATEFILE}" ] ; then
  while read file oks skips fails chars sum ; do
    chk=`dd 2>/dev/null if="$file" bs="${chars}" count=1 | sum`
    if [ "${chk}" = "${sum}" ] ; then
      OLDOK=`expr ${OLDOK} + ${oks}`
      OLDSKIP=`expr ${OLDSKIP} + ${skips}`
      OLDFAIL=`expr ${OLDFAIL} + ${fails}`
    fi
  done < "${STATEFILE}"
fi

# List of log files with result report.
LOGFILES=`find result/ -name log.out -size +0c | xargs grep -l 'NDBT_ProgramExit: ' /dev/null`

# Save the number of OK reports and FAILED and checksum  per log file
OK=0
SKIP=0
FAIL=0
for file in ${LOGFILES} ; do
  oks=`grep -c 'NDBT_ProgramExit: .*OK' "${file}"`
  skips=`grep -c 'NDBT_ProgramExit: .*Skipped' "${file}"`
  fails=`grep -c 'NDBT_ProgramExit: .*Failed' "${file}"`
  if [ $oks -gt 0 ] || [ $skips -gt 0 ] || [ $fails -gt 0 ]; then
    OK=`expr ${OK} + ${oks}`
    SKIP=`expr ${SKIP} + ${skips}`
    FAIL=`expr ${FAIL} + ${fails}`
    chars=`wc -c < "${file}" | awk '{ print $1 }'`
    sum=`dd 2>/dev/null if="$file" bs="${chars}" count=1 | sum`
    echo "${file}" "${oks}" "${skips}" "${fails}" "${chars}" "${sum}"
  fi
done > "${STATEFILE}"

if [ ${FAIL} -gt ${OLDFAIL} ]; then
  RC=1  # NDBT_FAILED
elif [ ${SKIP} -gt ${OLDSKIP} ]; then
  RC=4  # NDBT_SKIPPED
elif [ ${OK} -gt ${OLDOK} ]; then
  RC=0  # NDBT_OK
else
  # Return failure if no status was found
  RC=1  # NDBT_FAILED
fi

exit ${RC}
