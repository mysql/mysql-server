#! /usr/local/bin/perl

use strict;
use POSIX();
use Socket;
use Getopt::Long;
use File::Basename;
use Term::ReadLine;

use NDB::Net;

select(STDOUT);
$| = 1;

# get options and environment

my $log = NDB::Util::Log->instance;
$log->setpart();

sub printhelp {
    print <<END;
ndbnet -- ndbnet client
usage: ndbnet [options] [command...]
--help		print this text and exit
--base dir	ndb installation, default \$NDB_BASE
--netcfg file	net config, default \$NDB_BASE/etc/ndbnet.xml
--server id	ndbnetd server id, or host:port if no config
--noterm	no prompting and no input editing
--log prio	debug/info/notice/warn/error/fatal, default info
command...	command (by default becomes interactive)
END
	exit(0);
}

my $progopts = {};
my @progargv;

anon: {
    local $SIG{__WARN__} = sub {
	my $errstr = "@_";
	while (chomp($errstr)) {}
	$log->put("$errstr (try --help)")->fatal;
    };
    Getopt::Long::Configure(qw(
	default no_getopt_compat no_ignore_case require_order
    ));
    GetOptions($progopts, qw(
	help base=s netcfg=s server=s noterm log=s
    ));
}

$progopts->{help} && printhelp();
if (defined(my $prio = $progopts->{log})) {
    $log->setprio($prio);
}
@progargv = @ARGV;

my $netenv = NDB::Net::Env->instance(
    base => $progopts->{base},
    netcfg => $progopts->{netcfg},
);
$netenv or $log->fatal;

# get servers from command line or from net config

my @servers = ();
my $netcfg;
if ($netenv->hasnetcfg) {
    $netcfg = NDB::Net::Config->new(file => $netenv->getnetcfg);
}

if (defined(my $id = $progopts->{server})) {
    if ($id !~ /:/) {
	$netcfg or $log->put("need net config to find server $id")->fatal;
	$netcfg->load or $log->push->fatal;
	$netcfg->getservers or $log->push->fatal;
	my $s = NDB::Net::Server->get($id) or $log->fatal;
	push(@servers, $s);
    } else {
	my($host, $port) = split(/:/, $id, 2);
	my $s = NDB::Net::ServerINET->new(id => "?", host => $host, port => $port)
	    or $log->fatal;
	push(@servers, $s);
    }
} else {
    $netcfg or $log->put("need net config to find servers")->fatal;
    $netcfg->load or $log->push->fatal;
    my $list = $netcfg->getservers or $log->fatal;
    @servers=  @$list;
    @servers or $log->put("no servers")->push($netcfg)->fatal;
}

# server commands

my $server;
sub doserver {
    my($cmd) = @_;
    my $ret;
    my $found;
    for my $s (@servers) {
	if (! $s->testconnect) {
	    $log->warn;
	    next;
	}
	$found = 1;
	if ($server ne $s) {
	    $server = $s;
	    $log->put("selected")->push($server)->debug;
	}
	$ret = $server->request($cmd);
	last;
    }
    if (! $found) {
	$log->put("no available server");
	return undef;
    }
    my %seen = ();
    @servers = grep(! $seen{$_}++, $server, @servers);
    defined($ret) or $log->push, return undef;
    return $ret;
}

# local commands

sub cmd_help {
    my($cmd) = @_;
    my $text = $cmd->helptext;
    defined($text) or return undef;
    while(chomp($text)) {}
    print $text, "\n";
    return 1;
}

sub cmd_alias {
    my($cmd) = @_;
    my $text = $cmd->aliastext;
    while(chomp($text)) {}
    print $text, "\n";
}

sub cmd_quit {
    my($cmd) = @_;
    $log->put("bye-bye")->info;
    exit(0);
}

sub cmd_server {
    my($cmd) = @_;
    my $action = $cmd->getarg(0);
    if ($action !~ /^(start|restart|stop|ping)$/) {
	$log->put("$action: undefined action");
	return undef;
    }
    if ($action eq 'start') {
	$cmd->setopt('direct')
	    or $log->push, return undef;
    }
    if ($action eq 'ping' && ! @{$cmd->getarglist(1)}) {
	$cmd->setopt('all')
	    or $log->push, return undef;
    }
    if (! $cmd->getopt('direct')) {
	return doserver($cmd);
    }
    $netcfg->load
	or return undef;
    my $servers = $netcfg->getservers
	or return undef;
    my $list;
    if ($cmd->getopt('all')) {
	$list = $servers;
    }
    else {
	$list = [];
	for my $id (@{$cmd->getarglist(1)}) {
	    if (my $s = NDB::Net::ServerINET->get($id)) {
		push(@$list, $s);
		next;
	    }
	    if (my $s = NDB::Net::ServerINET->match($id, undef, $servers)) {
		if (@$s) {
		    push(@$list, @$s);
		    next;
		}
	    }
	    $log->push;
	    return undef;
	}
    }
    if (! @$list) {
	$log->put("no servers specified, use --all for all")->info;
	return 1;
    }
    for my $s (@$list) {
	if ($action eq 'ping') {
	    if ($s->testconnect) {
		$log->put("is alive")->push($s);
	    }
	    $log->info;
	    next;
	}
	if ($action eq 'start') {
	    if ($s->testconnect) {
		$log->put("already running")->push($s)->info;
		next;
	    }
	}
	my $script = $cmd->getopt('script') || "ndbnetd";
	my @cmd = ($script);
	if ($action eq 'restart') {
	    push(@cmd, "--restart");
	}
	if ($action eq 'stop') {
	    push(@cmd, "--stop");
	}
	if ($cmd->getopt('pass')) {
	    my $base = $netenv->getbase;
	    $cmd[0] = "$base/bin/$cmd[0]";
	}
	if ($cmd->getopt('parallel')) {
	    my $pid = fork;
	    defined($pid) or
		$log->push("fork failed: $!"), return undef;
	    $pid > 0 && next;
	}
	$log->put("$action via ssh")->push($s->getcanon)->push($s)->info;
	$log->put("run: @cmd")->push($s)->debug;
	system 'ssh', '-n', $s->getcanon, "@cmd";
	if ($cmd->getopt('parallel')) {
	    exit(0);
	}
    }
    if ($cmd->getopt('parallel')) {
	while ((my $pid = waitpid(-1, &POSIX::WNOHANG)) > 0) {
	    ;
	}
    }
    return 1;
}

sub cmd_list {
    my($cmd) = @_;
    my $ret = doserver($cmd) or
	$log->push, return undef;
    my @out = ();
    my @o = qw(NAME NODES PROCESS STATUS COMMENT);
    push(@out, [ @o ]);
    for my $name (sort keys %$ret) {
	$#o = -1;
	$o[0] = $name;
	my $dbsts = $ret->{$name};
	my @tmp = sort { $a->{id} <=> $b->{id} } values %{$dbsts->{node}};
	my @nodesmgmt = grep($_->{type} eq 'mgmt', @tmp);
	my @nodesdb = grep($_->{type} eq 'db', @tmp);
	my @nodesapi = grep($_->{type} eq 'api', @tmp);
	my @nodes = (@nodesmgmt, @nodesdb, @nodesapi);
	$o[1] = sprintf("%d/%d/%d", 0+@nodesmgmt, 0+@nodesdb, 0+@nodesapi);
	$o[2] = "-";
	$o[3] = "-";
	$o[4] = $dbsts->{comment};
	$o[4] .= " - " if length $o[4];
	$o[4] .= basename($dbsts->{home});
	push(@out, [ @o ]);
	for my $nodests (@nodes) {
	    $#o = -1;
	    $o[0] = $nodests->{id} . "-" . $nodests->{type};
	    $o[1] = $nodests->{host};
	    $o[1] =~ s/\..*//;
	    $o[2] = $nodests->{run};
	    $o[3] = $nodests->{status} || "-";
	    $o[4] = $nodests->{comment} || "-";
	    push(@out, [ @o ]);
	}
    }
    my @len = ( 8, 8, 8, 8 );
    for my $o (@out) {
	for my $i (0..$#len) {
	    $len[$i] = length($o->[$i]) if $len[$i] < length($o->[$i]);
	}
    }
    for my $o (@out) {
	my @t = ();
	for my $i (0..$#{$out[0]}) {
	    my $f = $len[$i] ? "%-$len[$i].$len[$i]s" : "%s";
	    push(@t, sprintf($f, $o->[$i]));
	}
	print "@t\n";
    }
    return 1;
}

# main program

sub docmd {
    my(@args) = @_;
    my $cmd = NDB::Net::Command->new(@args)
	or return undef;
    my $name = $cmd->getname;
    my $doit;
    {
	no strict 'refs';
	$doit = *{"cmd_$name"};
    }
    if (! defined(&$doit)) {
	$doit = \&doserver;
    }
    my $ret = &$doit($cmd);
    defined($ret) or $log->push, return undef;
    return $ret;
}

if (@progargv) {
    docmd(argv => \@progargv) or $log->push->fatal;
    exit(0);
}

my $term;
if ((-t STDIN) && (-t STDOUT) && ! $progopts->{noterm}) {
    $term = Term::ReadLine->new("ndbnet");
    $term->ornaments(0);
}

print "type 'h' for help\n" if $term;
while (1) {
    my($line);
    while (! $line) {
	$line = $term ? $term->readline("> ") : <STDIN>;
	if (! defined($line)) {
	    print("\n") if $term;
	    $line = 'EOF';
	}
    }
    my $ret = docmd(line => $line);
    $ret or $log->error;
    ($line eq 'EOF') && last;
}

1;
# vim:set sw=4:
