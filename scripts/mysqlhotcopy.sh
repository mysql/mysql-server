#!@PERL@ -w

use strict;
use Getopt::Long;
use Data::Dumper;
use File::Basename;
use File::Path;
use DBI;

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

WARNING: THIS IS VERY MUCH A FIRST-CUT ALPHA. Comments/patches welcome.

=cut

# Documentation continued at end of file

my $VERSION = "1.12";

my $opt_tmpdir = $ENV{TMPDIR} || "/tmp";

my $OPTIONS = <<"_OPTIONS";

$0 Ver $VERSION

Usage: $0 db_name[./table_regex/] [new_db_name | directory]

  -?, --help           display this helpscreen and exit
  -u, --user=#         user for database login if not current user
  -p, --password=#     password to use when connecting to server
  -P, --port=#         port to use when connecting to local server
  -S, --socket=#       socket to use when connecting to local server

  --allowold           don\'t abort if target already exists (rename it _old)
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

  Try \'perldoc $0 for more complete documentation\'
_OPTIONS

sub usage {
    die @_, $OPTIONS;
}

my %opt = (
    user	=> scalar getpwuid($>),
    noindices	=> 0,
    allowold	=> 0,	# for safety
    keepold	=> 0,
    method	=> "cp",
    flushlog    => 0,
);
Getopt::Long::Configure(qw(no_ignore_case)); # disambuguate -p and -P
GetOptions( \%opt,
    "help",
    "user|u=s",
    "password|p=s",
    "port|P=s",
    "socket|S=s",
    "allowold!",
    "keepold!",
    "noindices!",
    "method=s",
    "debug",
    "quiet|q",
    "mv!",
    "regexp=s",
    "suffix=s",
    "checkpoint=s",
    "flushlog",
    "resetmaster",
    "resetslave",
    "tmpdir|t=s",
    "dryrun|n",
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
my $dsn = ";host=localhost";
$dsn .= ";port=$opt{port}" if $opt{port};
$dsn .= ";mysql_socket=$opt{socket}" if $opt{socket};

my $dbh = DBI->connect("dbi:mysql:$dsn;mysql_read_default_group=mysqlhotcopy",
                        $opt{user}, $opt{password},
{
    RaiseError => 1,
    PrintError => 0,
    AutoCommit => 1,
});

# --- check that checkpoint table exists if specified ---
if ( $opt{checkpoint} ) {
    eval { $dbh->do( qq{ select time_stamp, src, dest, msg 
			 from $opt{checkpoint} where 1 != 1} );
       };

    die "Error accessing Checkpoint table ($opt{checkpoint}): $@"
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
    my $sth_dbs = $dbh->prepare("show databases");
    $sth_dbs->execute;
    while ( my ($db_name) = $sth_dbs->fetchrow_array ) {
	push @db_desc, { 'src' => $db_name } if ( $db_name =~ m/$opt{regexp}/o );
    }
}

# --- get list of tables to hotcopy ---

my $hc_locks = "";
my $hc_tables = "";
my $num_tables = 0;
my $num_files = 0;

foreach my $rdb ( @db_desc ) {
    my $db = $rdb->{src};
    eval { $dbh->do( "use $db" ); };
    die "Database '$db' not accessible: $@"  if ( $@ );
    my @dbh_tables = $dbh->func( '_ListTables' );

    ## generate regex for tables/files
    my $t_regex = $rdb->{t_regex};        ## assign temporary regex
    my $negated = $t_regex =~ tr/~//d;    ## remove and count negation operator: we don't allow ~ in table names
    $t_regex = qr/$t_regex/;              ## make regex string from user regex

    ## filter (out) tables specified in t_regex
    print "Filtering tables with '$t_regex'\n" if $opt{debug};
    @dbh_tables = ( $negated 
		    ? grep { $_ !~ $t_regex } @dbh_tables 
		    : grep { $_ =~ $t_regex } @dbh_tables );

    ## get list of files to copy
    my $db_dir = "$datadir/$db";
    opendir(DBDIR, $db_dir ) 
      or die "Cannot open dir '$db_dir': $!";

    my %db_files;
    map { ( /(.+)\.\w+$/ ? ( $db_files{$_} = $1 ) : () ) } readdir(DBDIR);
    unless( keys %db_files ) {
	warn "'$db' is an empty database\n";
    }
    closedir( DBDIR );

    ## filter (out) files specified in t_regex
    my @db_files = ( $negated 
			  ? grep { $db_files{$_} !~ $t_regex } keys %db_files
			  : grep { $db_files{$_} =~ $t_regex } keys %db_files );
    @db_files = sort @db_files;
    my @index_files=();

    ## remove indices unless we're told to keep them
    if ($opt{noindices}) {
        @index_files= grep { /\.(ISM|MYI)$/ } @db_files;
	@db_files = grep { not /\.(ISM|MYI)$/ } @db_files;
    }

    $rdb->{files}  = [ @db_files ];
    $rdb->{index}  = [ @index_files ];
    my @hc_tables = map { "$db.$_" } @dbh_tables;
    $rdb->{tables} = [ @hc_tables ];

    $hc_locks .= ", "  if ( length $hc_locks && @hc_tables );
    $hc_locks .= join ", ", map { "$_ READ" } @hc_tables;
    $hc_tables .= ", "  if ( length $hc_tables && @hc_tables );
    $hc_tables .= join ", ", @hc_tables;

    $num_tables += scalar @hc_tables;
    $num_files  += scalar @{$rdb->{files}};
}

# --- resolve targets for copies ---

my @targets = ();

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

  if ( @existing && !$opt{allowold} )
  {
    $dbh->disconnect();
    die "Can't hotcopy to '", join( "','", @existing ), "' because directory\nalready exist and the --allowold option was not given.\n"
  }
}

retire_directory( @existing ) if ( @existing );

foreach my $rdb ( @db_desc ) {
    my $tgt_dirpath = $rdb->{target};
    if ( $opt{dryrun} ) {
	print "mkdir $tgt_dirpath, 0750\n";
    }
    elsif ($opt{method} =~ /^scp\b/) {
	## assume it's there?
	## ...
    }
    else {
	mkdir($tgt_dirpath, 0750)
	  or die "Can't create '$tgt_dirpath': $!\n";
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

if ( $opt{checkpoint} ) {
    # convert existing READ lock on checkpoint table into WRITE lock
    unless ( $hc_locks =~ s/$opt{checkpoint}\s+READ/$opt{checkpoint} WRITE/ ) {
	$hc_locks .= ", $opt{checkpoint} WRITE";
    }
}

my $hc_started = time;	# count from time lock is granted

if ( $opt{dryrun} ) {
    print "LOCK TABLES $hc_locks\n";
    print "FLUSH TABLES /*!32323 $hc_tables */\n";
    print "FLUSH LOGS\n" if ( $opt{flushlog} );
    print "RESET MASTER\n" if ( $opt{resetmaster} );
    print "RESET SLAVE\n" if ( $opt{resetslave} );
}
else {
    my $start = time;
    $dbh->do("LOCK TABLES $hc_locks");
    printf "Locked $num_tables tables in %d seconds.\n", time-$start unless $opt{quiet};
    $hc_started = time;	# count from time lock is granted

    # flush tables to make on-disk copy uptodate
    $start = time;
    $dbh->do("FLUSH TABLES /*!32323 $hc_tables */");
    printf "Flushed tables ($hc_tables) in %d seconds.\n", time-$start unless $opt{quiet};
    $dbh->do( "FLUSH LOGS" ) if ( $opt{flushlog} );
    $dbh->do( "RESET MASTER" ) if ( $opt{resetmaster} );
    $dbh->do( "RESET SLAVE" ) if ( $opt{resetslave} );
}

my @failed = ();

foreach my $rdb ( @db_desc )
{
  my @files = map { "$datadir/$rdb->{src}/$_" } @{$rdb->{files}};
  next unless @files;
  
  eval { copy_files($opt{method}, \@files, $rdb->{target} ); };
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

    if ($method =~ /^s?cp\b/) { # cp or scp with optional flags
	@cmd = ($method);
	# add option to preserve mod time etc of copied files
	# not critical, but nice to have
	push @cmd, "-p" if $^O =~ m/^(solaris|linux|freebsd)$/;

	# add recursive option for scp
	push @cmd, "-r" if $^O =~ /m^(solaris|linux|freebsd)$/ && $method =~ /^scp\b/;

	# add files to copy and the destination directory
	push @cmd, @$files, $target;
    }
    else
    {
	die "Can't use unsupported method '$method'\n";
    }
    safe_system (@cmd);
}

#
# Copy only the header of the index file
#

sub copy_index
{
  my ($method, $files, $source, $target) = @_;
  my $tmpfile="$opt_tmpdir/mysqlhotcopy$$";
  
  print "Copying indices for ".@$files." files...\n" unless $opt{quiet};  
  foreach my $file (@$files)
  {
    my $from="$source/$file";
    my $to="$target/$file";
    my $buff;
    open(INPUT, "<$from") || die "Can't open file $from: $!\n";
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
    elsif ($opt{method} eq 'scp')
    {
      my $tmp=$tmpfile;
      open(OUTPUT,">$tmp") || die "Can\'t create file $tmp: $!\n";
      if (syswrite(OUTPUT,$buff) != length($buff))
      {
	die "Error when writing data to $tmp: $!\n";
      }
      close OUTPUT	     || die "Error on close of $tmp: $!\n";
      safe_system("scp $tmp $to");
    }
    else
    {
      die "Can't use unsupported method '$opt{method}'\n";
    }
  }
  unlink "$tmpfile" if  ($opt{method} eq 'scp');
}


sub safe_system
{
  my @cmd= @_;

  if ( $opt{dryrun} )
  {
    print "@cmd\n";
    return;
  }

  ## for some reason system fails but backticks works ok for scp...
  print "Executing '@cmd'\n" if $opt{debug};
  my $cp_status = system "@cmd > /dev/null";
  if ($cp_status != 0) {
    warn "Burp ('scuse me). Trying backtick execution...\n" if $opt{debug}; #'
    ## try something else
    `@cmd` && die "Error: @cmd failed ($cp_status) while copying files.\n";
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
	    rmtree([$tgt_oldpath])
	}
	rename($dir, $tgt_oldpath)
	  or die "Can't rename $dir=>$tgt_oldpath: $!\n";
	print "Existing hotcopy directory renamed to '$tgt_oldpath'\n" unless $opt{quiet};
    }
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

=item --flushlog

Rotate the log files by executing "FLUSH LOGS" after all tables are
locked, and before they are copied.

=item --resetmaster

Reset the bin-log by executing "RESET MASTER" after all tables are
locked, and before they are copied. Usefull if you are recovering a
slave in a replication setup.

=item --resetslave

Reset the master.info by executing "RESET SLAVE" after all tables are
locked, and before they are copied. Usefull if you are recovering a
server in a mutual replication setup.

=item --regexp pattern

Copy all databases with names matching the pattern

=item db_name./pattern/

Copy only tables matching pattern. Shell metacharacters ( (, ), |, !,
etc.) have to be escaped (e.g. \). For example, to select all tables
in database db1 whose names begin with 'foo' or 'bar':

    mysqlhotcopy --indices --method=cp db1./^\(foo\|bar\)/

=item db_name./~pattern/

Copy only tables not matching pattern. For example, to copy tables
that do not begin with foo nor bar:

    mysqlhotcopy --indices --method=cp db1./~^\(foo\|bar\)/

=item -?, --help

Display helpscreen and exit

=item -u, --user=#         

user for database login if not current user

=item -p, --password=#     

password to use when connecting to server

=item -P, --port=#         

port to use when connecting to local server

=item -S, --socket=#         

UNIX domain socket to use when connecting to local server

=item  --noindices          

Don\'t include index files in copy. Only up to the first 2048 bytes
are copied;  You can restore the indexes with isamchk -r or myisamchk -r
on the backup.

=item  --method=#           

method for copy (only "cp" currently supported). Alpha support for
"scp" was added in November 2000. Your experience with the scp method
will vary with your ability to understand how scp works. 'man scp'
and 'man ssh' are your friends.

The destination directory _must exist_ on the target machine using the
scp method. --keepold and --allowold are meeningless with scp.
Liberal use of the --debug option will help you figure out what\'s
really going on when you do an scp.

Note that using scp will lock your tables for a _long_ time unless
your network connection is _fast_. If this is unacceptable to you,
use the 'cp' method to copy the tables to some temporary area and then
scp or rsync the files at your leisure.

=item -q, --quiet              

be silent except for errors

=item  --debug

Debug messages are displayed 

=item -n, --dryrun

Display commands without actually doing them

=back

=head1 WARRANTY

This software is free and comes without warranty of any kind. You
should never trust backup software without studying the code yourself.
Study the code inside this script and only rely on it if I<you> believe
that it does the right thing for you.

Patches adding bug fixes, documentation and new features are welcome.
Please send these to internals@mysql.com.

=head1 TO DO

Extend the individual table copy to allow multiple subsets of tables
to be specified on the command line:

  mysqlhotcopy db newdb  t1 t2 /^foo_/ : t3 /^bar_/ : +

where ":" delimits the subsets, the /^foo_/ indicates all tables
with names begining with "foo_" and the "+" indicates all tables
not copied by the previous subsets.

newdb is either another not existing database or a full path to a directory
where we can create a directory 'db'

Add option to lock each table in turn for people who don\'t need
cross-table integrity.

Add option to FLUSH STATUS just before UNLOCK TABLES.

Add support for other copy methods (eg tar to single file?).

Add support for forthcoming MySQL ``RAID'' table subdirectory layouts.

=head1 AUTHOR

Tim Bunce

Martin Waite - added checkpoint, flushlog, regexp and dryrun options

Ralph Corderoy - added synonyms for commands

Scott Wiersdorf - added table regex and scp support

Monty - working --noindex (copy only first 2048 bytes of index file)
        Fixes for --method=scp

Ask Bjoern Hansen - Cleanup code to fix a few bugs and enable -w again.

Emil S. Hansen - Added resetslave and resetmaster.

