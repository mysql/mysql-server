#! /usr/local/bin/perl

use strict;
use POSIX();
use Socket;
use Getopt::Long;
use File::Basename;
use File::Spec;

use NDB::Net;

# save argv for restart via client
my @origargv = @ARGV;

# get options and environment

my $log = NDB::Util::Log->instance;

sub printhelp {
    print <<END;
ndbnetd -- ndbnet daemon
usage: ndbnetd [options]
--help		print this text and exit
--base dir	ndb installation, default \$NDB_BASE
--netcfg file	net config, default \$NDB_BASE/etc/ndbnet.xml
--port num	port number (if more than 1 server on this host)
--stop		kill any existing server
--restart	kill any existing server and start a new one
--fg		run in foreground (test option)
--log prio	debug/info/notice/warn/error/fatal, default info
END
	exit(0);
}

my $progopts = {};
anon: {
    local $SIG{__WARN__} = sub {
	my $errstr = "@_";
	while (chomp($errstr)) {}
	$log->put("$errstr (try --help)")->fatal;
    };
   Getopt::Long::Configure(qw(
	default no_getopt_compat no_ignore_case no_require_order
   ));
    GetOptions($progopts, qw(
	help base=s netcfg=s port=i stop restart fg log=s
    ));
}
$progopts->{help} && printhelp();
if (defined(my $prio = $progopts->{log})) {
    $log->setprio($prio);
}
@ARGV and $log->put("extra args on command line")->fatal;

my $netenv = NDB::Net::Env->instance(
    base => $progopts->{base},
    netcfg => $progopts->{netcfg},
);
$netenv or $log->fatal;
$netenv->hasbase or $log->put("need NDB_BASE")->fatal;

# load net config and find our entry

my $netcfg = NDB::Net::Config->new(file => $netenv->getnetcfg)
    or $log->push->fatal;
my $server;

sub loadnetcfg {
    $netcfg->load or $log->push->fatal;
    my $servers = $netcfg->getservers or $log->fatal;
    my $host = $netenv->gethostname;
    my $port = $progopts->{port} || 0;
    my $list = NDB::Net::ServerINET->match($host, $port, $servers)
	or $log->push->fatal;
    @$list == 1
	or $log->push->fatal;
    $server = $list->[0];
    $server->setlocal;
}
loadnetcfg();
$log->put("this server")->push($server)->debug;

# check if server already running

my $lock;
anon: {
    my $dir = NDB::Util::Dir->new(path => File::Spec->catfile($netenv->getbase, "run"));
    $dir->mkdir or $log->fatal;
    my $name = sprintf("ndbnet%s.pid", $server->getid);
    $lock = $dir->getfile($name)->getlock;
    my $ret;
    $ret = $lock->test;
    defined($ret) or $log->fatal;
    if ($ret) {
	if ($progopts->{stop} || $progopts->{restart}) {
	    $log->put("stopping server %s pid=%s", $netenv->gethostname, $lock->getpid)->info;
	    if ($^O ne 'MSWin32') {
		kill -15, $lock->getpid;
	    } else {
		kill 15, $lock->getpid;
	    }
	    while (1) {
		sleep 1;
		$ret = $lock->test;
		defined($ret) or $log->fatal;
		if ($ret) {
		    if (! kill(0, $lock->getpid) && $! == Errno::ESRCH) {
			$log->put("locked but gone (linux bug?)")->info;
			$lock->unlink;
			$ret = 0;
		    }
		}
		if (! $ret) {
		    if ($progopts->{stop}) {
			$log->put("stopped")->info;
			exit(0);
		    }
		    $log->put("restarting server %s", $netenv->gethostname)->info;
		    last;
		}
	    }
	}
	else {
	    $log->put("already running pid=%s", $lock->getpid)->fatal;
	}
    }
    else {
	if ($progopts->{stop}) {
	    $log->put("not running")->info;
	    exit(0);
	}
    }
    $lock->set or $log->fatal;
}

# become daemon, re-obtain the lock, direct log to file

anon: {
    $log->setpart(time => 1, pid => 1, prio => 1, line => 1);
    $progopts->{fg} && last anon;
    $lock->close;
    my $dir = NDB::Util::Dir->new(path => $netenv->getbase . "/log");
    $dir->mkdir or $log->fatal;
    my $pid = fork();
    defined($pid) or $log->put("fork failed: $!")->fatal;
    if ($pid) {
	exit(0);
    }
    $lock->set or $log->fatal;
    if ($^O ne 'MSWin32') {
	POSIX::setsid() or $log->put("setsid failed: $!")->fatal;
    }
    open(STDIN, "</dev/null");
    my $name = sprintf("ndbnet%s.log", $server->getid);
    $log->setfile($dir->getfile($name)->getpath) or $log->fatal;
}
$log->put("ndbnetd started pid=$$ port=%s", $server->getport)->info;

# create server socket and event

my $socket = NDB::Util::SocketINET->new or $log->fatal;
my $event = NDB::Util::Event->new;

# commands

sub cmd_server_fg {
    my($cmd) = @_;
    my $action = $cmd->getarg(0);
    if (! $cmd->getopt('local')) {
	return 1;
    }
    if ($action eq 'restart') {
	my $prog = $netenv->getbase . "/bin/ndbnetd";
	my @argv = @origargv;
	if (! grep(/^--restart$/, @argv)) {
	    push(@argv, "--restart");
	}
	unshift(@argv, basename($prog));
	$lock->close;
	$socket->close;
	$log->put("restart: @argv")->push($server)->user;
	$log->put("server restart")->putvalue(1)->user;
	exec $prog @argv;
	die "restart failed: $!";
    }
    if ($action eq 'stop') {
	$log->put("stop by request")->push($server)->user;
	$log->put("server stop")->putvalue(1)->user;
	exit(0);
    }
    if ($action eq 'ping') {
	return 1;
    }
    $log->put("$action: unimplemented");
    return undef;
}

sub cmd_server_bg {
    my($cmd) = @_;
    loadnetcfg() or return undef;
    my $action = $cmd->getarg(0);
    if (! $cmd->getopt('local')) {
	$cmd->setopt('local')
	    or $log->push, return undef;
	my $servers = $netcfg->getservers or $log->fatal;
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
	my $fail = 0;
	for my $s (@$list) {
	    if (! $s->request($cmd)) {
		$log->push->user;
		$fail++;
	    }
	}
	if ($fail) {
	    $log->put("failed %d/%d", $fail, scalar(@$list));
	    return undef;
	}
	return 1;
    }
    if ($action eq 'restart') {
	return 1;
    }
    if ($action eq 'stop') {
	return 1;
    }
    if ($action eq 'ping') {
	$log->put("is alive")->push($server)->user;
	return 1;
    }
    $log->put("$action: unimplemented");
    return undef;
}

sub cmd_start_bg {
    my($cmd) = @_;
    loadnetcfg() or return undef;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    $db->start($cmd->getopts) or return undef;
    return 1;
}

sub cmd_startnode_bg {
    my($cmd) = @_;
    loadnetcfg() or return undef;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    my $node = $db->getnode($cmd->getarg(1)) or return undef;
    $node->start($cmd->getopts) or return undef;
    return 1;
}

sub cmd_stop_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    $db->stop($cmd->getopts) or return undef;
    return 1;
}

sub cmd_stopnode_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    my $node = $db->getnode($cmd->getarg(1)) or return undef;
    $node->stop($cmd->getopts) or return undef;
    return 1;
}

sub cmd_kill_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    $db->kill($cmd->getopts) or return undef;
    return 1;
}

sub cmd_killnode_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    my $node = $db->getnode($cmd->getarg(1)) or return undef;
    $node->kill($cmd->getopts) or return undef;
    return 1;
}

sub cmd_statnode_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    my $node = $db->getnode($cmd->getarg(1)) or return undef;
    my $ret = $node->stat($cmd->getopts) or return undef;
    return $ret;
}

sub cmd_list_bg {
    my($cmd) = @_;
    loadnetcfg() or return undef;
    my $dblist;
    if ($cmd->getarg(0)) {
	my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
	$dblist = [ $db ];
    } else {
	$dblist = $netcfg->getdatabases or return undef;
    }
    my $ret = {};
    for my $db (@$dblist) {
	my $status = $db->list($cmd->getopts) || "error";
	$ret->{$db->getname} = $status;
    }
    return $ret;
}

sub cmd_writenode_bg {
    my($cmd) = @_;
    my $db = $netcfg->getdatabase($cmd->getarg(0)) or return undef;
    my $node = $db->getnode($cmd->getarg(1)) or return undef;
    my $ret = $node->write($cmd->getopts, $cmd->getarg(2)) or return undef;
    return $ret;
}

# main program

sub checkchild {
    while ((my $pid = waitpid(-1, &POSIX::WNOHANG)) > 0) {
	$log->put("harvested pid=$pid")->info;
    }
}

my $gotterm = 0;
$SIG{INT} = sub { $gotterm = 1 };
$SIG{TERM} = sub { $gotterm = 1 };

$socket->setopt(SOL_SOCKET, SO_REUSEADDR, 1) or $log->fatal;
$socket->bind($server->getport) or $log->fatal;
$socket->listen or $log->fatal;
$event->set($socket, 'r');

loop: {
    try: {
	my $n = $event->poll(10);
	if ($gotterm) {
	    $log->put("terminate on signal")->info;
	    last try;
	}
	if (! defined($n)) {
	    $log->error;
	    sleep 1;
	    last try;
	}
	if (! $n) {
	    $log->debug;
	    last try;
	}
	if (! $event->test($socket, 'r')) {
	    last try;
	}
	my $csocket = $socket->accept(10);
	if (! defined($csocket)) {
	    $log->error;
	    last try;
	}
	if (! $csocket) {
	    $log->warn;
	    last try;
	}
	my $client = NDB::Net::Client->new(
	    socket => $csocket,
	    serversocket => $socket,
	    serverlock => $lock,
	    event => $event,
	    context => 'main',
	);
	$client or $log->fatal;
    }
    loadnetcfg() or $log->fatal;
    NDB::Net::Client->processall;
    if ($gotterm) {
	last loop;
    }
    redo loop;
}

$log->put("ndbnetd done")->info;

1;
# vim:set sw=4:
