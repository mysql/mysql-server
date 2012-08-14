#!/usr/bin/perl

# Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
# MA 02110-1301, USA

use strict;
use Getopt::Long;
use Data::Dumper;
use File::Basename;
use File::Path;
use DBI;
use Sys::Hostname;
use File::Copy;
use File::Temp qw(tempfile);

=head1 NAME

mysqlhotcopy - fast on-line hot-backup utility for local MySQL databases and tables

=head1 SYNOPSIS

  mysqlhotcopy db_name

  mysqlhotcopy --suffix=_copy db_name_1 ... db_name_n

  mysqlhotcopy db_name_1 ... db_name_n /path/to/new_directory

  mysqlhotcopy db_name./regex/

  mysqlhotcopy db_name./^\(foo\|bar\)/

  mysqlhotcopy db_name./~regex/

  mysqlhotcopy db_name_1./regex_1/ db_name_1./regex_2/ ... db_name_n./regex_n/ /path/to/new_directory

  mysqlhotcopy --method='scp -Bq -i /usr/home/foo/.ssh/identity' --user=root --password=secretpassword \
         db_1./^nice_table/ user@some.system.dom:~/path/to/new_directory

WARNING: THIS PROGRAM IS STILL IN BETA. Comments/patches welcome.

=cut

# Documentation continued at end of file

my $VERSION = "1.23";

my $opt_tmpdir = $ENV{TMPDIR} || "/tmp";

my $OPTIONS = <<"_OPTIONS";

$0 Ver $VERSION

Usage: $0 db_name[./table_regex/] [new_db_name | directory]

  -?, --help           display this help-screen and exit
  -u, --user=#         user for database login if not current user
  -p, --password=#     password to use when connecting to server (if not set
                       in my.cnf, which is recommended)
  -h, --host=#         hostname for local server when connecting over TCP/IP
  -P, --port=#         port to use when connecting to local server with TCP/IP
  -S, --socket=#       socket to use when connecting to local server
      --old_server     connect to old MySQL-server (before v5.5) which
                       doesn't have FLUSH TABLES WITH READ LOCK fully implemented.

  --allowold           don\'t abort if target dir already exists (rename it _old)
  --addtodest          don\'t rename target dir if it exists, just add files to it
  --keepold            don\'t delete previous (now renamed) target when done
  --noindices          don\'t include full index files in copy
  --method=#           method for copy (only "cp" currently supported)

  -q, --quiet          be silent except for errors
  --debug              enable debug
  -n, --dryrun         report actions without doing them

  --regexp=#           copy all databases with names matching regexp
  --suffix=#           suffix for names of copied databases
  --checkpoint=#       insert checkpoint entry into specified db.table
  --flushlog           flush logs once all tables are locked 
  --resetmaster        reset the binlog once all tables are locked
  --resetslave         reset the master.info once all tables are locked
  --tmpdir=#	       temporary directory (instead of $opt_tmpdir)
  --record_log_pos=#   record slave and master status in specified db.table
  --chroot=#           base directory of chroot jail in which mysqld operates

  Try \'perldoc $0\' for more complete documentation
_OPTIONS

sub usage {
    die @_, $OPTIONS;
}

# Do not initialize user or password options; that way, any user/password
# options specified in option files will be used.  If no values are specified
# at all, the defaults will be used (login name, no password).

my %opt = (
    noindices	=> 0,
    allowold	=> 0,	# for safety
    keepold	=> 0,
    method	=> "cp",
    flushlog    => 0,
);
Getopt::Long::Configure(qw(no_ignore_case)); # disambiguate -p and -P
GetOptions( \%opt,
    "help",
    "host|h=s",
    "user|u=s",
    "password|p=s",
    "port|P=s",
    "socket|S=s",
    "old_server",
    "allowold!",
    "keepold!",
    "addtodest!",
    "noindices!",
    "method=s",
    "debug",
    "quiet|q",
    "mv!",
    "regexp=s",
    "suffix=s",
    "checkpoint=s",
    "record_log_pos=s",
    "flushlog",
    "resetmaster",
    "resetslave",
    "tmpdir|t=s",
    "dryrun|n",
    "chroot=s",
) or usage("Invalid option");

# @db_desc
# ==========
# a list of hash-refs containing:
#
#   'src'     - name of the db to copy
#   't_regex' - regex describing tables in src
#   'target'  - destination directory of the copy
#   'tables'  - array-ref to list of tables in the db
#   'files'   - array-ref to list of files to be copied
#   'index'   - array-ref to list of indexes to be copied
#

my @db_desc = ();
my $tgt_name = undef;

usage("") if ($opt{help});

if ( $opt{regexp} || $opt{suffix} || @ARGV > 2 ) {
    $tgt_name   = pop @ARGV unless ( exists $opt{suffix} );
    @db_desc = map { s{^([^\.]+)\./(.+)/$}{$1}; { 'src' => $_, 't_regex' => ( $2 ? $2 : '.*' ) } } @ARGV;
}
else {
    usage("Database name to hotcopy not specified") unless ( @ARGV );

    $ARGV[0] =~ s{^([^\.]+)\./(.+)/$}{$1};
    @db_desc = ( { 'src' => $ARGV[0], 't_regex' => ( $2 ? $2 : '.*' ) } );

    if ( @ARGV == 2 ) {
	$tgt_name   = $ARGV[1];
    }
    else {
	$opt{suffix} = "_copy";
    }
}

my %mysqld_vars;
my $start_time = time;
$opt_tmpdir= $opt{tmpdir} if $opt{tmpdir};
$0 = $1 if $0 =~ m:/([^/]+)$:;
$opt{quiet} = 0 if $opt{debug};
$opt{allowold} = 1 if $opt{keepold};

# --- connect to the database ---
my $dsn;
$dsn  = ";host=" . (defined($opt{host}) ? $opt{host} : "localhost");
$dsn .= ";port=$opt{port}" if $opt{port};
$dsn .= ";mysql_socket=$opt{socket}" if $opt{socket};

# use mysql_read_default_group=mysqlhotcopy so that [client] and
# [mysqlhotcopy] groups will be read from standard options files.

my $dbh = DBI->connect("dbi:mysql:$dsn;mysql_read_default_group=mysqlhotcopy",
                        $opt{user}, $opt{password},
{
    RaiseError => 1,
    PrintError => 0,
    AutoCommit => 1,
});

# --- check that checkpoint table exists if specified ---
if ( $opt{checkpoint} ) {
    $opt{checkpoint} = quote_names( $opt{checkpoint} );
    eval { $dbh->do( qq{ select time_stamp, src, dest, msg 
			 from $opt{checkpoint} where 1 != 1} );
       };

    die "Error accessing Checkpoint table ($opt{checkpoint}): $@"
      if ( $@ );
}

# --- check that log_pos table exists if specified ---
if ( $opt{record_log_pos} ) {
    $opt{record_log_pos} = quote_names( $opt{record_log_pos} );

    eval { $dbh->do( qq{ select host, time_stamp, log_file, log_pos, master_host, master_log_file, master_log_pos
			 from $opt{record_log_pos} where 1 != 1} );
       };

    die "Error accessing log_pos table ($opt{record_log_pos}): $@"
      if ( $@ );
}

# --- get variables from database ---
my $sth_vars = $dbh->prepare("show variables like 'datadir'");
$sth_vars->execute;
while ( my ($var,$value) = $sth_vars->fetchrow_array ) {
    $mysqld_vars{ $var } = $value;
}
my $datadir = $mysqld_vars{'datadir'}
    || die "datadir not in mysqld variables";
    $datadir= $opt{chroot}.$datadir if ($opt{chroot});
$datadir =~ s:/$::;


# --- get target path ---
my ($tgt_dirname, $to_other_database);
$to_other_database=0;
if (defined($tgt_name) && $tgt_name =~ m:^\w+$: && @db_desc <= 1)
{
    $tgt_dirname = "$datadir/$tgt_name";
    $to_other_database=1;
}
elsif (defined($tgt_name) && ($tgt_name =~ m:/: || $tgt_name eq '.')) {
    $tgt_dirname = $tgt_name;
}
elsif ( $opt{suffix} ) {
    print "Using copy suffix '$opt{suffix}'\n" unless $opt{quiet};
}
else
{
  $tgt_name="" if (!defined($tgt_name));
  die "Target '$tgt_name' doesn't look like a database name or directory path.\n";
}

# --- resolve database names from regexp ---
if ( defined $opt{regexp} ) {
    my $t_regex = '.*';
    if ( $opt{regexp} =~ s{^/(.+)/\./(.+)/$}{$1} ) {
        $t_regex = $2;
    }

    my $sth_dbs = $dbh->prepare("show databases");
    $sth_dbs->execute;
    while ( my ($db_name) = $sth_dbs->fetchrow_array ) {
	next if $db_name =~ m/^information_schema$/i;
	push @db_desc, { 'src' => $db_name, 't_regex' => $t_regex } if ( $db_name =~ m/$opt{regexp}/o );
    }
}

# --- get list of tables and views to hotcopy ---

my $hc_locks = "";
my $hc_tables = "";
my $hc_base_tables = "";
my $hc_views = "";
my $num_base_tables = 0;
my $num_views = 0;
my $num_tables = 0;
my $num_files = 0;

foreach my $rdb ( @db_desc ) {
    my $db = $rdb->{src};
    my @dbh_base_tables = get_list_of_tables( $db );
    my @dbh_views = get_list_of_views( $db );

    ## filter out certain system non-lockable tables. 
    ## keep in sync with mysqldump.
    if ($db =~ m/^mysql$/i)
    {
      @dbh_base_tables = grep 
        { !/^(apply_status|schema|general_log|slow_log)$/ } @dbh_base_tables
    }

    ## generate regex for tables/files
    my $t_regex;
    my $negated;
    if ($rdb->{t_regex}) {
        $t_regex = $rdb->{t_regex};        ## assign temporary regex
        $negated = $t_regex =~ s/^~//;     ## note and remove negation operator

        $t_regex = qr/$t_regex/;           ## make regex string from
                                           ## user regex

        ## filter (out) tables specified in t_regex
        print "Filtering tables with '$t_regex'\n" if $opt{debug};
        @dbh_base_tables = ( $negated 
                             ? grep { $_ !~ $t_regex } @dbh_base_tables
                             : grep { $_ =~ $t_regex } @dbh_base_tables );

        ## filter (out) views specified in t_regex
        print "Filtering tables with '$t_regex'\n" if $opt{debug};
        @dbh_views = ( $negated 
                       ? grep { $_ !~ $t_regex } @dbh_views
                       : grep { $_ =~ $t_regex } @dbh_views );
    }

    ## Now concatenate the base table and view arrays.
    my @dbh_tables = (@dbh_base_tables, @dbh_views);

    ## get list of files to copy
    my $db_dir = "$datadir/$db";
    opendir(DBDIR, $db_dir ) 
      or die "Cannot open dir '$db_dir': $!";

    my %db_files;

    while ( defined( my $name = readdir DBDIR ) ) {
        $db_files{$name} = $1 if ( $name =~ /(.+)\.\w+$/ );
    }
    closedir( DBDIR );

    unless( keys %db_files ) {
	warn "'$db' is an empty database\n";
    }

    ## filter (out) files specified in t_regex
    my @db_files;
    if ($rdb->{t_regex}) {
        @db_files = ($negated
                     ? grep { $db_files{$_} !~ $t_regex } keys %db_files
                     : grep { $db_files{$_} =~ $t_regex } keys %db_files );
    }
    else {
        @db_files = keys %db_files;
    }

    @db_files = sort @db_files;

    my @index_files=();

    ## remove indices unless we're told to keep them
    if ($opt{noindices}) {
        @index_files= grep { /\.(ISM|MYI)$/ } @db_files;
	@db_files = grep { not /\.(ISM|MYI)$/ } @db_files;
    }

    $rdb->{files}  = [ @db_files ];
    $rdb->{index}  = [ @index_files ];
    my @hc_base_tables = map { quote_names("$db.$_") } @dbh_base_tables;
    my @hc_views = map { quote_names("$db.$_") } @dbh_views;
    
    my @hc_tables = (@hc_base_tables, @hc_views);
    $rdb->{tables} = [ @hc_tables ];

    $hc_locks .= ", "  if ( length $hc_locks && @hc_tables );
    $hc_locks .= join ", ", map { "$_ READ" } @hc_tables;

    $hc_base_tables .= ", "  if ( length $hc_base_tables && @hc_base_tables );
    $hc_base_tables .= join ", ", @hc_base_tables;
    $hc_views .= ", "  if ( length $hc_views && @hc_views );
    $hc_views .= join " READ, ", @hc_views;

    @hc_tables = (@hc_base_tables, @hc_views);

    $num_base_tables += scalar @hc_base_tables;
    $num_views += scalar @hc_views;
    $num_tables += $num_base_tables + $num_views;
    $num_files  += scalar @{$rdb->{files}};
}

# --- resolve targets for copies ---

if (defined($tgt_name) && length $tgt_name ) {
    # explicit destination directory specified

    # GNU `cp -r` error message
    die "copying multiple databases, but last argument ($tgt_dirname) is not a directory\n"
      if ( @db_desc > 1 && !(-e $tgt_dirname && -d $tgt_dirname ) );

    if ($to_other_database)
    {
      foreach my $rdb ( @db_desc ) {
	$rdb->{target} = "$tgt_dirname";
      }
    }
    elsif ($opt{method} =~ /^scp\b/) 
    {   # we have to trust scp to hit the target
	foreach my $rdb ( @db_desc ) {
	    $rdb->{target} = "$tgt_dirname/$rdb->{src}";
	}
    }
    else
    {
      die "Last argument ($tgt_dirname) is not a directory\n"
	if (!(-e $tgt_dirname && -d $tgt_dirname ) );
      foreach my $rdb ( @db_desc ) {
	$rdb->{target} = "$tgt_dirname/$rdb->{src}";
      }
    }
  }
else {
  die "Error: expected \$opt{suffix} to exist" unless ( exists $opt{suffix} );

  foreach my $rdb ( @db_desc ) {
    $rdb->{target} = "$datadir/$rdb->{src}$opt{suffix}";
  }
}

print Dumper( \@db_desc ) if ( $opt{debug} );

# --- bail out if all specified databases are empty ---

die "No tables to hot-copy" unless ( length $hc_locks );

# --- create target directories if we are using 'cp' ---

my @existing = ();

if ($opt{method} =~ /^cp\b/)
{
  foreach my $rdb ( @db_desc ) {
    push @existing, $rdb->{target} if ( -d  $rdb->{target} );
  }

  if ( @existing && !($opt{allowold} || $opt{addtodest}) )
  {
    $dbh->disconnect();
    die "Can't hotcopy to '", join( "','", @existing ), "' because directory\nalready exist and the --allowold or --addtodest options were not given.\n"
  }
}

retire_directory( @existing ) if @existing && !$opt{addtodest};

foreach my $rdb ( @db_desc ) {
    my $tgt_dirpath = "$rdb->{target}";
    # Remove trailing slashes (needed for Mac OS X)
    substr($tgt_dirpath, 1) =~ s|/+$||;
    if ( $opt{dryrun} ) {
        print "mkdir $tgt_dirpath, 0750\n";
    }
    elsif ($opt{method} =~ /^scp\b/) {
        ## assume it's there?
        ## ...
    }
    else {
        mkdir($tgt_dirpath, 0750) or die "Can't create '$tgt_dirpath': $!\n"
            unless -d $tgt_dirpath;
        my @f_info= stat "$datadir/$rdb->{src}";
        chown $f_info[4], $f_info[5], $tgt_dirpath;
    }
}

##############################
# --- PERFORM THE HOT-COPY ---
#
# Note that we try to keep the time between the LOCK and the UNLOCK
# as short as possible, and only start when we know that we should
# be able to complete without error.

# read lock all the tables we'll be copying
# in order to get a consistent snapshot of the database

if ( $opt{checkpoint} || $opt{record_log_pos} ) {
  # convert existing READ lock on checkpoint and/or log_pos table into WRITE lock
  foreach my $table ( grep { defined } ( $opt{checkpoint}, $opt{record_log_pos} ) ) {
    $hc_locks .= ", $table WRITE" 
	unless ( $hc_locks =~ s/$table\s+READ/$table WRITE/ );
  }
}

my $hc_started = time;	# count from time lock is granted

if ( $opt{dryrun} ) {
    if ( $opt{old_server} ) {
        print "LOCK TABLES $hc_locks\n";
        print "FLUSH TABLES /*!32323 $hc_tables */\n";
    }
    else {
        # Lock base tables and views separately.
        print "FLUSH TABLES $hc_base_tables WITH READ LOCK\n"
          if ( $hc_base_tables );
        print "LOCK TABLES $hc_views READ\n" if ( $hc_views );
    }
    
    print "FLUSH LOGS\n" if ( $opt{flushlog} );
    print "RESET MASTER\n" if ( $opt{resetmaster} );
    print "RESET SLAVE\n" if ( $opt{resetslave} );
}
else {
    my $start = time;
    if ( $opt{old_server} ) {
        $dbh->do("LOCK TABLES $hc_locks");
        printf "Locked $num_tables tables in %d seconds.\n", time-$start unless $opt{quiet};
        $hc_started = time;	# count from time lock is granted

        # flush tables to make on-disk copy up to date
        $start = time;
        $dbh->do("FLUSH TABLES /*!32323 $hc_tables */");
        printf "Flushed tables ($hc_tables) in %d seconds.\n", time-$start unless $opt{quiet};
    }
    else {
        # Lock base tables and views separately, as 'FLUSH TABLES <tbl_name>
        # ... WITH READ LOCK' (introduced in 5.5) would fail for views.
        # Also, flush tables to make on-disk copy up to date
        $dbh->do("FLUSH TABLES $hc_base_tables WITH READ LOCK")
          if ( $hc_base_tables );
        printf "Flushed $num_base_tables tables with read lock ($hc_base_tables) in %d seconds.\n",
               time-$start unless $opt{quiet};

        $start = time;
        $dbh->do("LOCK TABLES $hc_views READ") if ( $hc_views );
        printf "Locked $num_views views ($hc_views) in %d seconds.\n",
               time-$start unless $opt{quiet};

        $hc_started = time;	# count from time lock is granted
    }
    $dbh->do( "FLUSH LOGS" ) if ( $opt{flushlog} );
    $dbh->do( "RESET MASTER" ) if ( $opt{resetmaster} );
    $dbh->do( "RESET SLAVE" ) if ( $opt{resetslave} );

    if ( $opt{record_log_pos} ) {
	record_log_pos( $dbh, $opt{record_log_pos} );
	$dbh->do("FLUSH TABLES /*!32323 $hc_tables */");
    }
}

my @failed = ();

foreach my $rdb ( @db_desc )
{
  my @files = map { "$datadir/$rdb->{src}/$_" } @{$rdb->{files}};
  next unless @files;
  
  eval { copy_files($opt{method}, \@files, $rdb->{target}); };
  push @failed, "$rdb->{src} -> $rdb->{target} failed: $@"
    if ( $@ );
  
  @files = @{$rdb->{index}};
  if ($rdb->{index})
  {
    copy_index($opt{method}, \@files,
	       "$datadir/$rdb->{src}", $rdb->{target} );
  }
  
  if ( $opt{checkpoint} ) {
    my $msg = ( $@ ) ? "Failed: $@" : "Succeeded";
    
    eval {
      $dbh->do( qq{ insert into $opt{checkpoint} (src, dest, msg) 
		      VALUES ( '$rdb->{src}', '$rdb->{target}', '$msg' )
		    } ); 
    };
    
    if ( $@ ) {
      warn "Failed to update checkpoint table: $@\n";
    }
  }
}

if ( $opt{dryrun} ) {
    print "UNLOCK TABLES\n";
    if ( @existing && !$opt{keepold} ) {
	my @oldies = map { $_ . '_old' } @existing;
	print "rm -rf @oldies\n" 
    }
    $dbh->disconnect();
    exit(0);
}
else {
    $dbh->do("UNLOCK TABLES");
}

my $hc_dur = time - $hc_started;
printf "Unlocked tables.\n" unless $opt{quiet};

#
# --- HOT-COPY COMPLETE ---
###########################

$dbh->disconnect;

if ( @failed ) {
    # hotcopy failed - cleanup
    # delete any @targets 
    # rename _old copy back to original

    my @targets = ();
    foreach my $rdb ( @db_desc ) {
        push @targets, $rdb->{target} if ( -d  $rdb->{target} );
    }
    print "Deleting @targets \n" if $opt{debug};

    print "Deleting @targets \n" if $opt{debug};
    rmtree([@targets]);
    if (@existing) {
	print "Restoring @existing from back-up\n" if $opt{debug};
        foreach my $dir ( @existing ) {
	    rename("${dir}_old", $dir )
	      or warn "Can't rename ${dir}_old to $dir: $!\n";
	}
    }

    die join( "\n", @failed );
}
else {
    # hotcopy worked
    # delete _old unless $opt{keepold}

    if ( @existing && !$opt{keepold} ) {
	my @oldies = map { $_ . '_old' } @existing;
	print "Deleting previous copy in @oldies\n" if $opt{debug};
	rmtree([@oldies]);
    }

    printf "$0 copied %d tables (%d files) in %d second%s (%d seconds overall).\n",
	    $num_tables, $num_files,
	    $hc_dur, ($hc_dur==1)?"":"s", time - $start_time
	unless $opt{quiet};
}

exit 0;


# ---

sub copy_files {
    my ($method, $files, $target) = @_;
    my @cmd;
    print "Copying ".@$files." files...\n" unless $opt{quiet};

    if ($method =~ /^s?cp\b/)  # cp or scp with optional flags
    {
	my $cp = $method;
	# add option to preserve mod time etc of copied files
	# not critical, but nice to have
	$cp.= " -p" if $^O =~ m/^(solaris|linux|freebsd|darwin)$/;

	# add recursive option for scp
	$cp.= " -r" if $^O =~ /m^(solaris|linux|freebsd|darwin)$/ && $method =~ /^scp\b/;

	# perform the actual copy
	safe_system( $cp, (map { "'$_'" } @$files), "'$target'" );
    }
    else
    {
	die "Can't use unsupported method '$method'\n";
    }
}

#
# Copy only the header of the index file
#

sub copy_index
{
  my ($method, $files, $source, $target) = @_;
  
  print "Copying indices for ".@$files." files...\n" unless $opt{quiet};  
  foreach my $file (@$files)
  {
    my $from="$source/$file";
    my $to="$target/$file";
    my $buff;
    open(INPUT, "<$from") || die "Can't open file $from: $!\n";
    binmode(INPUT, ":raw");
    my $length=read INPUT, $buff, 2048;
    die "Can't read index header from $from\n" if ($length < 1024);
    close INPUT;
    
    if ( $opt{dryrun} )
    {
      print "$opt{method}-header $from $to\n";
    }
    elsif ($opt{method} eq 'cp')
    {
      open(OUTPUT,">$to")   || die "Can\'t create file $to: $!\n";
      if (syswrite(OUTPUT,$buff) != length($buff))
      {
	die "Error when writing data to $to: $!\n";
      }
      close OUTPUT	   || die "Error on close of $to: $!\n";
    }
    elsif ($opt{method} =~ /^scp\b/)
    {
      my ($fh, $tmp)= tempfile('mysqlhotcopy-XXXXXX', DIR => $opt_tmpdir) or
	die "Can\'t create/open file in $opt_tmpdir\n";
      if (syswrite($fh,$buff) != length($buff))
      {
	die "Error when writing data to $tmp: $!\n";
      }
      close $fh || die "Error on close of $tmp: $!\n";
      safe_system("$opt{method} $tmp $to");
      unlink $tmp;
    }
    else
    {
      die "Can't use unsupported method '$opt{method}'\n";
    }
  }
}


sub safe_system {
  my @sources= @_;
  my $method= shift @sources;
  my $target= pop @sources;
  ## @sources = list of source file names

  ## We have to deal with very long command lines, otherwise they may generate 
  ## "Argument list too long".
  ## With 10000 tables the command line can be around 1MB, much more than 128kB
  ## which is the common limit on Linux (can be read from
  ## /usr/src/linux/include/linux/binfmts.h
  ## see http://www.linuxjournal.com/article.php?sid=6060).
 
  my $chunk_limit= 100 * 1024; # 100 kB
  my @chunk= (); 
  my $chunk_length= 0;
  foreach (@sources) {
      push @chunk, $_;
      $chunk_length+= length($_);
      if ($chunk_length > $chunk_limit) {
          safe_simple_system($method, @chunk, $target);
          @chunk=();
          $chunk_length= 0;
      }
  }
  if ($chunk_length > 0) { # do not forget last small chunk
      safe_simple_system($method, @chunk, $target); 
  }
}

sub safe_simple_system {
    my @cmd= @_;

    if ( $opt{dryrun} ) {
        print "@cmd\n";
    }
    else {
        ## for some reason system fails but backticks works ok for scp...
        print "Executing '@cmd'\n" if $opt{debug};
        my $cp_status = system "@cmd > /dev/null";
        if ($cp_status != 0) {
            warn "Executing command failed ($cp_status). Trying backtick execution...\n";
            ## try something else
            `@cmd` || die "Error: @cmd failed ($?) while copying files.\n";
        }
    }
}

sub retire_directory {
    my ( @dir ) = @_;

    foreach my $dir ( @dir ) {
	my $tgt_oldpath = $dir . '_old';
	if ( $opt{dryrun} ) {
	    print "rmtree $tgt_oldpath\n" if ( -d $tgt_oldpath );
	    print "rename $dir, $tgt_oldpath\n";
	    next;
	}

	if ( -d $tgt_oldpath ) {
	    print "Deleting previous 'old' hotcopy directory ('$tgt_oldpath')\n" unless $opt{quiet};
	    rmtree([$tgt_oldpath],0,1);
	}
	rename($dir, $tgt_oldpath)
	  or die "Can't rename $dir=>$tgt_oldpath: $!\n";
	print "Existing hotcopy directory renamed to '$tgt_oldpath'\n" unless $opt{quiet};
    }
}

sub record_log_pos {
    my ( $dbh, $table_name ) = @_;

    eval {
	my ($file,$position) = get_row( $dbh, "show master status" );
	die "master status is undefined" if !defined $file || !defined $position;
	
	my $row_hash = get_row_hash( $dbh, "show slave status" );
	my ($master_host, $log_file, $log_pos ); 
	if ( $dbh->{mysql_serverinfo} =~ /^3\.23/ ) {
	    ($master_host, $log_file, $log_pos ) 
	      = @{$row_hash}{ qw / Master_Host Log_File Pos / };
	} else {
	    ($master_host, $log_file, $log_pos ) 
	      = @{$row_hash}{ qw / Master_Host Relay_Master_Log_File Exec_Master_Log_Pos / };
	}
	my $hostname = hostname();
	
	$dbh->do( qq{ replace into $table_name 
			  set host=?, log_file=?, log_pos=?, 
                          master_host=?, master_log_file=?, master_log_pos=? }, 
		  undef, 
		  $hostname, $file, $position, 
		  $master_host, $log_file, $log_pos  );
	
    };
    
    if ( $@ ) {
	warn "Failed to store master position: $@\n";
    }
}

sub get_row {
  my ( $dbh, $sql ) = @_;

  my $sth = $dbh->prepare($sql);
  $sth->execute;
  return $sth->fetchrow_array();
}

sub get_row_hash {
  my ( $dbh, $sql ) = @_;

  my $sth = $dbh->prepare($sql);
  $sth->execute;
  return $sth->fetchrow_hashref();
}

sub get_list_of_tables {
    my ( $db ) = @_;

    my $tables =
        eval {
            $dbh->selectall_arrayref('SHOW FULL TABLES FROM ' .
                                     $dbh->quote_identifier($db) .
                                     ' WHERE Table_type = \'BASE TABLE\'')
        } || [];
    warn "Unable to retrieve list of tables in $db: $@" if $@;

    return (map { $_->[0] } @$tables);
}

sub get_list_of_views {
    my ( $db ) = @_;

    my $views =
        eval {
            $dbh->selectall_arrayref('SHOW FULL TABLES FROM ' .
                                     $dbh->quote_identifier($db) .
                                     ' WHERE Table_type = \'VIEW\'')
        } || [];
    warn "Unable to retrieve list of views in $db: $@" if $@;

    return (map { $_->[0] } @$views);
}

sub quote_names {
  my ( $name ) = @_;
  # given a db.table name, add quotes

  my ($db, $table, @cruft) = split( /\./, $name );
  die "Invalid db.table name '$name'" if (@cruft || !defined $db || !defined $table );

  # Earlier versions of DBD return table name non-quoted,
  # such as DBD-2.1012 and the newer ones, such as DBD-2.9002
  # returns it quoted. Let's have a support for both.
  $table=~ s/\`//g;
  return "`$db`.`$table`";
}

__END__

=head1 DESCRIPTION

mysqlhotcopy is designed to make stable copies of live MySQL databases.

Here "live" means that the database server is running and the database
may be in active use. And "stable" means that the copy will not have
any corruptions that could occur if the table files were simply copied
without first being locked and flushed from within the server.

=head1 OPTIONS

=over 4

=item --checkpoint checkpoint-table

As each database is copied, an entry is written to the specified
checkpoint-table.  This has the happy side-effect of updating the
MySQL update-log (if it is switched on) giving a good indication of
where roll-forward should begin for backup+rollforward schemes.

The name of the checkpoint table should be supplied in database.table format.
The checkpoint-table must contain at least the following fields:

=over 4

  time_stamp timestamp not null
  src varchar(32)
  dest varchar(60)
  msg varchar(255)

=back

=item --record_log_pos log-pos-table

Just before the database files are copied, update the record in the
log-pos-table from the values returned from "show master status" and
"show slave status". The master status values are stored in the
log_file and log_pos columns, and establish the position in the binary
logs that any slaves of this host should adopt if initialised from
this dump.  The slave status values are stored in master_host,
master_log_file, and master_log_pos, corresponding to the coordinates
of the next to the last event the slave has executed. The slave or its
siblings can connect to the master next time and request replication
starting from the recorded values. 

The name of the log-pos table should be supplied in database.table format.
A sample log-pos table definition:

=over 4

CREATE TABLE log_pos (
  host            varchar(60) NOT null,
  time_stamp      timestamp(14) NOT NULL,
  log_file        varchar(32) default NULL,
  log_pos         int(11)     default NULL,
  master_host     varchar(60) NULL,
  master_log_file varchar(32) NULL,
  master_log_pos  int NULL,

  PRIMARY KEY  (host) 
);

=back


=item --suffix suffix

Each database is copied back into the originating datadir under
a new name. The new name is the original name with the suffix
appended. 

If only a single db_name is supplied and the --suffix flag is not
supplied, then "--suffix=_copy" is assumed.

=item --allowold

Move any existing version of the destination to a backup directory for
the duration of the copy. If the copy successfully completes, the backup 
directory is deleted - unless the --keepold flag is set.  If the copy fails,
the backup directory is restored.

The backup directory name is the original name with "_old" appended.
Any existing versions of the backup directory are deleted.

=item --keepold

Behaves as for the --allowold, with the additional feature 
of keeping the backup directory after the copy successfully completes.

=item --addtodest

Don't rename target directory if it already exists, just add the
copied files into it.

This is most useful when backing up a database with many large
tables and you don't want to have all the tables locked for the
whole duration.

In this situation, I<if> you are happy for groups of tables to be
backed up separately (and thus possibly not be logically consistent
with one another) then you can run mysqlhotcopy several times on
the same database each with different db_name./table_regex/.
All but the first should use the --addtodest option so the tables
all end up in the same directory.

=item --flushlog

Rotate the log files by executing "FLUSH LOGS" after all tables are
locked, and before they are copied.

=item --resetmaster

Reset the bin-log by executing "RESET MASTER" after all tables are
locked, and before they are copied. Useful if you are recovering a
slave in a replication setup.

=item --resetslave

Reset the master.info by executing "RESET SLAVE" after all tables are
locked, and before they are copied. Useful if you are recovering a
server in a mutual replication setup.

=item --regexp pattern

Copy all databases with names matching the pattern.

=item --regexp /pattern1/./pattern2/

Copy all tables with names matching pattern2 from all databases with
names matching pattern1. For example, to select all tables which
names begin with 'bar' from all databases which names end with 'foo':

   mysqlhotcopy --indices --method=cp --regexp /foo$/./^bar/

=item db_name./pattern/

Copy only tables matching pattern. Shell metacharacters ( (, ), |, !,
etc.) have to be escaped (e.g., \). For example, to select all tables
in database db1 whose names begin with 'foo' or 'bar':

    mysqlhotcopy --indices --method=cp db1./^\(foo\|bar\)/

=item db_name./~pattern/

Copy only tables not matching pattern. For example, to copy tables
that do not begin with foo nor bar:

    mysqlhotcopy --indices --method=cp db1./~^\(foo\|bar\)/

=item -?, --help

Display help-screen and exit.

=item -u, --user=#         

User for database login if not current user.

=item -p, --password=#     

Password to use when connecting to the server. Note that you are strongly
encouraged *not* to use this option as every user would be able to see the
password in the process list. Instead use the '[mysqlhotcopy]' section in
one of the config files, normally /etc/my.cnf or your personal ~/.my.cnf.
(See the chapter 'my.cnf Option Files' in the manual.)

=item -h, -h, --host=#

Hostname for local server when connecting over TCP/IP.  By specifying this
different from 'localhost' will trigger mysqlhotcopy to use TCP/IP connection.

=item -P, --port=#         

Port to use when connecting to MySQL server with TCP/IP.  This is only used
when using the --host option.

=item -S, --socket=#         

UNIX domain socket to use when connecting to local server.

=item --old_server

Use old server (pre v5.5) commands.

=item  --noindices          

Don\'t include index files in copy. Only up to the first 2048 bytes
are copied;  You can restore the indexes with isamchk -r or myisamchk -r
on the backup.

=item  --method=#           

Method for copy (only "cp" currently supported). Alpha support for
"scp" was added in November 2000. Your experience with the scp method
will vary with your ability to understand how scp works. 'man scp'
and 'man ssh' are your friends.

The destination directory _must exist_ on the target machine using the
scp method. --keepold and --allowold are meaningless with scp.
Liberal use of the --debug option will help you figure out what\'s
really going on when you do an scp.

Note that using scp will lock your tables for a _long_ time unless
your network connection is _fast_. If this is unacceptable to you,
use the 'cp' method to copy the tables to some temporary area and then
scp or rsync the files at your leisure.

=item -q, --quiet              

Be silent except for errors.

=item  --debug

Debug messages are displayed.

=item -n, --dryrun

Display commands without actually doing them.

=back

=head1 WARRANTY

This software is free and comes without warranty of any kind. You
should never trust backup software without studying the code yourself.
Study the code inside this script and only rely on it if I<you> believe
that it does the right thing for you.

Patches adding bug fixes, documentation and new features are welcome.
Please send these to internals@lists.mysql.com.

=head1 TO DO

Extend the individual table copy to allow multiple subsets of tables
to be specified on the command line:

  mysqlhotcopy db newdb  t1 t2 /^foo_/ : t3 /^bar_/ : +

where ":" delimits the subsets, the /^foo_/ indicates all tables
with names beginning with "foo_" and the "+" indicates all tables
not copied by the previous subsets.

'newdb' is either the name of the new database, or the full path name
of the new database file. The database should not already exist.

Add option to lock each table in turn for people who don\'t need
cross-table integrity.

Add option to FLUSH STATUS just before UNLOCK TABLES.

Add support for other copy methods (e.g., tar to single file?).

Add support for forthcoming MySQL ``RAID'' table subdirectory layouts.

=head1 AUTHOR

Tim Bunce

Martin Waite - Added checkpoint, flushlog, regexp and dryrun options.
               Fixed cleanup of targets when hotcopy fails. 
               Added --record_log_pos.
               RAID tables are now copied (don't know if this works over scp).

Ralph Corderoy - Added synonyms for commands.

Scott Wiersdorf - Added table regex and scp support.

Monty - Working --noindex (copy only first 2048 bytes of index file).
        Fixes for --method=scp.

Ask Bjoern Hansen - Cleanup code to fix a few bugs and enable -w again.

Emil S. Hansen - Added resetslave and resetmaster.

Jeremy D. Zawodny - Removed deprecated DBI calls.  Fixed bug which
resulted in nothing being copied when a regexp was specified but no
database name(s).

Martin Waite - Fix to handle database name that contains space.

Paul DuBois - Remove end '/' from directory names.
