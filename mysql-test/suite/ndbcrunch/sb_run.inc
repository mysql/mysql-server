# Run sb_run.pl with given parameters

# Select the benchmark name
let $args = --benchmark=$bench_name;

# Benchmark parameters
if ($bench_tables) {
  let $args = $args --tables=$bench_tables;
}
if ($bench_threads) {
  let $args = $args --threads=$bench_threads;
}
if ($bench_secondary_index) {
  let $args = $args --secondary=$bench_secondary_index;
}
if ($bench_args) {
  let $args = $args $bench_args;
}
#echo args: $args;

--exec perl $MYSQL_TEST_DIR/suite/ndbcrunch/sb_run.pl $args

# Reset args
let $benchmark=;
let $bench_tables=;
let $bench_threads=;
let $bench_args=;
