#!/usr/bin/perl

use strict;
use warnings;

use Getopt::Long;
use File::Basename;
use IO::Handle;

# Setup to find modules in lib
use Cwd ();
use File::Spec ();
use lib File::Spec->catdir(File::Basename::dirname(Cwd::abs_path __FILE__),
                           '../../lib');
use My::Config;

my $vardir = $ENV{MYSQLTEST_VARDIR} || "/tmp";
my $testfile = $ENV{MYSQLTEST_FILE};
if (defined $testfile) {
  # Redirect to var/log/report_<testname>.log based on the mysqltest file name
  my $testname = fileparse($testfile, ".test");
  my $logfilename = "$vardir/log/report_$testname.log";
  open(STDOUT, '>', $logfilename) or die "Can't redirect STDOUT: $!";
}

STDOUT->autoflush(1);

print "# args: ", join(" ", @ARGV), "\n";

my @opt_ports;
my @opt_sockets;

# Populate list of ports and sockets from my.cnf, this will make use of
# all configured mysqld(s) unless overridden by arguments given by caller.
my $mycnf = "$vardir/my.cnf";
if (-f $mycnf) {
  my $config = My::Config->new($mycnf);

  foreach my $mysqld ($config->like("mysqld.")) {

    # Don't use binlogging mysqld(s) as client
    if ($mysqld->if_exist('ndb-log-bin')) {
      print "# - skip " . $mysqld->name() . " since it's binlogging\n";
      next;
    }

    # Hardcode second to not be used as well
    if ($mysqld->name() eq 'mysqld.2.crunch') {
      print "# - skip also " . $mysqld->name() . "\n";
      next;
    }

    push(@opt_ports, $mysqld->value('port'));
    push(@opt_sockets, $mysqld->value('socket'));
  }
}
my $opt_bench_name; # Benchmark name (aka. "testname")
my $opt_engine = $ENV{CRUNCH_ENGINE} || "ndbcluster";
my $opt_warmup = 0;
my $opt_tables = 8;
my $opt_rows = $ENV{CRUNCH_ROWS} || 10000000; # 10M
my $opt_time = $ENV{CRUNCH_TIME} || 30; # seconds
my $opt_events = $ENV{CRUNCH_EVENTS} || 0; # => run until time elapsed
my $opt_threads = $ENV{CRUNCH_THREADS}; # Use specific threads number
my $opt_report_interval = 1;  # seconds
my $opt_verbosity = 3; # Default of sysbench is 3
my $opt_debug;
# Turn off AUTO_INCREMENT primary key by default
my $opt_autoinc = $ENV{CRUNCH_AUTOINC} || 0;
my $opt_secondary_index = 0;
# Use uniform random numbers by default (this is the default on current
# sysbench version so only useful when using older sysbench), this changes
# transactions to be distributed over all rows in the tables and thus reduces
# deadlocks. This is important since deadlocks in NDB are detected using
# timeout and synchronously executing clients will be stalled meanwhile,
# thus reducing throughput
my $rand_type = "uniform";

Getopt::Long::Configure("pass_through", "no_auto_abbrev");
GetOptions(
  'port=i'              => \@opt_ports,
  'socket=s'            => \@opt_sockets,
  'benchmark=s'         => \$opt_bench_name,
  'engine=s'            => \$opt_engine,
  'warmup'              => \$opt_warmup,
  'tables=i'            => \$opt_tables,
  'rows=i'              => \$opt_rows,
  'events=i'            => \$opt_events,
  'time=i'              => \$opt_time,
  'threads=i'           => \$opt_threads,
  'autoinc'             => \$opt_autoinc,
  'secondary'           => \$opt_secondary_index,
  'verbose=i'           => \$opt_verbosity,
  'debug'               => \$opt_debug,
  'report-interval=i'   => \$opt_report_interval
) or die "Could not read arguments";

my $sb = "sysbench";
# Check that sysbench exists
my $bench_check = `$sb --version 2>&1`;
die "Could not execute '$sb'" if ($? == -1);

# Check parameters
die "Need --port for MySQL Server" unless @opt_ports > 0;
die "Need benchmark name" unless $opt_bench_name;

my @args;
push(@args, "--verbosity=$opt_verbosity") if $opt_verbosity != 3;
push(@args, "--debug=on") if defined $opt_debug;

# Connection parameters
push(@args, "--db-driver=mysql");
push(@args, "--mysql-storage-engine=$opt_engine");
push(@args, "--mysql-host=localhost");
push(@args, "--mysql-port=" . join (",", @opt_ports));
push(@args, "--mysql-socket=" . join (",", @opt_sockets)) if @opt_sockets > 0;
push(@args, "--mysql-user=root");
push(@args, "--mysql-password=");
push(@args, "--report-interval=$opt_report_interval");

# Benchmark parameters
push(@args, "--tables=$opt_tables");
push(@args, "--table_size=$opt_rows");
push(@args, "--events=$opt_events");
push(@args, "--time=$opt_time");

# Benchmark tuning
push(@args, "--auto_inc=$opt_autoinc");
push(@args, "--rand-type=$rand_type");
push(@args, "--create-secondary=$opt_secondary_index");

push(@args, @ARGV);

print_header("Prepare");
bench($opt_bench_name, "prepare", @args);

if ($opt_warmup) {
  print_header("Warmup");
  bench($opt_bench_name, "warmup", @args);
}

foreach my $thread ( threads() ) {
  my $threads = "--threads=$thread";
  print_header("Run $threads");
  bench($opt_bench_name, "run", @args, $threads);
}

print_header("Cleanup");
bench($opt_bench_name, "cleanup", @args);

exit(0);

sub threads {
  my @threads;
  # Only one specific thread
  if ($opt_threads) {
    return ($opt_threads);
  }

  # Geometric serie of threads
  for (my $i = 1; $i <= 1024; $i = $i*2) {
    push(@threads, $i);
  }
  return @threads;
}

sub bench {
    print "running: '$sb ", join(' ', @_), "'\n";
    system($sb, @_)
        and print "command failed: $!\n" and exit(1);
}

sub print_header {
  my ($what)= @_;
  print "\n";
  print "# ######################################################\n";
  print "#  $what\n";
  print "# ######################################################\n";
}
