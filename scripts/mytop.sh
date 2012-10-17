#!/usr/bin/perl -w
#
# $Id: mytop,v 1.91 2012/01/18 16:49:12 mgrennan Exp $

=pod

=head1 NAME

mytop - display MySQL server performance info like `top'

=cut

## most of the POD is at the bottom of the file

use 5.005;
use strict;
use DBI;
use Getopt::Long;
use Socket;
use List::Util qw(min max);
use File::Basename;

$main::VERSION = "1.91a";
my $path_for_script= dirname($0);

$|=1;
$0 = 'mytop';

my $WIN = ($^O eq 'MSWin32') ? 1 : 0;

## Test for color support.

eval { require Term::ANSIColor; };

my $HAS_COLOR = $@ ? 0 : 1;

$HAS_COLOR = 0 if $WIN;

## Test of Time::HiRes support

eval { require Time::HiRes };

my $HAS_TIME = $@ ? 0 : 1;

my $debug = 0;

## Try to lower our priority (which, who, pri)

setpriority(0,0,10) unless $WIN;

## Prototypes

sub Clear();
sub GetData();
sub GetQPS();
sub FullQueryInfo($);
sub Explain($);
sub PrintTable(@);
sub PrintHelp();
sub Sum(@);
sub commify($);
sub make_short($);
sub Hashes($);
sub Execute($);
sub StringOrRegex($);
sub GetInnoDBStatus();
sub GetCmdSummary();
sub GetShowVariables();
sub GetShowStatus();
sub cmd_s;
sub cmd_S;
sub cmd_q;
sub FindProg($);

## Default Config Values

my %config = (
    batchmode     => 0,
    color         => 1,
    db            => 'test',
    delay         => 5,
    filter_user   => qr/.?/,
    filter_db     => qr/.?/,
    filter_host   => qr/.?/,
    filter_state  => qr/.?/,
    header        => 1,
    help          => 0,
    host          => 'localhost',
    idle          => 1,
    long	  => 120,
    long_nums     => 0,
    mode          => 'top',
    prompt        => 0,
    pass          => '',
    port          => 3306,
    resolve       => 0,
    slow	  => 10,	# slow query time
    socket        => '',
    sort          => 1,         # default or reverse sort ("s")
    user          => 'root',
    fullqueries   => 0
);

my %qcache = ();    ## The query cache--used for full query info support.
my %ucache = ();    ## The user cache--used for full killing by user
my %dbcache = ();   ## The db cache.  This should be merged at some point.
my %statcache = (); ## The show status cache for GetShowStatus()

my (%STATUS, %OLD_STATUS); # header stuff.

my $CLEAR = $WIN ? '': `clear`;

## Term::ReadKey values

my $RM_RESET   = 0;
my $RM_NOBLKRD = 3; ## using 4 traps Ctrl-C :-(

# Add options from .my.cnf first

my $my_print_defaults;
if (!defined($my_print_defaults=my_which("my_print_defaults")))
{
  print "Warning: Can't find my_print_defaults. Please add it to your PATH!\n";
  exit(1);
}

unshift @ARGV, split "\n", `$my_print_defaults client mytop`;

## Read the user's config file, if it exists.

my $config = "$ENV{HOME}/.mytop";

if (-e $config)
{
    if (open CFG, "<$config")
    {
        while (<CFG>)
        {
            next if /^\s*$/;  ## skip blanks
            next if /^\s*#/;  ## skip comments

            chomp;

	    if (/(\S+)\s*=\s*(.*\S)/)
            {
                $config{lc $1} = $2 if exists $config{lc $1};
            }
        }
        close CFG;
    }
}

## Command-line args.

use vars qw($opt_foo);

Getopt::Long::Configure('no_ignore_case', 'bundling');

GetOptions(
    "color!"              => \$config{color},
    "user|u=s"            => \$config{user},
    "pass|password|p=s"   => \$config{pass},
    "database|db|d=s"     => \$config{db},
    "host|h=s"            => \$config{host},
    "port|P=i"            => \$config{port},
    "socket|S=s"          => \$config{socket},
    "delay|s=i"           => \$config{delay},
    "batch|batchmode|b"   => \$config{batchmode},
    "header!"             => \$config{header},
    "idle|i!"             => \$config{idle},
    "resolve|r!"          => \$config{resolve},
    "prompt!"             => \$config{prompt},
    "long=i"		  => \$config{long},
    "long_nums!"          => \$config{long_nums},
    "mode|m=s"            => \$config{mode},
    "slow=i"		  => \$config{slow},
    "sort=s"              => \$config{sort},
    "fullqueries|L!"      => \$config{fullqueries}
);

## User may have put the port with the host.

if ($config{host} =~ s/:(\d+)$//)
{
    $config{port} = $1;
}

## Don't use Term::ReadKey unless running interactively.

if (not $config{batchmode})
{
    require Term::ReadKey;
    Term::ReadKey->import();
}

## User may want to disable color.

if ($HAS_COLOR and not $config{color})
{
    $HAS_COLOR = 0;
}

if ($HAS_COLOR)
{
    import Term::ANSIColor ':constants';
}
else
{
    *RESET  = sub { };
    *YELLOW = sub { };
    *RED    = sub { };
    *MAGENTA  = sub { };
    *GREEN  = sub { };
    *BLUE   = sub { };
    *WHITE  = sub { };
    *BOLD   = sub { };
}

my $RESET  = RESET()   || '';
my $YELLOW = YELLOW()  || '';
my $RED    = RED()     || '';
my $MAGENTA  = MAGENTA()   || '';
my $GREEN  = GREEN()   || '';
my $BLUE   = BLUE()    || '';
my $WHITE  = WHITE()   || '';
my $BOLD   = BOLD()    || '';

## Connect

my $dsn;

## Socket takes precedence.

$dsn ="DBI:mysql:database=$config{db};mysql_read_default_group=mytop;";

if ($config{socket} and -S $config{socket})
{
    $dsn .= "mysql_socket=$config{socket}";
}
else
{
    $dsn .= "host=$config{host};port=$config{port}";
}

if ($config{prompt})
{
    print "Password: ";
    ReadMode(2);
    chomp($config{pass} = <STDIN>);
    ReadMode(0);
    print "\n";
}

my $dbh = DBI->connect($dsn, $config{user}, $config{pass},
                       { PrintError => 0 });

if (not ref $dbh)
{
    my $Error = <<EODIE
Cannot connect to MySQL server. Please check the:

  * database you specified "$config{db}" (default is "test")
  * username you specified "$config{user}" (default is "root")
  * password you specified "$config{pass}" (default is "")
  * hostname you specified "$config{host}" (default is "localhost")
  * port you specified "$config{port}" (default is 3306)
  * socket you specified "$config{socket}" (default is "")

The options my be specified on the command-line or in a ~/.mytop
config file. See the manual (perldoc mytop) for details.

Here's the exact error from DBI. It might help you debug:

$DBI::errstr

EODIE
;

    die $Error;

}

ReadMode($RM_RESET) unless $config{batchmode};

## Get static data

my $db_version;
my $db_release;
my $server="MySQL";
my $have_query_cache;

my @variables = Hashes("show variables");

foreach (@variables)
{
    if ($_->{Variable_name} eq "version")
    {
        $db_version = $_->{Value};
	$db_version =~ /(\d+)/;
        $db_release= $1;
        $server="MariaDB" if ($db_version =~ /maria/i);
        next;
    }
    if ($_->{Variable_name} eq "have_query_cache")
    {
#        if ($_->{Value} eq 'YES')
	if ($_->{Value} eq 'YES' or $_->{Value} eq 'DEMAND')  # http://freshmeat.net/users/jerjones
        {
            $have_query_cache = 1;
        }
        else
        {
            $have_query_cache = 0;
        }
        next;
    }
}

#########################################################################
##
## The main loop
##
#########################################################################

ReadMode($RM_NOBLKRD)  unless $config{batchmode};

while (1)
{
    my $key;

    if ($config{mode} eq 'qps')
    {
        GetQPS();
        $key = ReadKey(1);

        next unless $key;

        if ($key =~ /t/i)
        {
            $config{mode} = 'top';
        }
        if ($key =~ /q/)
        {
            cmd_q();
        }
        next;
    }
    if ($config{mode} eq 'top')
    {
        GetData();
        last if $config{batchmode};
        $key = ReadKey($config{delay});
        next unless $key;
    }
    elsif ($config{mode} eq 'cmd')
    {
        GetCmdSummary();
        last if $config{batchmode};
        $key = ReadKey($config{delay});
        next unless $key;
    }
    elsif ($config{mode} eq 'innodb')
    {
        GetInnoDBStatus();
        last if $config{batchmode};
        $key = ReadKey($config{delay});
        next unless $key;
    }
    elsif ($config{mode} eq 'status')
    {
        GetShowStatus();
        last if $config{batchmode};
        $key = ReadKey($config{delay});
        next unless $key;
    }

    ##
    ## keystroke command processing (if we get this far)
    ##

    if ($key eq '!')
    {
        Execute("stop slave");
	Execute("set global sql_slave_skip_counter=1");
	Execute("start slave");
    }

    # t - top

    if ($key =~ /t/i)
    {
        $config{mode} = 'top';
    }

    ## q - quit

    if ($key eq 'q')
    {
        cmd_q();
    }

    if ($key eq 'D')
    {
        require Data::Dumper;
        print Data::Dumper::Dumper([\%config]);
        ReadKey(0);
    }

    ## l - change long running hightling

    if ($key eq 'l')
    {
        cmd_l();
        next;
    }

    ## m - mode switch to qps

    if ($key eq 'm')
    {
        $config{mode} = 'qps';
        Clear() unless $config{batchmode};
        print "Queries Per Second [hit q to exit this mode]\n";
        next;
    }

    ## c - mode switch to command summary

    if ($key eq 'c')
    {
        $config{mode} = 'cmd';
        Clear() unless $config{batchmode};
        print "Command Summary [hit q to exit this mode]\n";
        next;
    }

    ## C - change Color on and off

    if ($key eq 'C')
    {
	if ( $HAS_COLOR ) 
        {
	    $HAS_COLOR = 0;
	}
	else
	{
	    $HAS_COLOR = 1;
	}
    }

    ## s - seconds of delay

    if ($key eq 's')
    {
        cmd_s();
        next;
    }

    if ($key eq 'S')
    {
	cmd_S();
	next;
    }

    ## R - resolve hostnames
    if ($key eq 'R')
    {
        if ($config{resolve})
        {
            $config{resolve} = 0;
        }
        else
        {
            $config{resolve} = 1;
        }
    }

    ## t - username based filter

    if ($key eq 't')
    {
        ReadMode($RM_RESET);
        print RED(), "Which state (blank for all, /.../ for regex): ", RESET();
        $config{filter_state} = StringOrRegex(ReadLine(0));
        ReadMode($RM_NOBLKRD);
        next;
    }

    ## u - username based filter

    if ($key eq 'u')
    {
        ReadMode($RM_RESET);
        print RED(), "Which user (blank for all, /.../ for regex): ", RESET();
        $config{filter_user} = StringOrRegex(ReadLine(0));
        ReadMode($RM_NOBLKRD);
        next;
    }

    ## d - database name based filter

    if ($key eq 'd')
    {
        ReadMode($RM_RESET);
        print RED(), "Which database (blank for all, /.../ for regex): ",
            RESET();
        $config{filter_db} = StringOrRegex(ReadLine(0));
        ReadMode($RM_NOBLKRD);
        next;
    }

    ## h - hostname based filter

    if ($key eq 'h')
    {
        ReadMode($RM_RESET);
        print RED(), "Which hostname (blank for all, /.../ for regex): ",
            RESET();
        $config{filter_host} = StringOrRegex(ReadLine(0));
        ReadMode($RM_NOBLKRD);
        next;
    }

    ## E - Show full Replication Error

    if ($key eq 'E')
    {
	my($data) = Hashes('SHOW SLAVE STATUS');
	Clear();
	print "Error is: $data->{Last_Error}\n";
        print RED(), "-- paused. press any key to resume --", RESET();
	ReadKey(0);
	next;
    }
    ## F - remove all filters

    if ($key eq 'F')
    {
        $config{filter_host}  = qr/.?/;
        $config{filter_db}    = qr/.?/;
        $config{filter_user}  = qr/.?/;
        $config{filter_state} = qr/.?/;
        print RED(), "-- display unfiltered --", RESET();
        sleep 1;
        next;
    }

    ## p - pause

    if ($key eq 'p')
    {
        print RED(), "-- paused. press any key to resume --", RESET();
        ReadKey(0);
        next;
    }

    ## i - idle toggle

    if ($key =~ /i/)
    {
        if ($config{idle})
        {
            $config{idle} = 0;
            $config{sort} = 1;
            print RED(), "-- idle (sleeping) processed filtered --", RESET();
            sleep 1;
        }
        else
        {
            $config{idle} = 1;
            $config{sort} = 0;
            print RED(), "-- idle (sleeping) processed unfiltered --", RESET();
            sleep 1;
        }
    }

    ## I - InnoDB status

    if ($key =~ 'I')
    {
        $config{mode} = 'innodb';
        Clear() unless $config{batchmode};
        print "InnoDB Status [hit q to exit this mode]\n";
        next;
    }

    ## o - sort order

    if ($key =~ /o/)
    {
        if ($config{sort})
        {
            $config{sort} = 0;
            print RED(), "-- sort order reversed --", RESET();
            sleep 1;
        }
        else
        {
            $config{sort} = 1;
            print RED(), "-- sort order reversed --", RESET();
            sleep 1;
        }
    }

    ## ? - help

    if ($key eq '?')
    {
        Clear();
        PrintHelp();
        ReadKey(0);
        next;
    }

    ## k - kill

    if ($key eq 'k')
    {
        ReadMode($RM_RESET);

        print RED(), "Thread id to kill: ", RESET();
        my $id = ReadLine(0);

        $id =~ s/\s//g;

        if ($id =~ /^\d+$/)
        {
            Execute("KILL $id");
        }
        else
        {
            print RED(), "-- invalid thread id --", RESET();
            sleep 1;
        }

        ReadMode($RM_NOBLKRD);
        next;
    }

    ## K - kill based on a username
    if ($key =~ /K/)
    {
        ReadMode($RM_RESET);

        print RED(), "User to kill: ", RESET();
        my $user = ReadLine(0);

        $user =~ s/\s//g;

        if ($user =~ /^\S+$/)
        {
            for my $pid (keys %ucache)
            {
                next unless $ucache{$pid} eq $user;
                Execute("KILL $pid");
                select(undef, undef, undef, 0.2);
            }
        }
        else
        {
            print RED(), "-- invalid thread id --", RESET();
            sleep 1;
        }

        ReadMode($RM_NOBLKRD);
    }

    ## f - full info

    if ($key =~ /f/)
    {
        ReadMode($RM_RESET);
        print RED(), "Full query for which thread id: ", RESET();
        my $id = ReadLine(0);
        chomp $id;
        FullQueryInfo($id);
        ReadMode($RM_NOBLKRD);
        print RED(), "-- paused. press any key to resume or (e) to explain --",
            RESET();
        my $key = ReadKey(0);

        if ($key eq 'e')
        {
            Explain($id);
            print RED(), "-- paused. press any key to resume --", RESET();
            ReadKey(0);
        }

        next;
    }

    ## e - explain

    if ($key =~ /e/)
    {
        ReadMode($RM_RESET);
        print RED(), "Explain which query (id): ", RESET();
        my $id = ReadLine(0);
        chomp $id;
        Explain($id);
        ReadMode($RM_NOBLKRD);
        print RED(), "-- paused. press any key to resume --", RESET();
        ReadKey(0);
        next;
    }

    ## r - reset status counters

    if ($key =~ /r/)
    {
        Execute("FLUSH STATUS");
        print RED(), "-- counters reset --", RESET();
        sleep 1;
        next;
    }

    ## H - header toggle

    if ($key eq 'H')
    {
        if ($config{header})
        {
            $config{header} = 0;
        }
        else
        {
            $config{header}++;
        }
    }

    ## # - magic debug key

    if ($key eq '#')
    {
        $debug = 1;
    }

    if ($key eq 'V')
    {
        GetShowVariables();
        print RED(), "-- paused. press any key to resume --", RESET();
        ReadKey(0);
    }

    # Switch to show status mode

    if ($key eq 'M')
    {
        $config{mode} = 'status';
    }

   ## L - full queries toggle

    if ($key eq 'L')
    {
        if ($config{fullqueries})
        {
            $config{fullqueries} = 0;
            print RED(), "-- full queries OFF --", RESET();
            sleep 1;
        }
        else
        {
            $config{fullqueries} = 1;
            print RED(), "-- full queries ON --", RESET();
            sleep 1;
        }
    }

}

ReadMode($RM_RESET) unless $config{batchmode};

exit;

#######################################################################

sub Clear()
{
    if (not $WIN)
    {
        print "$CLEAR"
    }
    else
    {
        print "\n" x 90; ## dumb hack for now. Anyone know how to
                         ## clear the screen in dos window on a Win32
                         ## system??
    }
}

my $last_time;

sub GetData()
{
    ## Get terminal info
    my $now_time;
    %qcache = ();  ## recycle memory
    %dbcache = ();

    my ($width, $height, $wpx, $hpx, $lines_left);

    if (not $config{batchmode})
    {
        ($width, $height, $wpx, $hpx) = GetTerminalSize();
        $lines_left = $height - 2;
    }
    else
    {
        $height = 999_999;     ## I hope you don't have more than that!
        $lines_left = 999_999;
        $width = 80;
    }

    ##
    ## Header stuff.
    ##
    if ($config{header})
    {
        my @recs = "";
        if ( $db_release > 4 ) 
	{
            @recs = Hashes("show global status");
	} 
	else 
	{
           @recs = Hashes("show status");
	}

        ## if the server died or we lost connectivity
        if (not @recs)
        {
            ReadMode($RM_RESET);
            exit 1;
        }

        ## get high-res or low-res time
        my ($t_delta);

        if ($HAS_TIME)
        {
            $now_time = Time::HiRes::gettimeofday();
        }
        else
        {
            $now_time = time;
        }

        if ($last_time and $last_time != $now_time)
        {
          $t_delta = $now_time - $last_time;
        }

        %OLD_STATUS = %STATUS;

	# Set some status that may not exist in all versions
	$STATUS{Handler_tmp_write}= 0;
	$STATUS{Handler_tmp_update}= 0;
	$STATUS{Rows_tmp_read}= 0;

        foreach my $ref (@recs)
        {
            my $key = $ref->{Variable_name};
            my $val = $ref->{Value};

            $STATUS{$key} = $val;
        }

        ## Compute Key Cache Hit Stats

        $STATUS{Key_read_requests} ||= 1; ## can't divide by zero next

        my $cache_hits_percent = (100-($STATUS{Key_reads}/$STATUS{Key_read_requests}) * 100);
        $cache_hits_percent = sprintf("%2.2f",$cache_hits_percent);

        ## Query Cache info for <= Ver. 4.1
        ##
        ## mysql> show status like 'qcache%';
        ## +-------------------------+----------+
        ## | Variable_name           | Value    |
        ## +-------------------------+----------+
        ## | Qcache_queries_in_cache | 81       |
        ## | Qcache_inserts          | 4961668  |
        ## | Qcache_hits             | 1374170  |
        ## | Qcache_not_cached       | 5656249  |
        ## | Qcache_free_memory      | 33164800 |
        ## | Qcache_free_blocks      | 2        |
        ## | Qcache_total_blocks     | 168      |
        ## +-------------------------+----------+
	##
        ## Query Cache info for => Ver. 5.0
        ##
        ## mysql> show status like 'qcache%';
        ## +-------------------------+------------+
        ## | Variable_name           | Value      |
        ## +-------------------------+------------+
        ## | Qcache_free_blocks      | 37652      |
        ## | Qcache_free_memory      | 110289712  |
        ## | Qcache_hits             | 1460617356 |
        ## | Qcache_inserts          | 390563495  |
        ## | Qcache_lowmem_prunes    | 6414172    |
        ## | Qcache_not_cached       | 93002420   |
        ## | Qcache_queries_in_cache | 66558      |
        ## | Qcache_total_blocks     | 192031     |
        ## +-------------------------+------------+

        my $query_cache_hits             = 0;
        my $query_cache_hits_per_sec     = 0;
        my $now_query_cache_hits_per_sec = 0;

        if ($have_query_cache)
        {
            $query_cache_hits = $STATUS{Qcache_hits};
            $query_cache_hits_per_sec = $STATUS{Qcache_hits} / $STATUS{Uptime};

            if (defined $last_time and $last_time != $now_time)
            {
                my $q_delta = $STATUS{Qcache_hits} - $OLD_STATUS{Qcache_hits};
                $now_query_cache_hits_per_sec = sprintf "%.2f", $q_delta / $t_delta;
            }
        }

	open L, "</proc/loadavg";
	my $l = <L>;
	close L;
	chomp $l;

        $last_time = $now_time;

        ## Server Uptime in meaningful terms...

        my $time         = $STATUS{Uptime};
        my ($d,$h,$m,$s) = (0, 0, 0, 0);

        $d += int($time / (60*60*24)); $time -= $d * (60*60*24);
        $h += int($time / (60*60));    $time -= $h * (60*60);
        $m += int($time / (60));       $time -= $m * (60);
        $s += int($time);

        my $uptime = sprintf("%d+%02d:%02d:%02d", $d, $h, $m, $s);

        ## Queries per second...

        my $avg_queries_per_sec  = sprintf("%.2f", $STATUS{Questions} / $STATUS{Uptime});
        my $num_queries          = $STATUS{Questions};

        my @t = localtime(time);

        my $current_time = sprintf "[%02d:%02d:%02d]", $t[2], $t[1], $t[0];

        my $host_width = 50;
        my $up_width   = $width - $host_width - 1;
        Clear() unless $config{batchmode};
        print RESET();

        printf "%-.${host_width}s %${up_width}s\n",
               "$server on $config{host} ($db_version)",
               "up $uptime $current_time";
#              "load $l up $uptime $current_time";
        $lines_left--;


        printf " Queries: %-6s  qps: %4.0f Slow: %7s         Se/In/Up/De(%%):    %02.0f/%02.0f/%02.0f/%02.0f\n",
               make_short( $STATUS{Questions} ),  # q total
               $STATUS{Questions} / $STATUS{Uptime},  # qps, average
               make_short( $STATUS{Slow_queries} ),    # slow

               # hmm. a Qcache hit is really a select and should be counted.
               100 * ($STATUS{Com_select} + ($STATUS{Qcache_hits}||0) )    / $STATUS{Questions},
               100 * ($STATUS{Com_insert} +  $STATUS{Com_replace} ) / $STATUS{Questions},
               100 * ($STATUS{Com_update} )  / $STATUS{Questions},
               100 * $STATUS{Com_delete} / $STATUS{Questions};

        $lines_left--;

        if ($t_delta)
        {
          my $q_diff = ( $STATUS{Questions} - $OLD_STATUS{Questions} );
#          print("q_diff: $STATUS{Questions} - $OLD_STATUS{Questions}  / $t_delta = $q_diff\n");

          printf(" Sorts: %5.0f qps now: %4.0f Slow qps: %3.1f  Threads: %4.0f (%4.0f/%4.0f) %02.0f/%02.0f/%02.0f/%02.0f\n",
		 ( $STATUS{Sort_rows} - $OLD_STATUS{Sort_rows} ) / $t_delta, 
                 ( $STATUS{Questions} - $OLD_STATUS{Questions} ) / $t_delta,
                 ( # slow now (qps)
                  ($STATUS{Slow_queries} ) ?
                  ( $STATUS{Slow_queries} - $OLD_STATUS{Slow_queries} ) / $t_delta :
                  0
                 ),
                 $STATUS{Threads_connected},
                 $STATUS{Threads_running},
                 $STATUS{Threads_cached},

                 (100 * ($STATUS{Com_select} - $OLD_STATUS{Com_select} + 
                         ($STATUS{Qcache_hits}||0) - ($OLD_STATUS{Qcache_hits}||0)
                        ) ) / ($q_diff ),
                 (100 * ($STATUS{Com_insert} - $OLD_STATUS{Com_insert} +
                         $STATUS{Com_replace} - $OLD_STATUS{Com_replace}
                        ) ) / ($q_diff ),
                 (100 * ($STATUS{Com_update} - $OLD_STATUS{Com_update}) ) / ($q_diff ),
                 (100 * ($STATUS{Com_delete} - $OLD_STATUS{Com_delete}) ) / ($q_diff ),
                 );
        }
        else
        {
            print "\n";
        }
        $lines_left--;

        if ($have_query_cache and $STATUS{Com_select} and $query_cache_hits)
        {
          printf(" Cache Hits: %-5s Hits/s: %4.1f Hits now: %5.1f  Ratio: ",
                 make_short($STATUS{Qcache_hits}),        # cache hits
                 $STATUS{Qcache_hits} / $STATUS{Uptime}, # hits / sec
                 ($t_delta) ?  ($STATUS{Qcache_hits} - $OLD_STATUS{Qcache_hits}) / $t_delta : 0,  # Hits Now
                 );

          my($Ratio) =  100 * ($STATUS{Qcache_hits})  / ($STATUS{Qcache_hits} + $STATUS{Com_select} );
          if ($HAS_COLOR)
          {
                print YELLOW() if ($Ratio < 80.0);
                print RED() if ($Ratio < 50.0);
                print MAGENTA() if ($Ratio < 20.0);
          }
          printf("%4.1f%% ",$Ratio);
          if ($HAS_COLOR)
          {
                print RESET();
          }

          print " Ratio now: ";
          my($Ratio_now) = ($t_delta) ?   # ratio now
                 100 * ($STATUS{Qcache_hits} - $OLD_STATUS{Qcache_hits} ) /
                 ( ($STATUS{Com_select} + $STATUS{Qcache_hits} -
                    ($OLD_STATUS{Qcache_hits} + $OLD_STATUS{Com_select})
                   ) || 1) : 0;
          if ($HAS_COLOR)
          {
                print GREEN() if ($Ratio_now >= 80.0);
                print YELLOW() if ($Ratio_now < 80.0);
                print RED() if ($Ratio_now < 50.0);
                print MAGENTA() if ($Ratio_now < 20.0);
          }
          printf("%4.1f%% \n",$Ratio_now);
          if ($HAS_COLOR)
          {
                print RESET();
          }
        }
        $lines_left--;

        if ($t_delta)
	{
          my $rows_read;
          if (defined($STATUS{Rows_read}))
          {
            $rows_read= $STATUS{Rows_read} - $OLD_STATUS{Rows_read};
          }
          else
          {
            $rows_read=
              ($STATUS{Handler_read_first}+$STATUS{Handler_read_key}+
               $STATUS{Handler_read_next}+$STATUS{Handler_read_prev}+
               $STATUS{Handler_read_rnd}+$STATUS{Handler_read_rnd_next} -
               $OLD_STATUS{Handler_read_first}-$OLD_STATUS{Handler_read_key}-
               $OLD_STATUS{Handler_read_next}-$OLD_STATUS{Handler_read_prev}-
               $OLD_STATUS{Handler_read_rnd}-
               $OLD_STATUS{Handler_read_rnd_next});
          }
	  printf(" Handler: (R/W/U/D) %5d/%5d/%5d/%5d        Tmp: R/W/U: %5d/%5d/%5d\n",
		 $rows_read/$t_delta,
		 ($STATUS{Handler_write} - $OLD_STATUS{Handler_write}) /
		 $t_delta,
		 ($STATUS{Handler_update} - $OLD_STATUS{Handler_update}) /
		 $t_delta,
		 ($STATUS{Handler_delete} - $OLD_STATUS{Handler_delete}) /
		 $t_delta,
		 ($STATUS{Rows_tmp_read} - $OLD_STATUS{Rows_tmp_read}) /
		 $t_delta,
		 ($STATUS{Handler_tmp_write} 
		  -$OLD_STATUS{Handler_tmp_write})/$t_delta,
		 ($STATUS{Handler_tmp_update} -
		  $OLD_STATUS{Handler_tmp_update})/$t_delta);
	}
	else
        {
            print "\n";
        }

	$lines_left--;

        printf(" ISAM Key Efficiency: %2.1f%%  Bps in/out: %5s/%5s   ",
               $cache_hits_percent,
               make_short($STATUS{Bytes_received} / $STATUS{Uptime} ),
               make_short($STATUS{Bytes_sent} / $STATUS{Uptime}));
        printf("Now in/out: %5s/%5s",
               make_short(($STATUS{Bytes_received} - $OLD_STATUS{Bytes_received}) / $t_delta ),
               make_short(($STATUS{Bytes_sent} - $OLD_STATUS{Bytes_sent}) / $t_delta ))
          if ($t_delta);
        print "\n";

        $lines_left--;

        my($data) = Hashes('show global variables like "read_only"');
        if ($data->{Value} ne "OFF")
        {
            print RED() if ($HAS_COLOR) ;
            print " ReadOnly";
	    RESET() if ($HAS_COLOR);
        }

	($data) = Hashes('SHOW SLAVE STATUS');
	if (defined($data->{Master_Host}))
        {
	    if (defined($data->{Seconds_Behind_Master}))
	    {
                if ($HAS_COLOR) {
	  	    print GREEN();
		    print YELLOW() if ($data->{Seconds_Behind_Master}  >  60);
		    print MAGENTA() if ($data->{Seconds_Behind_Master} > 360);
	        }
	    }
	    print " Replication ";
	    print "IO:$data->{Slave_IO_Running} ";
 	    print "SQL:$data->{Slave_SQL_Running} ";
	    print RESET() if ($HAS_COLOR);

 	    if (defined($data->{Seconds_Behind_Master}))
	    {
        	if ($HAS_COLOR) {
			print GREEN();
			print YELLOW() if ($data->{Seconds_Behind_Master}  >  60);
			print MAGENTA() if ($data->{Seconds_Behind_Master} > 360);
		}
		print "Delay: $data->{Seconds_Behind_Master} sec.";
	    } else {
	        my $free = $width - 45;
		my $Err = substr $data->{Last_Error},0 ,$free;
	        printf(" ERR: %-${free}s", $Err) if ( $Err ne "" );
	    }
	    print WHITE() if ($HAS_COLOR);
	    print "\n";
	    $lines_left--;
	}
	print "\n";
    }

    if (not $config{batchmode} and not $config{header})
    {
        Clear();
        print RESET();
    }

    ##
    ## Threads
    ##

    my @sz   = (9, 8, 15, 9, 6, 5, 6, 8);
    my $used = scalar(@sz) + Sum(@sz);
    my $state= $width <= 80 ? 6 : int(min(6+($width-80)/3, 15));
    my $free = $width - $used - ($state - 6);
    my $format= "%9s %8s %15s %9s %6s %5s %6s %${state}s %-.${free}s\n";
    my $format2;
    if ($config{fullqueries})
    {
         $format2 = "%9d %8.8s %15.15s %9.9s %6d %5.1f %6.6s %${state}.${state}s %-${free}s\n";
    } else {
         $format2 = "%9d %8.8s %15.15s %9.9s %6d %5.1f %6.6s %${state}.${state}s %-${free}.${free}s\n";
    }
    print BOLD() if ($HAS_COLOR);

    printf $format,
        'Id','User','Host/IP','DB','Time', '%', 'Cmd', 'State', 'Query';

    print RESET() if ($HAS_COLOR);

    ##      Id User Host DB
    printf $format,
        '--','----','-------','--','----', '-', '---', '-----', '----------';

    $lines_left -= 2;

    my $proc_cmd = "show full processlist";

    my @data = Hashes($proc_cmd);

    foreach my $thread (@data)
    {
        last if not $lines_left;

        ## Drop Domain Name, unless it looks like an IP address.  If
        ## it's an IP, we'll strip the port number because it's rarely
        ## interesting.

        my $is_ip = 0;

        if ($thread->{Host} =~ /^(\d{1,3}\.){3}(\d{1,3})(:\d+)?$/)
        {
            $thread->{Host} =~ s/:.*$//;
            $is_ip = 1;
        }
        else
        {
            $thread->{Host} =~ s/^([^.]+).*/$1/;
        }

        ## Otherwise, look up the IP (if resolve is set) and strip the
        ## name
        if ($is_ip and $config{resolve})
        {
            $thread->{Host} =~ s/:\d+$//;
#	    my $host = $thread->{Host};
	    my $host = gethostbyaddr(inet_aton($thread->{Host}), AF_INET);
#            $host =~ s/^([^.]+).*/$1/;
            $thread->{Host} = $host;
        }

        ## Fix possible undefs

        $thread->{db}      ||= '';
        $thread->{Info}    ||= '';
        $thread->{Time}    ||= 0 ;
        $thread->{Id}      ||= 0 ;
        $thread->{User}    ||= '';
        $thread->{Command} ||= '';
        $thread->{Host}    ||= '';
	$thread->{State}   ||= "";
	$thread->{Progress} ||= 0;

	## alter double hyphen comments so they don't break 
	## the query when newlines are removed - http://freshmeat.net/users/jerjones
	$thread->{Info} =~ s~\s--(.*)$~ /* $1 */ ~mg; 

        ## Normalize spaces -- mostly disabled for now.  This can
        ## break EXPLAIN if you try to explain a mangled query.  It
        ## may be re-enabled later as an option.

        ## leading space removal
        $thread->{Info} =~ s/^\s*//;

        if (1)
        {
            ## remove newlines and carriage returns
            $thread->{Info} =~ s/[\n\r]//g;

            ## collpase whitespace
            $thread->{Info} =~ s/\s+/ /g;
        }

        ## stow it in the cache

        $qcache{$thread->{Id}}  = $thread->{Info};
        $dbcache{$thread->{Id}} = $thread->{db};
        $ucache{$thread->{Id}}  = $thread->{User};

    }

    ## Sort by idle time (closest thing to CPU usage I can think of).

    my @sorted;

    if (not $config{sort})
    {
        @sorted = sort { $a->{Time} <=> $b->{Time} } @data
    }
    else
    {
        @sorted = sort { $b->{Time} <=> $a->{Time} } @data
    }

    foreach my $thread (@sorted)
    {
        # Check to see if we can skip out.  We skip out if we know the
        # given line doesn't match.

        next if (($thread->{Command} eq "Sleep")
                 and
                 (not $config{idle}));

        next if (($thread->{Command} eq "Binlog Dump")
                 and
                 (not $config{idle}));

        next if (($thread->{Command} eq "Daemon")
                 and
                 (not $config{idle}));

        next if ($thread->{User}  !~ $config{filter_user});
        next if ($thread->{db}    !~ $config{filter_db});
        next if ($thread->{Host}  !~ $config{filter_host});
        next if ($thread->{State} !~ $config{filter_state});

        # Otherwise, print.

        my $smInfo;

        if ($thread->{Info})
        {
            if ($config{fullqueries})
            {
                $smInfo = $thread->{Info};
            } else {
                $smInfo = substr $thread->{Info}, 0, $free;
            }
        }
#        if ($thread->{State})
#        {
#            $smInfo = substr $thread->{State}, 0, $free;
#        }
        else
        {
            $smInfo = "";
        }

        if ($HAS_COLOR)
        {
            print YELLOW() if $thread->{Command} eq 'Query';
            print WHITE()  if $thread->{Command} eq 'Sleep';
            print GREEN()  if $thread->{Command} eq 'Connect';
            print BOLD() if $thread->{Time} > $config{slow};
	    print MAGENTA() if $thread->{Time} > $config{long};
        }

        printf $format2,
            $thread->{Id}, $thread->{User}, $thread->{Host}, $thread->{db},
            $thread->{Time}, $thread->{Progress}, $thread->{Command}, $thread->{State}, $smInfo;

        print RESET() if $HAS_COLOR;

        $lines_left--;

        last if $lines_left == 0;

    }

}

###########################################################################

my $questions;

sub GetQPS()
{
    my($data) = Hashes('SHOW STATUS LIKE "Questions"');
    my $num   = $data->{Value};

    if (not defined $questions) ## first time?
    {
        $questions = $num;
        return;
    }

    my $qps = $num - $questions;
    $questions = $num;
    print "$qps\n";
}

###########################################################################

sub GetQcacheSummary()
{
}

###########################################################################

sub GetInnoDBStatus()
{
    if (not $config{pager})
    {
        if (not $config{pager} = FindProg('less'))
        {
            $config{pager} = FindProg('more');
        }
    }

    my @data = Hashes("SHOW INNODB STATUS");

    open P, "|$config{pager}" or die "$!";
    print keys %{$data[0]};
    print $data[0]->{Status},"\n";
    close P;
}

###########################################################################

my %prev_data;

sub GetCmdSummary()
{
    my ($width, $height, $wpx, $hpx, $lines_left);

    if (not $config{batchmode})
    {
        ($width, $height, $wpx, $hpx) = GetTerminalSize();

        $lines_left = $height - 2;
    }
    else
    {
        $height = 999_999;     ## I hope you don't have more than that!
        $lines_left = 999_999;
        $width = 80;
    }

    # Variable_name and Value pairs come back...
    my @data = Hashes("SHOW STATUS LIKE 'Com_%'");
    my %cmd_data;
    my %cmd_delta;
    my %cmd_pct;
    my %cmd_delta_pct;
    my $total;
    my $delta_total;

    for my $item (@data)
    {
        next unless $item->{Value};
        $item->{Variable_name} =~ s/^Com_//;
        $item->{Variable_name} =~ s/_/ /g;
        $cmd_data{$item->{Variable_name}} = $item->{Value};
        $total += $item->{Value};
    }

    ## Populate other stats

    for my $item (keys %cmd_data)
    {
        $cmd_delta{$item} = $cmd_data{$item} -
            ($prev_data{$item} || $cmd_data{$item} - 1);

        $delta_total += $cmd_delta{$item};

        $cmd_pct{$item}  = int(($cmd_data{$item} / $total) * 100);
    }

    for my $item (keys %cmd_data)
    {
        $cmd_delta_pct{$item}  = int(($cmd_delta{$item} / $delta_total) * 100);
    }


    ## Display

    Clear() unless $config{batchmode};
    print RESET();
    printf "%18s %10s %4s  | %5s %4s\n", 'Command', 'Total', 'Pct', 'Last', 'Pct';
    printf "%18s %10s %4s  | %5s %4s\n", '-------', '-----', '---', '----', '---';
    $lines_left -= 2;

    for my $item (sort { $cmd_data{$b} <=> $cmd_data{$a} } keys %cmd_data)
    {
        printf "%18s %10d %4s  | %5d %4s\n",
            $item,
            $cmd_data{$item},
            $cmd_pct{$item} . "%",
            $cmd_delta{$item},
            $cmd_delta_pct{$item} . "%";

        last if not $lines_left;
        $lines_left -= 1;
    }

    %prev_data = %cmd_data;
}

###########################################################################

sub GetShowVariables()
{
    if (not $config{pager})
    {
        if (not $config{pager} = FindProg('less'))
        {
            $config{pager} = FindProg('more');
        }
    }

    my @rows = Hashes("SHOW VARIABLES");

    open P, "|$config{pager}" or die "$!";

    for my $row (@rows)
    {
        my $name  = $row->{Variable_name};
        my $value = $row->{Value};
        printf P "%32s: %s\n", $name, $value;
    }

    close P;
}

###########################################################################

sub GetShowStatus()
{
    Clear() unless $config{batchmode};
    my @rows = Hashes("SHOW STATUS");

    printf "%32s  %10s %10s   Toggle idle with 'i'\n", 'Counter', 'Total', 'Change';
    printf "%32s  %10s %10s\n", '-------', '-----', '------';

    for my $row (@rows)
    {
        my $name  = $row->{Variable_name};
        my $value = $row->{Value};
        my $old   = $statcache{$name};
        my $delta = 0;

        next if $name  =~ m/^Com_/;      ## skip Com_ stats
        next if $value =~ m/^[^0-9]*$/;  ## skip non-numeric

        ## TODO: if Qcache is off, we should skip Qcache_ values

        if ($HAS_COLOR and defined $old and $old =~ /^\d/)
        {
            if ($value > $old)
            {
                print YELLOW();
                $delta = $value - $old;
            }
            elsif ($value < $old)
            {
                print RED();
                $delta = $value - $old;
            }

            if (not $config{idle} and $value == $old)
            {
                # filter unchanging stats, maybe
                print RESET();
                next;
            }
        }

        printf "%32s: %10s %10s\n", $name, $value, $delta;
        print RESET() if $HAS_COLOR;

        $statcache{$name} = $value;
    }

}

###########################################################################

sub FullQueryInfo($)
{
    my $id = shift;

    if (not exists $qcache{$id} or not defined $qcache{$id})
    {
        print "*** Invalid id. ***\n";
        return;
    }

    my $sql = $qcache{$id};
    print $CLEAR;
    print "Thread $id was executing following query:\n\n";
    print YELLOW(), $sql,"\n\n", RESET();
}

###########################################################################

sub Explain($)
{
    my $id  = shift;

    if (not exists $qcache{$id} or not defined $qcache{$id})
    {
        print "*** Invalid id. ***\n";
        return;
    }

    my $sql = $qcache{$id};
    my $db  = $dbcache{$id};

    Execute("USE $db");
    my @info = Hashes("EXPLAIN $sql");
    print $CLEAR;
    print "EXPLAIN $sql:\n\n";
    PrintTable(@info);
}

###########################################################################

sub PrintTable(@)
{
    my $cnt = 1;
    my @cols = qw(table type possible_keys key key_len ref rows Extra);

    for my $row (@_)
    {
        print "*** row $cnt ***\n";
        for my $key (@cols)
        {
            my $val = $row->{$key} || 'NULL';
            printf "%15s:  %s\n", $key, $val;
        }
        $cnt++;
    }
}

###########################################################################

sub StringOrRegex($)
{
    my $input = shift;
    chomp $input;
    if (defined $input)
    {
        # regex, strip /.../ and use via qr//
        if ($input =~ m{^/} and $input =~ m{/$})
        {
            $input =~ s{^/}{} if $config{filter_user};
            $input =~ s{/$}{} if $config{filter_user};
            $input =  qr/$input/;
        }


        # reset to match anything
        elsif ($input eq '')
        {
            $input = qr/.*/;
        }

        # string, build a simple regex
        else
        {
            $input =  '^' . $input . '$';
            $input = qr/$input/;
        }
    }

    # reset to match anything
    else
    {
        $input = qr/.*/;
    }
    return $input;
}

###########################################################################

sub cmd_l
{
    ReadMode($RM_RESET);

    print RED(), "Seconds for long queries: ", RESET();
    my $secs = ReadLine(0);

    if ($secs =~ /^\s*(\d+)/)
    {
        $config{long} = $1;
        if ($config{long} < 1)
        {
            $config{long} = 1;
        }
    }
    ReadMode($RM_NOBLKRD);
}

sub cmd_s
{
    ReadMode($RM_RESET);

    print RED(), "Seconds of Delay: ", RESET();
    my $secs = ReadLine(0);

    if ($secs =~ /^\s*(\d+)/)
    {
        $config{delay} = $1;
        if ($config{delay} < 1)
        {
            $config{delay} = 1;
        }
    }
    ReadMode($RM_NOBLKRD);
}

sub cmd_S
{
    ReadMode($RM_RESET);

    print RED(), "Seconds for Slow queries: ", RESET();
    my $secs = ReadLine(0);

    if ($secs =~ /^\s*(\d+)/)
    {
        $config{slow} = $1;
        if ($config{slow} < 1)
        {
            $config{slow} = 1;
        }
    }
    ReadMode($RM_NOBLKRD);
}

sub cmd_q
{
    ReadMode($RM_RESET);
    print "\n";
    exit;
}

sub trim($)
{
	my $string = shift;
	$string =~ s/^\s+//;
	$string =~ s/\s+$//;
	return $string;
}

###########################################################################

sub PrintHelp()
{
    my $help = qq[
Help for mytop version $main::VERSION by Jeremy D. Zawodny <${YELLOW}Jeremy\@Zawodny.com${RESET}>
 with updates by Mark Grennan <${YELLOW}mark\@grennan.com${RESET}>

  ? - display this screen
  # - toggle short/long numbers (not yet implemented)
  c - command summary view (based on Com_* counters)
  C - turn color on and off
  d - show only a specific database
  e - explain the query that a thread is running
  E - display current replication error
  f - show full query info for a given thread
  F - unFilter the dispaly
  h - show only a specifc host's connections
  H - toggle the mytop header
  i - toggle the display of idle (sleeping) threads
  I - show innodb status
  k - kill a thread
  p - pause the display
  l - change long running queries hightlighing
  m - switch [mode] to qps (queries/sec) scrolling view
  M - switch [mode] to status
  o - reverse the sort order (toggle)
  q - quit
  r - reset the status counters (via FLUSH STATUS on your server)
  R - change reverse IP lookup
  s - change the delay between screen updates
  S - change slow quiery hightlighting
  t - switch to thread view (default)
  u - show only a specific user
  V - show variables
  : - enter a command (not yet implemented)
  ! - Skip an error that has stopped replications (at your own risk)
  L - show full queries (do not strip to terminal width)

Base version from ${GREEN}http://www.mysqlfanboy.com/mytop${RESET}
This version comes as part of the ${GREEN}MariaDB${RESET} distribution.
];

    print $help;
}

sub Sum(@)
{
    my $sum;
    while (my $val = shift @_) { $sum += $val; }
    return $sum;
}

## A useful routine from perlfaq

sub commify($)
{
    local $_  = shift;
    return 0 unless defined $_;
    1 while s/^([-+]?\d+)(\d{3})/$1,$2/;
    return $_;
}

## Compact numeric representation (10,000 -> 10.0k)

sub make_short($)
{
    my $number = shift;
    return commify($number) if $config{long_nums};
    my $n = 0;
    while ($number > 1_025) { $number /= 1024; $n++; };
    return sprintf "%.1f%s", $number, ('','k','M','G', 'T')[$n];
}



## Run a query and return the records has an array of hashes.

sub Hashes($)
{
    my $sql = shift;
    my @records;

    if (my $sth = Execute($sql))
    {
	while (my $ref = $sth->fetchrow_hashref)
        {
            print "record\n" if $debug;
	    push @records, $ref;
	}
    }
    return @records;
}

## Execute an SQL query and return the statement handle.

sub Execute($)
{
    my $sql = shift;
    my $sth = $dbh->prepare($sql);

    if (not $sth) { ReadMode($RM_RESET); die $DBI::errstr; }

    my $ReturnCode = $sth->execute;

    if (not $ReturnCode)
    {
        if ($debug)
        {
            print "query failed\n";
            sleep 10;
        }
        return undef;
    }

    return $sth;
}

sub FindProg($)
{
    my $prog  = shift;
    my $found = undef;
    my @search_dirs = ("/bin", "/usr/bin", "/usr/sbin",
                       "/usr/local/bin", "/usr/local/sbin");

    for (@search_dirs)
    {
        my $loc = "$_/$prog";
        if (-e $loc)
        {
            $found = $loc;
            last;
        }
    }
    return $found;
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

  # Check first if this is a source distribution, then if this binary
  # distribution and last in the path

  push @paths, "./extra";
  push @paths, $path_for_script;
  push @paths, split(':', $ENV{'PATH'});

  foreach $path (@paths)
  {
    $path .= "/$command";
    return $path if (-f $path && -x $path);
  }
  return undef();
}

=pod

=head1 SYNOPSIS

B<mytop> [options]

=head1 AVAILABILITY

Base version from B<http://www.mysqlfanboy.com/mytop>.

This version comes as part of the B<MariaDB> distribution. See B<http://mariadb.org/>.

And older (the original) version B<mytop> is available from
http://jeremy.zawodny.com/mysql/mytop/ it B<might> also be on CPAN as
well.

=head1 REQUIREMENTS

In order for B<mytop> to function properly, you must have the
following:

  * Perl 5.005 or newer
  * Getopt::Long
  * DBI and DBD::mysql
  * Term::ReadKey from CPAN

Most systems are likely to have all of those installed--except for
Term::ReadKey. You will need to pick that up from the CPAN. You can
pick up Term::ReadKey here:

    http://search.cpan.org/search?dist=TermReadKey

And you obviously need access to a MySQL server (version 3.22.x or
3.23.x) with the necessary security to run the I<SHOW PROCESSLIST> and
I<SHOW STATUS> commands.

If you are a Windows user, using ActiveState's Perl, you can use PPM
(the Perl Package Manager) to install the MySQL and Term::ReadKey
modules.

=head2 Optional Color Support

In additon, if you want a color B<mytop> (recommended), install
Term::ANSIColor from the CPAN:

    http://search.cpan.org/search?dist=ANSIColor

Once you do, B<mytop> will automatically use it. However, color is not
yet working on Windows. Patches welcome. :-)

=head2 Optional Hi-Res Timing

If you want B<mytop> to provide more accurate real-time
queries-per-second statistics, install the Time::HiRes module from
CPAN.  B<mytop> will automatically notice that you have it and use it
rather than the standard timing mechanism.

=head2 Platforms

B<mytop> is known to work on:

  * Linux (2.2.x, 2.4.x)
  * FreeBSD (2.2, 3.x, 4.x)
  * Mac OS X
  * BSDI 4.x
  * Solaris 2.x
  * Windows NT 4.x (ActivePerl)

If you find that it works on another platform, please let me
know. Given that it is all Perl code, I expect it to be rather
portable to Unix and Unix-like systems. Heck, it I<might> even work on
Win32 systems.

=head1 DESCRIPTION

Help is always welcome in improving this software. Feel free to
contact the author (see L<"AUTHOR"> below) with bug reports, fixes,
suggestions, and comments. Additionally L<"BUGS"> will provide a list
of things this software is not able to do yet.

Having said that, here are the details on how it works and what you can
do with it.

=head2 The Basics

B<mytop> was inspired by the system monitoring tool B<top>. I
routinely use B<top> on Linux, FreeBSD, and Solaris. You are likely to
notice features from each of them here.

B<mytop> will connect to a MySQL server and periodically run the
I<SHOW PROCESSLIST> and I<SHOW STATUS> commands and attempt to
summarize the information from them in a useful format.

=head2 The Display

The B<mytop> display screen is really broken into two parts. The top 4
lines (header) contain summary information about your MySQL
server. For example, you might see something like:

MySQL on localhost (4.0.13-log)                        up 1+11:13:00 [23:29:11]
 Queries: 19.3M  qps:  160 Slow:     1.0         Se/In/Up/De(%):    00/80/03/17
             qps now:  219 Slow qps: 0.0  Threads:    1 (   1/  16) 00/74/00/25
 Key Efficiency: 99.3%  Bps in/out: 30.5k/162.8   Now in/out: 32.7k/ 3.3k

The first line identifies the hostname of the server (localhost) and
the version of MySQL it is running. The right had side shows the
uptime of the MySQL server process in days+hours:minutes:seconds
format (much like FreeBSD's top) as well as the current time.

The second line displays the total number of queries the server has
processed, the average number of queries per second, the number of
slow queries, and the percentage of Select, Insert, Update, and Delete
queries.

The third real-time values. First is the number of queries per second,
then the number of slow queries, followed by query precentages (like
on the previous line).

And the fourth line displays key buffer efficiency (how often keys are
read from the buffer rather than disk) and the number of bytes that
MySQL has sent and received, both over all and in the last cycle.

You can toggle the header by hitting B<h> when running B<mytop>.

The second part of the display lists as many threads as can fit on
screen. By default they are sorted according to their idle time (least
idle first). The display looks like:

    Id     User       Host      Dbase   Time      Cmd Query or State
    --     ----       ----      -----   ----      --- --------------
    61  jzawodn  localhost      music      0    Query show processlist

As you can see, the thread id, username, host from which the user is
connecting, database to which the user is connected, number of seconds
of idle time, the command the thread is executing, and the query info
are all displayed.

Often times the query info is what you are really interested in, so it
is good to run B<mytop> in an xterm that is wider than the normal 80
columns if possible.

The thread display color-codes the threads if you have installed color
support. The current color scheme only works well in a window with a
dark (like black) background. The colors are selected according to the
C<Command> column of the display:

    Query   -  Yellow
    Sleep   -  White
    Connect -  Green
    Slow    -  Bright
    Long    -  Magenta

Those are purely arbitrary and will be customizable in a future
release. If they annoy you just start B<mytop> with the B<--nocolor>
flag or adjust your config file appropriately.

=head2 Arguments

B<mytop> handles long and short command-line arguments. Not all
options have both long and short formats, however. The long arguments
have two dashes `--'. Short arguments only have one '-'.

=over

=item B<-u> or B<-user> username

Username to use when logging in to the MySQL server. Default: ``root''.

=item B<-p> or B<-pass> or B<-password> password

Password to use when logging in to the MySQL server. Default: none.

=item B<-h> or B<--host> hostname[:port]

Hostname of the MySQL server. The hostname may be followed by an
option port number. Note that the port is specified separate from the
host when using a config file. Default: ``localhost''.

=item B<--port> or B<-P> port

If you're running MySQL on a non-standard port, use this to specify
the port number. Default: 3306.

=item B<-s> or B<--delay> seconds

How long between display refreshes. Default: 5

=item B<-d> or B<--db> or B<--database> database

Use if you'd like B<mytop> to connect to a specific database by
default. Default: ``test''.

=item B<-b> or B<--batch> or B<--batchmode>

In batch mode, mytop runs only once, does not clear the screen, and
places no limit on the number of lines it will print. This is suitable
for running periodically (perhaps from cron) to capture the
information into a file for later viewing. You might use batch mode in
a CGI script to occasionally display your MySQL server status on the
web.

Default: unset.

=item B<-S> or B<--socket> /path/to/socket

If you're running B<mytop> on the same host as MySQL, you may wish to
have it use the MySQL socket directly rather than a standard TCP/IP
connection. If you do,just specify one.

Note that specifying a socket will make B<mytop> ignore any host
and/or port that you might have specified. If the socket does not
exist (or the file specified is not a socket), this option will be
ignored and B<mytop> will use the hostname and port number instead.

Default: none.

=item B<--header> or B<--noheader>

Sepcify if you want the header to display or not. You can toggle this
with the B<h> key while B<mytop> is running.

Default: header.

=item B<--color> or B<--nocolor>

Specify if you want a color display. This has no effect if you don't
have color support available.

Default: If you have color support, B<mytop> will try color unless you
tell it not to.

=item B<-i> or B<--idle> or B<--noi> or B<--noidle>

Specify if you want idle (sleeping) threads to appear in the list. If
sleeping threads are omitted, the default sorting order is reversed so
that the longest running queries appear at the top of the list.

Default: idle.

=item B<--prompt> or B<--noprompt>

Specify if you want to be prompted to type in your database password.
This provides a little bit more security since it not only prevents
the password from viewable in a process list, but also doesn't require
the password to be stored in plain text in your ~/.mytop config file.
You will B<only> be prompted if a password has not been specified in
your config file or through another command line option.

Default: noprompt.

=item B<--resolve>

If you have skip-resolve set on MySQL (to keep it from doing a reverse
DNS lookup on each inbound connection), mytop can replace IP addresses
with hostnames but toggling this option.

Default: noresolve

=back

Command-line arguments will always take precedence over config file
options. That happens because the config file is read I<BEFORE> the
command-line arguments are applied.

=head2 Config File

Instead of always using bulky command-line parameters, you can also
use a config files for the default value of your options.

mytop will first read the [client] and [mytop] sections from your
my.cnf files. After that it will read the (C<~/.mytop>) file from your
home directory (if present). These are read I<before> any of your
command-line arguments are processed, so your command-line arguments
will override directives in the config file.


Here is a sample config file C<~/.mytop> which implements the defaults
described above.

  user=root
  pass=
  host=localhost
  db=test
  delay=5
  port=3306
  slow=10
  socket=
  batchmode=0
  header=1
  color=1
  idle=1
  long=120

Using a config file will help to ensure that your database password
isn't visible to users on the command-line. Just make sure that the
permissions on C<~/.mytop> are such that others cannot read it (unless
you want them to, of course).

You may have white space on either side of the C<=> in lines of the
config file.

=head2 Shortcut Keys

The following keys perform various actions while B<mytop> is
running. Those which have not been implemented are listed as
such. They are included to give the user idea of what is coming.

=over

=item B<?>

Display help.

=item B<c>

Show "command counters" based on the Com_* values in SHOW STATUS.
This is a new feature.  Feedback welcome.

=item B<C>

Turn display color on and off. Default is on.

=item B<d>

Show only threads connected to a particular database.

=item B<f>

Given a thread id, display the entire query that thread was (and still
may be) running.

=item B<F>

Disable all filtering (host, user, and db).

=item B<h>

Only show queries from a particular host.

=item B<H>

Toggle the header display. You can also specify either C<header=0> or
C<header=1> in your config file to set the default behavior.

=item B<i>

Toggle the display of idle (sleeping) threads. If sleeping threads are
filtered, the default sorting order is reversed so that the longest
running queries appear at the top of the list.

=item B<I>

Switch to InnoDB Status mode.  The output of "SHOW INNODB STATUS" will
be displayed every cycle.  In a future version, this may actually
summarize that data rather than producing raw output.

=item B<k>

Kill a thread.

=item B<m>

Toggle modes. Currently this switches from `top' mode to `qps'
(Queries Per Second Mode). In this mode, mytop will write out one
integer per second. The number written reflects the number of queries
executed by the server in the previous one second interval.

More modes may be added in the future.

=item B<o>

Reverse the default sort order.

=item B<p>

Pause display.

=item B<q>

Quit B<mytop>

=item B<r>

Reset the server's status counters via a I<FLUSH STATUS> command.

=item B<R>

Togle IP reverse lookup. Default is on.

=item B<s>

Change the sleep time (number of seconds between display refreshes).

=item B<S>

Set the number of seconds a query will need to run before it is
considered old and will be highlighted.

=item B<u>

Show only threads owned by a giver user.

=back

The B<s> key has a command-line counterpart: B<-s>.

The B<h> key has two command-line counterparts: B<-header> and
B<-noheader>.

=head1 BUGS

This is more of a BUGS + WishList.

Some performance information is not available when talking to a
version 3.22.x MySQL server. Additional information (about threads
mostly) was added to the output of I<SHOW STATUS> in MySQL 3.23.x and
B<mytop> makes use of it. If the information is not available, you
will simply see zeros where the real numbers should be.

Simply running this program will increase your overall counters (such
as the number of queries run). But you may or may not view that as a
bug.

B<mytop> consumes too much CPU time when running (verified on older
versions of Linux and FreeBSD). It's likely a problem related to
Term::ReadKey. I haven't had time to investigate yet, so B<mytop> now
automatically lowers its priority when you run it. You may also think
about running B<mytop> on another workstation instead of your database
server. However, C<mytop> on Solaris does B<not> have this problem.
Newer versions of Linux and FreeBSD seem to have fixed this.

You can't specify the maximum number of threads to list. If you have
many threads and a tall xterm, B<mytop> will always try to display as
many as it can fit.

The size of most of the columns in the display has a small maximum
width. If you have fairly long database/user/host names the display
may appear odd. I have no good idea as to how best to deal with that
yet. Suggestions are welcome.

It'd be nice if you could just add B<mytop> configuration directives
in your C<my.cnf> file instead of having a separate config file.

You should be able to specify the columns you'd like to see in the
display and the order in which they appear. If you only have one
username that connects to your database, it's probably not worth
having the User column appear, for example.

=head1 AUTHOR

mytop was developed and is maintained by Jeremy D. Zawodny
(Jeremy@Zawodny.com).

If you wish to e-mail me regarding this software, B<PLEASE> subscribe
to the B<mytop> mailing list.  See the B<mytop> homepage for details.

=head1 DISCLAIMER

While I use this software in my job at Yahoo!, I am solely responsible
for it. Yahoo! does not necessarily support this software in any
way. It is merely a personal idea which happened to be very useful in
my job.

=head1 SEE ALSO

Please check the MySQL manual if you're not sure where some of the
output of B<mytop> is coming from.

=head1 COPYRIGHT

Copyright (C) 2000-2010, Jeremy D. Zawodny.

=head1 CREDITS

Fix a bug. Add a feature. See your name here!

Many thanks go to these fine folks:

=over

=Item Jean Weisbuch

Added --fullqueries and reading of .my.cnf

=item Mark Grennan (mark@grennan.com) www.linuxfangoy.com

Added updates for MySQL 5.x. Added 'S' (slow) highlighting.
Added 'C' to turn on and off Color. Added 'l' command to change
color for long running queries. Fixed a few documentation issues.
Monitors Slave status. Added color to Queue hit ratio.
Added number of rows sorted per second.
Created release 1.7.

=item Sami Ahlroos (sami@avis-net.de)

Suggested the idle/noidle stuff.

=item Jan Willamowius (jan@janhh.shnet.org)

Mirnor bug report. Documentation fixes.

=item Alex Osipov (alex@acky.net)

Long command-line options, Unix socket support.

=item Stephane Enten (tuf@grolier.fr)

Suggested batch mode.

=item Richard Ellerbrock (richarde@eskom.co.za)

Bug reports and usability suggestions.

=item William R. Mattil (wrm@newton.irngtx.tel.gte.com)

Bug report about empty passwords not working.

=item Benjamin Pflugmann (philemon@spin.de)

Suggested -P command-line flag as well as other changes.

=item Justin Mecham <justin@aspect.net>

Suggested setting $0 to `mytop'.

=item Thorsten Kunz <thorsten.kunz@de.tiscali.com>

Provided a fix for cases when we try remove the domain name from the
display even if it is actually an IP address.

=item Sasha Pachev <sasha@mysql.com>

Provided the idea of real-time queries per second in the main display.

=item Paul DuBois <paul@snake.net>

Pointed out some option-handling bugs.

=item Mike Wexler <mwexler@tias.com>

Suggested that we don't mangle (normalize) whitespace in query info by
default.

=item Mark Zweifel <markez@yahoo-inc.com>

Make the --idle command-line argument negatable.

=item Axel Schwenke <schwenke@jobpilot.de>

Noticed the inccorect formula for query cache hit percentages in
version 1.2.

=item Steven Roussey <sroussey@network54.com>

Supplied a patch to help filter binary junk in queries so that
terminals don't freak out.

=item jon r. luini <falcon@chime.com>

Supplied a patch that formed the basis for C<-prompt> support.  Sean
Leach <sleach@wiggum.com> submitted a similar patch.

=item Yogish Baliga <baliga@yahoo-inc.com>

Supplied a patch that formed the basis for C<-resolve> support.

=item Per Andreas Buer <perbu@linpro.no>

Supplied an excellent patch to tidy up the top display.  This includes
showing most values in short form, such as 10k rather than 10000.

=item Michael "Monty" Widenius <monty@askmonty.org>

Fixed a couple of minor bugs that gave warnings on startup.
Added support for MariaDB (show MariaDB at top and % done).
Cut long server version names to display width.
Made 'State' length dynamic.

=back

See the Changes file on the B<mytop> distribution page for more
details on what has changed.

=head1 LICENSE

B<mytop> is licensed under the GNU General Public License version
2. For the full license information, please visit
http://www.gnu.org/copyleft/gpl.html

=cut

__END__
