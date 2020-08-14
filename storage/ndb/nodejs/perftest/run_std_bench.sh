# Copyright (c) 2014, 2020, Oracle and/or its affiliates.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.

# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.

#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

LOGDIR='./benchmark_logs'
SUMMARYFILE="$LOGDIR/All-runs-summary.txt"

REVNO=`bzr revno`
[ -d $LOGDIR ] || mkdir $LOGDIR

DoRun() {
  ADAPTER=$1
  TIME=`date +%d%b%Y-%H%M%S`
  LOGFILE="$LOGDIR/bzr-$REVNO-$ADAPTER-$TIME.txt"
  Echo "Running $ADAPTER"
  node --expose-gc jscrund --adapter=$ADAPTER --modes=indy,bulk -r 8  | tee $LOGFILE
} 

Sum() {
  echo "## bzr: $REVNO  Adapter: $ADAPTER  Date: $TIME"
  tail $LOGFILE | Analyze
  echo ""
}


Analyze() {
  awk '
    func summarize() { for(i = 2 ; i < 8 ; i++) sums[i] += $i } 

    NR == 1 { print }
    NR == 6 { print; summarize(); }
    NR == 7 { print; summarize(); }
    NR == 8 { print; summarize(); }
    END     { printf("AVGS\t")
              for(i = 2 ; i < 8 ; i++) printf("%.1f\t", sums[i]/3);
              printf("\n")
              printf("TOTAL\tindy\t%d\n", sums[2]+sums[3]+sums[4])
              printf("TOTAL\tbulk\t%d\n", sums[5]+sums[6]+sums[7])
            } 
  '
}

DoRun ndb 
Sum | tee -a $SUMMARYFILE

# DoRun mysql
# Sum | tee -a $SUMMARYFILE

