#!@PERL@

use Getopt::Long;
use POSIX qw(strftime);

$|=1;
$VER="2.2";

$opt_config_file   = undef();
$opt_example       = 0;
$opt_help          = 0;
$opt_log           = "/tmp/mysqld_multi.log";
$opt_mysqladmin    = "mysqladmin";
$opt_mysqld        = "@libexecdir@/mysqld";
$opt_no_log        = 0;
$opt_password      = undef();
$opt_tcp_ip        = 0;
$opt_user          = "root";
$opt_version       = 0;

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
  }
  my @defops = `my_print_defaults mysqld_multi`;
  chop @defops;
  splice @ARGV, 0, 0, @defops;
  GetOptions("help","example","version","mysqld=s","mysqladmin=s",
	     "config-file=s","user=s","password=s","log=s","no-log","tcp-ip")
  || die "Wrong option! See $my_progname --help for detailed information!\n";

  $groupids = $ARGV[1];

  if ($opt_version)
  {
    print "$my_progname version $VER by Jani Tolonen\n";
    exit(0);
  }
  example() if ($opt_example);
  if (!defined(($mysqld = my_which($opt_mysqld))))
  {
    print "Couldn't find the mysqld binary! Tried: $opt_mysqld\n";
    $flag_exit=1;
  }
  if (!defined(($mysqladmin = my_which($opt_mysqladmin))))
  {
    print "Couldn't find the mysqladmin binary! Tried: $opt_mysqladmin\n";
    $flag_exit=1;
  }
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
	      ($ARGV[0] ne 'start' && $ARGV[0] ne 'START' &&
	       $ARGV[0] ne 'stop' && $ARGV[0] ne 'STOP' &&
	       $ARGV[0] ne 'report' && $ARGV[0] ne 'REPORT'));

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
  if ($ARGV[0] eq 'report' || $ARGV[0] eq 'REPORT')
  {
    report_mysqlds();
  }
  elsif ($ARGV[0] eq 'start' || $ARGV[0] eq 'START')
  {
    start_mysqlds();
  }
  else
  {
    stop_mysqlds();
  }
}

####
#### Report living and not running MySQL servers
####

sub report_mysqlds
{
  my (@groups, $com, $i, @options, $j, $pec);

  print "Reporting MySQL servers\n";
  if (!$opt_no_log)
  {
    w2log("\nReporting MySQL servers","$opt_log",0,0);
  }
  @groups = &find_groups($groupids);
  for ($i = 0; defined($groups[$i]); $i++)
  {
    $com = "my_print_defaults";
    $com.= defined($opt_config_file) ? " --config-file=$opt_config_file" : "";
    $com.= " $groups[$i]";
    @options = `$com`;
    chop @options;

    $com = "$mysqladmin -u $opt_user";
    $com.= defined($opt_password) ? " -p$opt_password" : "";
    $com.= $opt_tcp_ip ? " -h 127.0.0.1" : "";
    for ($j = 0; defined($options[$j]); $j++)
    {
      if ((($options[$j] =~ m/^(\-\-socket\=)(.*)$/) && !$opt_tcp_ip) ||
	  ($options[$j] =~ m/^(\-\-port\=)(.*)$/))
      {
	$com.= " $options[$j]";
      }
    }
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
  my (@groups, $com, $i, @options, $j);

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

    $com = "$mysqld";
    for ($j = 0; defined($options[$j]); $j++)
    {
      $com.= " $options[$j]";
    }
    $com.= " >> $opt_log 2>&1" if (!$opt_no_log);
    $com.= " &";
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
  my (@groups, $com, $i, @options, $j);

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
    $com = "my_print_defaults";
    $com.= defined($opt_config_file) ? " --config-file=$opt_config_file" : "";
    $com.= " $groups[$i]";
    @options = `$com`;
    chop @options;

    $com = "$mysqladmin -u $opt_user";
    $com.= defined($opt_password) ? " -p$opt_password" : "";
    $com.= $opt_tcp_ip ? " -h 127.0.0.1" : "";
    for ($j = 0; defined($options[$j]); $j++)
    {
      if ((($options[$j] =~ m/^(\-\-socket\=)(.*)$/) && !$opt_tcp_ip) ||
	  ($options[$j] =~ m/^(\-\-port\=)(.*)$/))
      {
	$com.= " $options[$j]";
      }
    }
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
# 2.pid-file is very important, if you are using safe_mysqld to start mysqld
#   (e.g. --mysqld=safe_mysqld) Every mysqld should have it's own pid-file.
#   The advantage using safe_mysqld instead of mysqld directly here is, that
#   safe_mysqld 'guards' every mysqld process and will restart it, if mysqld
#   process fails due to signal kill -9, or similar. (Like segmentation fault,
#   which MySQL should never do, of course ;) Please note that safe_mysqld
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
mysqld     = @bindir@/safe_mysqld
mysqladmin = @bindir@/mysqladmin
user       = multi_admin
password   = multipass

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
--config-file=...  Alternative config file. NOTE: This will not affect
                   this program's own options (group [mysqld_multi]),
                   but only groups [mysqld#]. Without this option everything
                   will be searched from the ordinary my.cnf file.
                   Using: $opt_config_file
--example          Give an example of a config file. (PLEASE DO CHECK THIS!)
--help             Print this help and exit.
--log=...          Log file. Full path to and the name for the log file. NOTE:
                   If the file exists, everything will be appended.
                   Using: $opt_log
--mysqladmin=...   mysqladmin binary to be used for a server shutdown.
                   Using: $mysqladmin
--mysqld=...       mysqld binary to be used. Note that you can give safe_mysqld
                   to this option also. The options are passed to mysqld. Just
                   make sure you have mysqld in your PATH or fix safe_mysqld.
                   Using: $mysqld
--no-log           Print to stdout instead of the log file. By default the log
                   file is turned on.
--password=...     Password for user for mysqladmin.
--tcp-ip           Connect to the MySQL server(s) via the TCP/IP port instead
                   of the UNIX socket. This affects stopping and reporting.
                   If a socket file is missing, the server may still be
                   running, but can be accessed only via the TCP/IP port.
                   By default connecting is done via the UNIX socket.
--user=...         MySQL user for mysqladmin. Using: $opt_user
--version          Print the version number and exit.
EOF
  exit(0);
}
