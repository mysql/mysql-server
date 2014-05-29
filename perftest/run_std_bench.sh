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

