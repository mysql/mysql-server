#! /usr/bin/perl

use strict;
use Getopt::Long;
use Symbol;
use Socket;

my $progname = $0;
$progname =~ s!^.*/|\.pl$!!g;
my $defaultport = 27127;
my $defaulttotal = 120;
my $defaultsample = 5;
my $defaultrange = 5000;

sub printhelp {
    print <<END;
$progname -- plot ndb operation counts in X11 window
usage: $progname [options]
--help		print this summary and exit
--debug		print lots of debug information
--port N	port number to listen on, default $defaultport
--total N	total time interval shown, default $defaulttotal seconds
--sample N	sample interval, default $defaultsample seconds
--range N	range (max ops per second), default $defaultrange
--nopct		show no percentages in graph titles
--z "..."	add X11/gnuplot options, for example:
		--z "-bg grey80 -geometry 500x300" --z "-persist"
END
    exit(0);
}

# get options
use vars qw(
    $helpflag $debug $serverport $totaltime $sampletime $range $nopct
    @zopts
);
$helpflag = 0;
$debug = 0;
$serverport = $defaultport;
$totaltime = $defaulttotal;
$sampletime = $defaultsample;
$range = $defaultrange;
$nopct = 0;
@zopts = ();
GetOptions(
    'help' => \$helpflag,
    'debug' => \$debug,
    'port=i' => \$serverport,
    'total=i' => \$totaltime,
    'sample=i' => \$sampletime,
    'range=i' => \$range,
    'nopct' => \$nopct,
    'z=s' => \@zopts,
) or die "try: $progname -h\n";
$helpflag && printhelp();

# calculate number of data points
my $samplecnt;
$samplecnt = int($totaltime / $sampletime) + 1;
$totaltime = ($samplecnt - 1) * $sampletime;
warn "total time = $totaltime sec, sample time = $sampletime sec\n";

# open gnuplot
my $plotfile;
sub openplot {
    $plotfile = gensym();
    if (! open($plotfile, "| gnuplot @zopts")) {
	die "open plot: $!\n";
    }
    my $sav = select($plotfile);
    $| = 1;
    select($sav);
    print $plotfile "clear\n";
}

# samples
my @sample;		# samples 0..$samplecnt in time order
my $sampleready = 0;	# samples 1..$samplecnt are ready (true/false)

@sample = map({ start => 0 }, 0..$samplecnt);

sub adddata {
    my($node, $type, $value) = @_;
    my $now = time;
    my $s = $sample[0];
    if ($now - $s->{start} >= $sampletime) {
	unshift(@sample, {
	    start => $now,
	    total => 0,
	});
	$s = $sample[0];
	pop(@sample);		# delete oldest
	$sampleready = 1;
    }
    # if no type then this is just a time tick
    if ($type) {
	$s->{$type} += $value;
	$s->{total} += $value;
    }
}

# data file name
my $datadir;
if ($ENV{NDB_BASE}) {
    $datadir = "$ENV{NDB_BASE}/var/plot";
} else {
    $datadir = "/var/tmp";
}
(-d $datadir || mkdir($datadir, 0777))
	or die "mkdir $datadir failed: $!\n";
my $datafile = "$datadir/plot$$.dat";
warn "writing plot data to $datafile\n";

# refresh the plot
sub plotsample {
    my $fh = gensym();
    if (! open($fh, ">$datafile")) {
	die "$datafile: $!\n";
    }
    # sample 0 is never ready
    my $currops = "";
    my $currpct = {};
    for (my $i = @sample; $i >= 1; $i--) {
	my $s = $sample[$i];
	if (! $s->{start}) {	# initial empty sample
	    next;
	}
	printf $fh "%d", -($i - 1) * $sampletime;
	printf $fh " %.0f", 1.01 * $s->{"total"} / $sampletime;
	for my $k (qw(insert update select delete)) {
	    printf $fh " %.0f", $s->{$k} / $sampletime;
	}
	printf $fh "\n";
	if ($i == 1) {
	    $currops = sprintf("%.0f", $s->{"total"} / $sampletime);
	    if (! $nopct && $currops > 0) {
		$currpct->{"total"} = sprintf("%5s", ""); 
		for my $k (qw(insert update select delete)) {
		    $currpct->{$k} = sprintf(" %3.0f%%",
			100.0 * $s->{$k} / $s->{"total"});
		}
	    }
	}
    }
    close($fh);
    print $plotfile <<END;
clear
set title "ops/sec [ $currops ]"
set xrange [@{[ -($totaltime-1) ]}:0]
set yrange [0:$range]
plot \\
    '$datafile' \\
	using 1:3 \\
	title "insert$currpct->{insert}" \\
	with lines lt 2, \\
    '$datafile' \\
	using 1:4 \\
	title "update$currpct->{update}" \\
	with lines lt 3, \\
    '$datafile' \\
	using 1:5 \\
	title "select$currpct->{select}" \\
	with lines lt 4, \\
    '$datafile' \\
	using 1:6 \\
	title "delete$currpct->{delete}" \\
	with lines lt 5, \\
    '$datafile' \\
	using 1:2 \\
	title "total$currpct->{total}" \\
	with lines lt 1 lw 2
END
}

# set up server socket
my $sock = gensym();
if (! socket($sock, PF_INET, SOCK_STREAM, getprotobyname("tcp"))) {
    die "socket: $!\n";
}
if (! setsockopt($sock, SOL_SOCKET, SO_REUSEADDR, pack("l*", 1))) {
    die "setsockopt: $!\n";
}
if (! bind($sock, pack_sockaddr_in($serverport, INADDR_ANY))) {
    die "bind: $!\n";
}
if (! listen($sock, SOMAXCONN)) {
    die "listen: $!\n";
}

# bit vectors for select on server socket and clients
my $readin = '';
vec($readin, fileno($sock), 1) = 1;

# clients
my @client = ();
my $clientid = 0;
sub addclient {
    my($conn) = @_;
    my $c = {
	conn => $conn,
	data => "",
	name => "client " . ++$clientid,
    };
    push(@client, $c);
    vec($readin, fileno($c->{conn}), 1) = 1;
    if (1 || $debug) {
	warn "added $c->{name}\n";
    }
}
sub deleteclient {
    my($c) = @_;
    @client = grep($_ ne $c, @client);
    vec($readin, fileno($c->{conn}), 1) = 0;
    shutdown($c->{conn}, 2);
    if (1 || $debug) {
	warn "deleted $c->{name}\n";
    }
}
sub readclient {
    my($c) = @_;
    my $data;
    my $n;
    eval {
	local $SIG{ALRM} = sub { die "timeout\n" };
	alarm(5);
	$n = sysread($c->{conn}, $data, 512);
	alarm(0);
    };
    if ($@) {
	chomp($@);
	warn "$c->{name}: read: $@\n";
	return undef;
    }
    if (!defined($n)) {
	warn "$c->{name}: read: $!\n";
	return undef;
    }
    $c->{data} .= $data;
    if ($debug) {
	warn "$c->{name}: read @{[ length($data) ]} bytes\n";
    }
    return $n;
}
sub processclient {
    my($c) = @_;
    my $i;
    while (($i = index($c->{data}, "\n")) >= 0) {
	my $line = substr($c->{data}, 0, $i);
	$c->{data} = substr($c->{data}, $i+1);
	my($node, $type, $value) = split(' ', $line);
	if ($node !~ /^\d+$/) {
		warn "$c->{name}: $line: bad node id\n";
		next;
	}
	if ($type !~ /^(insert|update|read|delete|verify|verifydelete)$/) {
		warn "$c->{name}: $line: bad type\n";
		next;
	}
	if ($value !~ /^\d+$/) {
		warn "$c->{name}: $line: bad value\n";
		next;
	}
	if ($type eq "read") {
		$type = "select";
	}
	adddata($node, $type, $value);
    }
}

# main loop
openplot();
while (1) {
    my $readout = '';
    my $ret = select($readout = $readin, undef, undef, 1.0);
    if (vec($readout, fileno($sock), 1)) {
	my $conn = gensym();
	if (! accept($conn, $sock)) {
	    warn "accept failed: $!\n";
	} else {
	    addclient($conn);
	}
    }
    for my $c (@client) {
	if (vec($readout, fileno($c->{conn}), 1)) {
	    my $n = readclient($c);
	    if (! defined($n)) {
		deleteclient($c);
	    } else {
		processclient($c);
		if ($n == 0) {		# end of file
		    deleteclient($c);
		}
	    }
	}
    }
    adddata();				# keep clock ticking
    if ($sampleready) {
	if ($debug) {
	    warn "sample ready\n";
	}
	plotsample();
	$sampleready = 0;
    }
}
# vim: set sw=4:
