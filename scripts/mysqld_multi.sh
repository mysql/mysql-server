#!@PERL@

use Getopt::Long;
use POSIX qw(strftime);

$|=1;
$VER="2.10";

$opt_config_file   = undef();
$opt_example       = 0;
$opt_help          = 0;
$opt_log           = undef();
$opt_mysqladmin    = "@bindir@/mysqladmin";
$opt_mysqld        = "@libexecdir@/mysqld";
$opt_no_log        = 0;
$opt_password      = undef();
$opt_tcp_ip        = 0;
$opt_user          = "root";
$opt_version       = 0;
$opt_silent        = 0;
$opt_verbose       = 0;

my $my_print_defaults_exists= 1;
my $logdir= undef();

my ($mysqld, $mysqladmin, $groupids, $homedir, $my_progname);

$homedir = $ENV{HOME};
$my_progname = $0;
$my_progname =~ s/.*[\/]//;

main();

####
#### main sub routine
####

sub main
{
  my ($flag_exit);

  if (!defined(my_which(my_print_defaults)))
  {
    # We can't throw out yet, since --version, --help, or --example may
    # have been given
    print "WARNING! my_print_defaults command not found!\n";
    print "Please make sure you have this command available and\n";
    print "in your path. The command is available from the latest\n";
    print "MySQL distribution.\n";
    $my_print_defaults_exists= 0;
  }
  if ($my_print_defaults_exists)
  {
    foreach my $arg (@ARGV)
    {
      if ($arg =~ m/^--config-file=(.*)/)
      {
	if (!length($1))
	{
	  die "Option config-file requires an argument\n";
	}
	elsif (!( -e $1 && -r $1))
	{
	  die "Option file '$1' doesn't exists, or is not readable\n";
	}
	else
	{
	  $opt_config_file= $1;
	}
      }
    }
    my $com= "my_print_defaults ";
    $com.= "--config-file=$opt_config_file " if (defined($opt_config_file));
    $com.= "mysqld_multi";
    my @defops = `$com`;
    chop @defops;
    splice @ARGV, 0, 0, @defops;
  }
  GetOptions("help","example","version","mysqld=s","mysqladmin=s",
             "config-file=s","user=s","password=s","log=s","no-log","tcp-ip",
             "silent","verbose")
  || die "Wrong option! See $my_progname --help for detailed information!\n";

  if ($opt_verbose && $opt_silent)
  {
    print "Both --verbose and --silent has been given. Some of the warnings ";
    print "will be disabled\nand some will be enabled.\n\n";
  }

  init_log() if (!defined($opt_log));
  $groupids = $ARGV[1];
  if ($opt_version)
  {
    print "$my_progname version $VER by Jani Tolonen\n";
    exit(0);
  }
  example() if ($opt_example);
  usage() if ($opt_help);
  if ($flag_exit)
  {
    print "Error with an option, see $my_progname --help for more info!\n";
    exit(1);
  }
  if (!defined(my_which(my_print_defaults)))
  {
    print "ABORT: Can't find command 'my_print_defaults'!\n";
    print "This command is available from the latest MySQL\n";
    print "distribution. Please make sure you have the command\n";
    print "in your PATH.\n";
    exit(1);
  }
  usage() if (!defined($ARGV[0]) ||
	      (!($ARGV[0] =~ m/^start$/i) &&
	       !($ARGV[0] =~ m/^stop$/i) &&
	       !($ARGV[0] =~ m/^report$/i)));

  if (!$opt_no_log)
  {
    w2log("$my_progname log file version $VER; run: ",
	  "$opt_log", 1, 0);
  }
  else
  {
    print "$my_progname log file version $VER; run: ";
    print strftime "%a %b %e %H:%M:%S %Y", localtime;
    print "\n";
  }
  if ($ARGV[0] =~ m/^start$/i)
  {
    if (!defined(($mysqld= my_which($opt_mysqld))) && $opt_verbose)
    {
      print "WARNING: Couldn't find the default mysqld binary.\n";
      print "Tried: $opt_mysqld\n";
      print "This is OK, if you are using option \"mysqld=...\" in ";
      print "groups [mysqldN] separately for each.\n\n";
    }
    start_mysqlds();
  }
  else
  {
    if (!defined(($mysqladmin= my_which($opt_mysqladmin))) && $opt_verbose)
    {
      print "WARNING: Couldn't find the default mysqladmin binary.\n";
      print "Tried: $opt_mysqladmin\n";
      print "This is OK, if you are using option \"mysqladmin=...\" in ";
      print "groups [mysqldN] separately for each.\n\n";
    }
    if ($ARGV[0] =~ m/^report$/i)
    {
      report_mysqlds();
    }
    else
    {
      stop_mysqlds();
    }
  }
}

####
#### Init log file. Check for appropriate place for log file, in the following
#### order my_print_defaults mysqld datadir, @datadir@, /var/log, /tmp
####

sub init_log
{
  if ($my_print_defaults_exists)
  {
    @mysqld_opts= `my_print_defaults mysqld`;
    chomp @mysqld_opts;
    foreach my $opt (@mysqld_opts)
    {
      if ($opt =~ m/^\-\-datadir[=](.*)/)
      {
        if (-d "$1" && -w "$1")
        {
	  $logdir= $1;
        }
      }
    }
  }
  if (!defined($logdir))
  {
    $logdir= "@datadir@" if (-d "@datadir@" && -w "@datadir@");
  }
  if (!defined($logdir))
  {
    # Log file was not specified and we could not log to a standard place,
    # so log file be disabled for now.
    if (!$opt_silent)
    {
      print "WARNING: Log file disabled. Maybe directory or file isn't writable?\n";
    }
    $opt_no_log= 1;
  }
  else
  {
    $opt_log= "$logdir/mysqld_multi.log";
  }
}

####
#### Report living and not running MySQL servers
####

sub report_mysqlds
{
  my (@groups, $com, $i, @options, $pec);

  print "Reporting MySQL servers\n";
  if (!$opt_no_log)
  {
    w2log("\nReporting MySQL servers","$opt_log",0,0);
  }
  @groups = &find_groups($groupids);
  for ($i = 0; defined($groups[$i]); $i++)
  {
    $com= get_mysqladmin_options($i, @groups);
    $com.= " ping >> /dev/null 2>&1";
    system($com);
    $pec = $? >> 8;
    if ($pec)
    {
      print "MySQL server from group: $groups[$i] is not running\n";
      if (!$opt_no_log)
      {
	w2log("MySQL server from group: $groups[$i] is not running",
	      "$opt_log", 0, 0);
      }
    }
    else
    {
      print "MySQL server from group: $groups[$i] is running\n";
      if (!$opt_no_log)
      {
	w2log("MySQL server from group: $groups[$i] is running",
	      "$opt_log", 0, 0);
      }
    }
  }
  if (!$i)
  {
    print "No groups to be reported (check your GNRs)\n";
    if (!$opt_no_log)
    {
      w2log("No groups to be reported (check your GNRs)", "$opt_log", 0, 0);
    }
  }
}

####
#### start multiple servers
####

sub start_mysqlds()
{
  my (@groups, $com, $tmp, $i, @options, $j, $mysqld_found, $info_sent);

  if (!$opt_no_log)
  {
    w2log("\nStarting MySQL servers\n","$opt_log",0,0);
  }
  else
  {
    print "\nStarting MySQL servers\n";
  }
  @groups = &find_groups($groupids);
  for ($i = 0; defined($groups[$i]); $i++)
  {
    $com = "my_print_defaults";
    $com.= defined($opt_config_file) ? " --config-file=$opt_config_file" : "";
    $com.= " $groups[$i]";
    @options = `$com`;
    chop @options;

    $mysqld_found= 1; # The default
    $mysqld_found= 0 if (!length($mysqld));
    $com= "$mysqld";
    for ($j = 0, $tmp= ""; defined($options[$j]); $j++)
    {
      if ("--mysqladmin=" eq substr($options[$j], 0, 13))
      {
	# catch this and ignore
      }
      elsif ("--mysqld=" eq substr($options[$j], 0, 9))
      {
	$options[$j]=~ s/\-\-mysqld\=//;
	$com= $options[$j];
        $mysqld_found= 1;
      }
      else
      {
	$options[$j]=~ s/;/\\;/g;
	$tmp.= " $options[$j]";
      }
    }
    if ($opt_verbose && $com =~ m/\/safe_mysqld$/ && !$info_sent)
    {
      print "WARNING: safe_mysqld is being used to start mysqld. In this case you ";
      print "may need to pass\n\"ledir=...\" under groups [mysqldN] to ";
      print "safe_mysqld in order to find the actual mysqld binary.\n";
      print "ledir (library executable directory) should be the path to the ";
      print "wanted mysqld binary.\n\n";
      $info_sent= 1;
    }
    $com.= $tmp;
    $com.= " >> $opt_log 2>&1" if (!$opt_no_log);
    $com.= " &";
    if (!$mysqld_found)
    {
      print "\n";
      print "FATAL ERROR: Tried to start mysqld under group [$groups[$i]], ";
      print "but no mysqld binary was found.\n";
      print "Please add \"mysqld=...\" in group [mysqld_multi], or add it to ";
      print "group [$groups[$i]] separately.\n";
      exit(1);
    }
    system($com);
  }
  if (!$i && !$opt_no_log)
  {
    w2log("No MySQL servers to be started (check your GNRs)",
	  "$opt_log", 0, 0);
  }
}

####
#### stop multiple servers
####

sub stop_mysqlds()
{
  my (@groups, $com, $i, @options);

  if (!$opt_no_log)
  {
    w2log("\nStopping MySQL servers\n","$opt_log",0,0);
  }
  else
  {
    print "\nStopping MySQL servers\n";
  }
  @groups = &find_groups($groupids);
  for ($i = 0; defined($groups[$i]); $i++)
  {
    $com= get_mysqladmin_options($i, @groups);
    $com.= " shutdown";
    $com.= " >> $opt_log 2>&1" if (!$opt_no_log);
    $com.= " &";
    system($com);
  }
  if (!$i && !$opt_no_log)
  {
    w2log("No MySQL servers to be stopped (check your GNRs)",
	  "$opt_log", 0, 0);
  }
}

####
#### Sub function for mysqladmin option parsing
####

sub get_mysqladmin_options
{
  my ($i, @groups)= @_;
  my ($mysqladmin_found, $com, $tmp, $j);

  $com = "my_print_defaults";
  $com.= defined($opt_config_file) ? " --config-file=$opt_config_file" : "";
  $com.= " $groups[$i]";
  @options = `$com`;
  chop @options;

  $mysqladmin_found= 1; # The default
  $mysqladmin_found= 0 if (!length($mysqladmin));
  $com = "$mysqladmin";
  $tmp = " -u $opt_user";
  $tmp.= defined($opt_password) ? " -p$opt_password" : "";
  $tmp.= $opt_tcp_ip ? " -h 127.0.0.1" : "";
  for ($j = 0; defined($options[$j]); $j++)
  {
    if ("--mysqladmin=" eq substr($options[$j], 0, 13))
    {
      $options[$j]=~ s/\-\-mysqladmin\=//;
      $com= $options[$j];
      $mysqladmin_found= 1;
    }
    elsif ((($options[$j] =~ m/^(\-\-socket\=)(.*)$/) && !$opt_tcp_ip) ||
	   ($options[$j] =~ m/^(\-\-port\=)(.*)$/))
    {
      $tmp.= " $options[$j]";
    }
  }
  if (!$mysqladmin_found)
  {
    print "\n";
    print "FATAL ERROR: Tried to use mysqladmin in group [$groups[$i]], ";
    print "but no mysqladmin binary was found.\n";
    print "Please add \"mysqladmin=...\" in group [mysqld_multi], or ";
    print "in group [$groups[$i]].\n";
    exit(1);
  }
  $com.= $tmp;
  return $com;
}

####
#### Find groups. Takes the valid group numbers as an argument, parses
#### them, puts them in the ascending order, removes duplicates and
#### returns the wanted groups accordingly.
####

sub find_groups
{
  my ($raw_gids) = @_;
  my (@groups, @data, @tmp, $line, $i, $k, @pre_gids, @gids, @tmp2,
      $prev_value);

  # Read the lines from the config file to variable 'data'
  if (defined($opt_config_file))
  {
    open(MY_CNF, "<$opt_config_file") && (@data=<MY_CNF>) && close(MY_CNF);
  }
  else
  {
    if (-f "/etc/my.cnf" && -r "/etc/my.cnf")
    {
      open(MY_CNF, "</etc/my.cnf") && (@tmp=<MY_CNF>) && close(MY_CNF);
    }
    for ($i = 0; ($line = shift @tmp); $i++)
    {
      $data[$i] = $line;
    }
    if (-f "$homedir/.my.cnf" && -r "$homedir/.my.cnf")
    {
      open(MY_CNF, "<$homedir/.my.cnf") && (@tmp=<MY_CNF>) && close(MY_CNF);
    }
    for (; ($line = shift @tmp); $i++)
    {
      $data[$i] = $line;
    }
  }
  chop @data;
  # Make a list of the wanted group ids
  if (defined($raw_gids))
  {
    @pre_gids = split(',', $raw_gids);
  }
  if (defined($raw_gids))
  {
    for ($i = 0, $j = 0; defined($pre_gids[$i]); $i++)
    {
      if ($pre_gids[$i] =~ m/^(\d+)$/)
      {
	$gids[$j] = $1;
	$j++;
      }
      elsif ($pre_gids[$i] =~ m/^(\d+)(\-)(\d+)$/)
      {
	for ($k = $1; $k <= $3; $k++)
	{
	  $gids[$j] = $k;
	  $j++;
	}
      }
      else
      {
	print "ABORT: Bad GNR: $pre_gids[$i] See $my_progname --help\n";
	exit(1);
      }
    }
  }
  # Sort the list of gids numerically in ascending order
  @gids = sort {$a <=> $b} @gids;
  # Remove non-positive integers and duplicates
  for ($i = 0, $j = 0; defined($gids[$i]); $i++)
  {
    next if ($gids[$i] <= 0);
    if (!$i || $prev_value != $gids[$i])
    {
      $tmp2[$j] = $gids[$i];
      $j++;
    }
    $prev_value = $gids[$i];
  }
  @gids = @tmp2;
  # Find and return the wanted groups
  for ($i = 0, $j = 0; defined($data[$i]); $i++)
  {
    if ($data[$i] =~ m/^(\s*\[\s*)(mysqld)(\d+)(\s*\]\s*)$/)
    {
      if (defined($raw_gids))
      {
	for ($k = 0; defined($gids[$k]); $k++)
	{
	  if ($gids[$k] == $3)
	  {
	    $groups[$j] = $2 . $3;
	    $j++;
	  }
	}
      }
      else
      {
	$groups[$j] = $2 . $3;
	$j++;
      }
    }
  }
  return @groups;
}

####
#### w2log: Write to a logfile.
#### 1.arg: append to the log file (given string, or from a file. if a file,
####        file will be read from $opt_logdir)
#### 2.arg: logfile -name (w2log assumes that the logfile is in $opt_logdir).
#### 3.arg. 0 | 1, if true, print current date to the logfile. 3. arg will
####        be ignored, if 1. arg is a file.
#### 4.arg. 0 | 1, if true, first argument is a file, else a string
####

sub w2log
{
  my ($msg, $file, $date_flag, $is_file)= @_;
  my (@data);

  open (LOGFILE, ">>$opt_log")
    or die "FATAL: w2log: Couldn't open log file: $opt_log\n";

  if ($is_file)
  {
    open (FROMFILE, "<$msg") && (@data=<FROMFILE>) &&
      close(FROMFILE)
	or die "FATAL: w2log: Couldn't open file: $msg\n";
    foreach my $line (@data)
    {
      print LOGFILE "$line";
    }
  }
  else
  {
    print LOGFILE "$msg";
    print LOGFILE strftime "%a %b %e %H:%M:%S %Y", localtime if ($date_flag);
    print LOGFILE "\n";
  }
  close (LOGFILE);
  return;
}

####
#### my_which is used, because we can't assume that every system has the
#### which -command. my_which can take only one argument at a time.
#### Return values: requested system command with the first found path,
#### or undefined, if not found.
####

sub my_which
{
  my ($command) = @_;
  my (@paths, $path);

  return $command if (-f $command && -x $command);
  @paths = split(':', $ENV{'PATH'});
  foreach $path (@paths)
  {
    $path .= "/$command";
    return $path if (-f $path && -x $path);
  }
  return undef();
}


####
#### example
####

sub example
{
  print <<EOF;
# This is an example of a my.cnf file on behalf of $my_progname.
# This file should probably be in your home dir (~/.my.cnf) or /etc/my.cnf
# Version $VER by Jani Tolonen
# NOTES:
# 1.Make sure that the MySQL user, who is stopping the mysqld services (e.g
#   using the mysqladmin) have the same password and username for all the
#   data directories accessed (to the 'mysql' database) And make sure that
#   the user has the 'Shutdown_priv' privilege! If you have many data-
#   directories and many different 'mysql' databases with different passwords
#   for the MySQL 'root' user, you may want to create a common 'multi_admin'
#   user for each using the same password (see below). Example how to do it:
#   shell> mysql -u root -S /tmp/mysql.sock -proot_password -e
#   "GRANT SHUTDOWN ON *.* TO multi_admin\@localhost IDENTIFIED BY 'multipass'"
#   You will have to do the above for each mysqld running in each data
#   directory, that you have (just change the socket, -S=...)
#   See more detailed information from chapter:
#   '6 The MySQL Access Privilege System' from the MySQL manual.
# 2.pid-file is very important, if you are using mysqld_safe to start mysqld
#   (e.g. --mysqld=mysqld_safe) Every mysqld should have it's own pid-file.
#   The advantage using mysqld_safe instead of mysqld directly here is, that
#   mysqld_safe 'guards' every mysqld process and will restart it, if mysqld
#   process fails due to signal kill -9, or similar. (Like segmentation fault,
#   which MySQL should never do, of course ;) Please note that mysqld_safe
#   script may require that you start it from a certain place. This means that
#   you may have to CD to a certain directory, before you start the
#   mysqld_multi. If you have problems starting, please see the script.
#   Check especially the lines:
#   --------------------------------------------------------------------------
#   MY_PWD=`pwd`
#   Check if we are starting this relative (for the binary release)
#   if test -d $MY_PWD/data/mysql -a -f ./share/mysql/english/errmsg.sys -a \
#   -x ./bin/mysqld
#   --------------------------------------------------------------------------
#   The above test should be successful, or you may encounter problems.
# 3.Beware of the dangers starting multiple mysqlds in the same data directory.
#   Use separate data directories, unless you *KNOW* what you are doing!
# 4.The socket file and the TCP/IP port must be different for every mysqld.
# 5.The first and fifth mysqld was intentionally left out from the example.
#   You may have 'gaps' in the config file. This gives you more flexibility.
#   The order in which the mysqlds are started or stopped depends on the order
#   in which they appear in the config file.
# 6.When you want to refer to a certain group with GNR with this program,
#   just use the number in the end of the group name ( [mysqld# <== )
# 7.You may want to use option '--user' for mysqld, but in order to do this
#   you need to be root when you start this script. Having the option
#   in the config file doesn't matter; you will just get a warning, if you are
#   not the superuser and the mysqlds are started under *your* unix account.
#   IMPORTANT: Make sure that the pid-file and the data directory are
#   read+write(+execute for the latter one) accessible for *THAT* UNIX user,
#   who the specific mysqld process is started as. *DON'T* use the UNIX root
#   account for this, unless you *KNOW* what you are doing!
# 8.MOST IMPORTANT: Make sure that you understand the meanings of the options
#   that are passed to the mysqlds and why *WOULD YOU WANT* to have separate
#   mysqld processes. Starting multiple mysqlds in one data directory *WON'T*
#   give you extra performance in a threaded system!
#
[mysqld_multi]
mysqld     = @bindir@/mysqld_safe
mysqladmin = @bindir@/mysqladmin
user       = root
password   = your_password

[mysqld2]
socket     = /tmp/mysql.sock2
port       = 3307
pid-file   = @localstatedir@2/hostname.pid2
datadir    = @localstatedir@2
language   = @datadir@/mysql/english
user       = john

[mysqld3]
socket     = /tmp/mysql.sock3
port       = 3308
pid-file   = @localstatedir@3/hostname.pid3
datadir    = @localstatedir@3
language   = @datadir@/mysql/swedish
user       = monty

[mysqld4]
socket     = /tmp/mysql.sock4
port       = 3309
pid-file   = @localstatedir@4/hostname.pid4
datadir    = @localstatedir@4
language   = @datadir@/mysql/estonia
user       = tonu
 

[mysqld6]
socket     = /tmp/mysql.sock6
port       = 3311
pid-file   = @localstatedir@6/hostname.pid6
datadir    = @localstatedir@6
language   = @datadir@/mysql/japanese
user       = jani
EOF
  exit(0);
}

####
#### usage
####

sub usage
{
  print <<EOF;
$my_progname version $VER by Jani Tolonen

This software comes with ABSOLUTELY NO WARRANTY. This is free software,
and you are welcome to modify and redistribute it under the GPL license.

Description:
$my_progname can be used to start, or stop any number of separate
mysqld processes running in different TCP/IP ports and UNIX sockets.

This program can read group [mysqld_multi] from my.cnf file.
You may want to put options mysqld=... and mysqladmin=... there.

The program will search for group(s) named [mysqld#] from my.cnf (or
the given --config-file=...), where # can be any positive number
starting from 1. These groups should be the same as the usual [mysqld]
group (e.g. options to mysqld, see MySQL manual for detailed
information about this group), but with those port, socket
etc. options that are wanted for each separate mysqld processes. The
number in the group name has another function; it can be used for
starting, stopping, or reporting some specific mysqld servers with
this program. See the usage and options below for more information.

Usage: $my_progname [OPTIONS] {start|stop|report} [GNR,GNR,GNR...]
or     $my_progname [OPTIONS] {start|stop|report} [GNR-GNR,GNR,GNR-GNR,...]

The GNR above means the group number. You can start, stop or report
any GNR, or several of them at the same time. (See --example) The GNRs
list can be comma separated, or a dash combined, of which the latter
means that all the GNRs between GNR1-GNR2 will be affected. Without
GNR argument all the found groups will be either started, stopped, or
reported. Note that you must not have any white spaces in the GNR
list. Anything after a white space are ignored.

Options:
--config-file=...  Alternative config file.
                   Using: $opt_config_file
--example          Give an example of a config file.
--help             Print this help and exit.
--log=...          Log file. Full path to and the name for the log file. NOTE:
                   If the file exists, everything will be appended.
                   Using: $opt_log
--mysqladmin=...   mysqladmin binary to be used for a server shutdown.
                   Using: $mysqladmin
--mysqld=...       mysqld binary to be used. Note that you can give mysqld_safe
                   to this option also. The options are passed to mysqld. Just
                   make sure you have mysqld in your PATH or fix mysqld_safe.
                   Using: $mysqld
                   Please note: Since mysqld_multi version 2.3 you can also
                   give this option inside groups [mysqld#] in ~/.my.cnf,
                   where '#' stands for an integer (number) of the group in
                   question. This will be recognized as a special option and
                   will not be passed to the mysqld. This will allow one to
                   start different mysqld versions with mysqld_multi.
--no-log           Print to stdout instead of the log file. By default the log
                   file is turned on.
--password=...     Password for user for mysqladmin.
--silent           Disable warnings.
--tcp-ip           Connect to the MySQL server(s) via the TCP/IP port instead
                   of the UNIX socket. This affects stopping and reporting.
                   If a socket file is missing, the server may still be
                   running, but can be accessed only via the TCP/IP port.
                   By default connecting is done via the UNIX socket.
--user=...         MySQL user for mysqladmin. Using: $opt_user
--verbose          Be more verbose.
--version          Print the version number and exit.
EOF
  exit(0);
}
