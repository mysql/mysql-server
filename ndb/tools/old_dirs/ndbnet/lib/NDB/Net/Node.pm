package NDB::Net::Node;

use strict;
use Carp;
use Symbol;
use Socket;
use IPC::Open3;
use POSIX();
use Errno;
use File::Spec;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

my %nodecache = ();

NDB::Net::Node->attributes(
    db => sub { ref && $_->isa('NDB::Net::Database') },
    comment => sub { defined },
    id => sub { s/^\s+|\s+$//g; s/^0+(\d+)$/$1/; /^\d+$/ && $_ > 0 },
    type => sub { s/^\s+|\s+$//g; /^(mgmt|db|api)$/ },
    server => sub { ref && $_->isa('NDB::Net::Server') },
    base => sub { File::Spec->file_name_is_absolute($_) },
    home => sub { File::Spec->file_name_is_absolute($_) },
    state => sub { /^(new|run|stop)$/ },
    run => sub { defined },
    runenv => sub { defined },
    runtype => sub { m!(auto|once|manual)$! },
    lockpid => sub { $_ != 0 },
    iow => sub { ref && $_->isa('NDB::Util::IO') },
    ior => sub { ref && $_->isa('NDB::Util::IO') },
    pid => sub { $_ > 1 },
    event => sub { ref && $_->isa('NDB::Util::Event') },
);

sub desc {
    my $node = shift;
    my $dbname = $node->getdb->getname;
    my $id = $node->getid;
    my $type = $node->gettype;
    return "$dbname.$id-$type";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $node = $class->SUPER::new(%attr);
    $node->setdb($attr{db})
	or $log->push, return undef;
    $node->setid($attr{id})
    	or $log->push, return undef;
    if ($nodecache{$node->getdb->getname,$node->getid}) {
	$log->put("duplicate node")->push($node);
	return undef;
    }
    $node->setcomment($attr{comment});
    $node->settype($attr{type})
	or $log->push, return undef;
    if ($node->getdb->cmpversion("1.0") <= 0 && $node->gettype eq 'mgmt') {
	$log->put("no mgmt nodes in db version <= 1.0")->push($node);
	return undef;
    }
    $node->setserver($attr{server})
	or $log->push, return undef;
    for my $base ($attr{base}, $node->getdb->getbase(undef)) {
	if (defined($base)) {
	    $node->setbase($base)
		or $log->push, return undef;
	}
    }
    for my $home ($attr{home}, $node->getdb->gethome(undef)) {
	if (defined($home)) {
	    if ($^O ne 'MSWin32' && $home !~ m!^/! && $node->hasbase) {
		$home = $node->getbase . "/$home";
	    }
	    $node->sethome($home)
		or $log->push, return undef;
	}
    }
    if (! $node->hashome) {
	$log->put("home not defined")->push($node);
	return undef;
    }
    $node->setstate('new')
	or $log->push, return undef;
    if (defined($attr{run})) {
	$node->setrun($attr{run})
	    or $log->push, return undef;
    }
    if (defined($attr{runenv})) {
	$node->setrunenv($attr{runenv})
	    or $log->push, return undef;
    }
    if (defined($attr{runtype})) {
	$node->setruntype($attr{runtype})
	    or $log->push, return undef;
    }
    if (! $node->hasruntype) {
	my $runtype = "manual";
	$runtype = "once"
	    if $node->gettype =~ /^(mgmt|db)$/ || $node->hasrun;
	$node->setruntype($runtype)
	    or $log->push, return undef;
    }
    if (! $node->getdb->addnode($node)) {
	$log->push;
	return undef;
    }
    $nodecache{$node->getdb->getname,$node->getid} = $node;
    return $node;
}

sub delete {
    my $node = shift;
    delete $nodecache{$node->getdb->getname,$node->getid} or
	confess 'oops';
}

sub deleteall {
    my $class = shift;
    for my $k (sort keys %nodecache) {
	my $node = $nodecache{$k};
	$node->delete;
    }
}

# node startup

sub getconfdir {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $netenv = NDB::Net::Env->instance;
    my $name = File::Spec->catfile($netenv->getbase, "etc");
    my $dir = NDB::Util::Dir->new(path => $name);
    return $dir;
}

sub getdbdir {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $netenv = NDB::Net::Env->instance;
    my $name = File::Spec->catfile($netenv->getbase, "db", $node->getdb->getname);
    my $dir = NDB::Util::Dir->new(path => $name);
    return $dir;
}

sub getnodedir {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("%s-%s", $node->getid, $node->gettype);
    my $dir = $node->getdbdir->getdir($name);
    return $dir;
}

sub getrundir {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("run");
    my $dir = $node->getdbdir->getdir($name);
    return $dir;
}

sub getlogdir {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("log");
    my $dir = $node->getdbdir->getdir($name);
    return $dir;
}

sub getlock {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("%s-%s.pid", $node->getid, $node->gettype);
    my $lock = $node->getrundir->getfile($name)->getlock;
    return $lock;
}

sub getsocketfile {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("%s-%s.socket", $node->getid, $node->gettype);
    my $file = $node->getrundir->getfile($name);
    return $file;
}

sub getlogfile {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("%s-%s.log", $node->getid, $node->gettype);
    my $file = $node->getlogdir->getfile($name);
    return $file;
}

sub getshellfile {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("run.sh");
    my $file = $node->getnodedir->getfile($name);
    return $file;
}

sub getlocalcfg {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = "Ndb.cfg";
    my $file = $node->getnodedir->getfile($name);
    return $file;
}

sub writelocalcfg {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $db = $node->getdb;
    my $file = $node->getlocalcfg;
    $file->mkdir or $log->push, return undef;
    if ($db->cmpversion("1.0") <= 0) {
	my $section = "";
	my $edit = sub {
	    chomp;
	    if (/^\s*\[\s*(\S+)\s*\]/) {
		$section = uc($1);
	    }
	    if ($section eq 'OWN_HOST') {
		if (/^\s*ThisHostId\b/i) {
		    $_ = "ThisHostId " . $node->getid;
		}
	    }
	    if ($section eq 'CM') {
		if (/^\s*ThisNodeId\b/i) {
		    $_ = "ThisNodeId " . $node->getid;
		}
	    }
	    if (0 and $section eq 'PROCESS_ID') {
		if (/^\s*Host(\d+)\s+(\S+)(.*)/) {
		    my $id2 = $1;
		    my $host2 = $2;
		    my $rest2 = $3;
		    my $node2 = $db->getnode($id2)
			or $log->push, return undef;
		    $_ = "Host$id2 ";
		    $_ .= $node2->getserver->getcanon;
		    $_ .= " $rest2";
		}
	    }
	    $_ .= "\n";
	    return 1;
	};
	$node->getinifile->copyedit($file, $edit)
	    or $log->push, return undef;
    }
    else {
	my @text = ();
	push(@text, sprintf("OwnProcessId %s", $node->getid));
	my $nodesmgmt = $db->getnodelist('mgmt');
	for my $mnode (@$nodesmgmt) {
	    my $host = $mnode->getserver->getcanon;
	    my $port = $mnode->getport;
	    push(@text, "$host $port");
	}
	$file->putlines(\@text) or $log->push, return undef;
    }
    return 1;
}

sub getinifile {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("%s.ini", $node->getdb->getname);
    my $file = $node->getconfdir->getfile($name);
    return $file;
}

sub getbincfg {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = sprintf("config.bin");
    my $file = $node->getnodedir->getfile($name);
    return $file;
}

sub getenvdefs {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my $opts = shift;
    my $home = $opts->{home} || $node->gethome;
    my $netenv = NDB::Net::Env->instance;
    if (! File::Spec->file_name_is_absolute($home)) {
	$netenv->hasbase
	    or $log->put("no base and home=$home not absolute"), return undef;
	$home = File::Spec->catfile($netenv->getbase, $home);
    }
    (-d $home)
	or $log->put("$home: no such directory"), return undef;
    my $defs;
    if ($^O ne 'MSWin32') {
	$defs = <<END;
# @{[ $node->desc ]} @{[ $node->getcomment("") ]}
# @{[ $node->getserver->desc ]} @{[ $node->getserver->getcanon ]}
#
debugger=\$1
#
NDB_TOP=$home
export NDB_TOP
PATH=\$NDB_TOP/bin:\$PATH
export PATH
LD_LIBRARY_PATH=\$NDB_TOP/lib:\$LD_LIBRARY_PATH
export LD_LIBRARY_PATH
PERL5LIB=\$NDB_TOP/lib/perl5:\$PERL5LIB
export PERL5LIB
NDB_NODEID=@{[ $node->getid ]}
export NDB_NODEID
NDB_NODETYPE=@{[ $node->gettype ]}
export NDB_NODETYPE
ulimit -Sc unlimited
END
	if ($node->hasrunenv) {
	    $defs .= <<END;
#
cd @{[ $node->getnodedir->getpath ]} || exit 1
@{[ $node->getrunenv ]}
END
	}
	$defs .= <<END;
#
unset NDB_HOME	# current NdbConfig.c would look here
#
END
    } else {
	$defs = <<END;
rem @{[ $node->desc ]} @{[ $node->getcomment("") ]}
rem @{[ $node->getserver->desc ]} @{[ $node->getserver->getcanon ]}
rem
set NDB_TOP=$home
set PATH=%NDB_TOP%\\bin;%PATH%
set PERL5LIB=%NDB_TOP%\\lib\\perl5;%PERL5LIB%
set NDB_NODEID=@{[ $node->getid ]}
set NDB_NODETYPE=@{[ $node->gettype ]}
END
	if ($node->hasrunenv) {
	    $defs .= <<END;
rem
@{[ $node->getrunenv ]}
END
	}
	$defs .= <<END;
rem
rem current NdbConfig.c would look here
set NDB_HOME=
rem
END
    }
    chomp($defs);
    return $defs;
}

sub startlocal {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("start local")->push($node)->info;
    my $lock = $node->getlock;
    $lock->mkdir or $log->push, return undef;
    anon: {
	my $ret = $lock->test;
	defined($ret) or $log->push, return undef;
	if ($ret) {
	    $log->put("already running under serverpid=%s",
		$lock->getpid)->push($node)->user;
	    return 1;
	}
	$lock->set or $log->push, return undef;
    }
    if ($opts->{clean}) {
	$node->getnodedir->rmdir(1);
	$node->getlogfile->unlink;
    }
    if (! $opts->{old}) {
	$node->writelocalcfg or $log->push, return undef;
	$node->handleprepare($opts) or $log->push, return undef;
    }
    anon: {
	$lock->close;
	if ($opts->{config}) {
	    return 1;
	}
	my $file = $node->getlogfile;
	$file->mkdir or $log->push, return undef;
	my $pid = fork();
	defined($pid) or $log->put("fork failed: $!"), return undef;
	if ($pid) {
	    exit(0);
	}
	$lock->set or $log->push->fatal;
	$node->setlockpid($$) or $log->push->fatal;
	if ($^O ne 'MSWin32') {
	    POSIX::setsid() or $log->put("setsid failed: $!")->fatal;
	}
	$log->setfile($file->getpath) or $log->push->fatal;
    }
    my $socket;
    anon: {
	my $file = $node->getsocketfile;
	$file->mkdir or $log->push($node)->fatal;
	unlink($file->getpath);
	if ($^O ne 'MSWin32') {
	    $socket = NDB::Util::SocketUNIX->new
		or $log->push($node)->fatal;
	} else {
	    $socket = NDB::Util::SocketINET->new
		or $log->push($node)->fatal;
	}
	$socket->setopt(SOL_SOCKET, SO_REUSEADDR, 1)
	    or $log->push($node)->fatal;
	if ($^O ne 'MSWin32') {
	    $socket->bind($file->getpath)
		or $log->push($node)->fatal;
	} else {
	    $socket->bind($node->getdb->getnodeport + $node->getid)
		or $log->push($node)->fatal;
	}
	$socket->listen
	    or $log->push($node)->fatal;
    }
    START: {
	my $w = gensym();
	my $r = gensym();
	my @arg = ('/bin/sh', $node->getshellfile->getpath);
	my $pid = open3($w, $r, undef, @arg);
	$node->setiow(NDB::Util::IO->new(fh => $w))
	    or $log->push->fatal;
	$node->setior(NDB::Util::IO->new(fh => $r))
	    or $log->push->fatal;
	$node->setpid($pid)
	    or $log->push->fatal;
    }
    $node->setstate('run')
	or $log->push($node)->fatal;
    $log->put("started host=%s pid=%s",
	$node->getserver->gethost, $node->getpid)->push($node)->user;
    $log->push("started")->push($node)->putvalue(1)->user;
    $log->detachuser;
    NDB::Net::Client->deleteall;
    my $event = NDB::Util::Event->new;
    $event->set($socket, 'r');
    $event->set($node->getior, 'r');
    loop: {
	try: {
	    my $n = $event->poll(10);
	    if (! defined($n)) {
		$log->push->error;
		sleep 1;
		last try;
	    }
	    if (! $n) {
		$log->push->debug;
		last try;
	    }
	    if ($node->hasior && $event->test($node->getior, 'r')) {
		my $data = $node->getior->read;
		if (! defined($data)) {
		    $log->push->fatal;
		}
		if (length($data) > 0) {
		    $node->handleoutput($opts, $data);
		}
		if ($node->getior->getreadend) {
		    $log->put("input closed")->warn;
		    $event->clear($node->getior, 'r');
		    $node->getior->close;
		    $node->delior;
		    $node->handleeof($opts);
		    last loop;
		}
	    }
	    if (! $event->test($socket, 'r')) {
		last try;
	    }
	    my $csocket = $socket->accept(10);
	    if (! defined($csocket)) {
		$log->push->error;
		last try;
	    }
	    if (! $csocket) {
		$log->push->warn;
		last try;
	    }
	    my $client = NDB::Net::Client->new(
		socket => $csocket,
		serversocket => $socket,
		serverlock => $lock,
		event => $event,
		context => $node,
	    );
	    $client or $log->push->fatal;
	}
	NDB::Net::Client->processall;
	redo loop;
    }
    if ($node->getruntype eq "auto") {
	if ($node->getstate eq "run") {
	    $log->put("restart in 5 seconds...")->info;
	    sleep 5;
	    goto START;
	}
	$log->put("stopping, skip restart")->info;
    }
    $lock->close;
    $node->getsocketfile->unlink;
    while (wait() != -1) {}
    $log->put("exit")->push->info;
    exit(0);
}

# handlers can be overridden in subclass

sub handleprepare { confess 'oops'; }

sub handleoutput {
    my $node = shift;
    @_ == 2 or confess 0+@_;
    my($opts, $data) = @_;
    $data =~ s/\015//g;
    $data = $node->{savedata} . $data;
    while ((my $i = index($data, "\n")) >= 0) {
	my $line = substr($data, 0, $i);
	$data = substr($data, $i+1);
	$log->put($line)->info;
	if ($opts->{user} && $line !~ /^\s*$/) {
	    $log->put($line)->user;
	}
    }
    $node->{savedata} = $data;
    if (1 && length $node->{savedata}) {	# XXX partial line
	my $line = $node->{savedata};
	$log->put($line)->info;
	if ($opts->{user} && $line !~ /^\s*$/) {
	    $log->put($line)->user;
	}
	$node->{savedata} = "";
    }
}

sub handleeof {
}

# command subs can be overridden by subclass

sub waitforexit {
    my $node = shift;
    my $lock = $node->getlock;
    my $lockpid = $node->getlockpid;
    my $n1 = 0;
    my $n2 = 10;
    while (1) {
	my $ret = $lock->test;
	defined($ret) or $log->push, return undef;
	if (! $ret) {
	    $log->put("exit done")->push($node)->user;
	    last;
	}
	if ($lockpid != $lock->getpid) {
	    $log->put("restarted: lock pid changed %s->%s",
	    	$lockpid, $lock->getpid)->push($node);
	    return undef;
	}
	if (++$n1 >= $n2) {
	    $n2 *= 2;
	    $log->put("wait for exit")->push($node)->user;
	}
	select(undef, undef, undef, 0.1);
    }
    return 1;
}

sub cmd_stopnode_bg {
    my($node, $cmd) = @_;
    return $node->waitforexit;
}

sub cmd_killnode_fg {
    my($node, $cmd) = @_;
    my $pid = $node->getpid;
    $log->put("kill -9 $pid")->push($node)->user;
    kill(9, $pid);
    $node->setstate('stop')
	or $log->push($node), return undef;
    return 1;
}

sub cmd_killnode_bg {
    my($node, $cmd) = @_;
    return $node->waitforexit;
}

sub cmd_statnode_bg {
    my($node, $cmd) = @_;
    return "up";
}

sub cmd_writenode_fg {
    my($node, $cmd) = @_;
    my $text = $cmd->getarg(2);
    while(chomp($text)) {};
    $log->put("write: $text")->push($node)->user;
    $node->getiow->write("$text\n");
    my $output = "";
    if ((my $num = $cmd->getopt("wait")) > 0) {
	my $lim = time + $num;
	$node->getior->settimeout(1);
	loop: {
	    my $data = $node->getior->read;
	    if (length($data) > 0) {
		$node->handleoutput({user => 1}, $data);
		$output .= $data;
	    }
	    redo loop if time < $lim;
	}
	$node->getior->settimeout(0);
    }
    return { output => $output };
}

# commands

sub doremote {
    my $node = shift;
    my($cmdname, $opts, @args) = @_;
    my $server = $node->getserver;
    $log->put("$cmdname remote")->push($server)->push($node)->info;
    my $argv = [
    	$cmdname, q(--local),
	$opts, $node->getdb->getname, $node->getid, @args ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $server->request($cmd)
	or $log->push, return undef;
    return $ret;
}

sub dolocal {
    my $node = shift;
    my($cmdname, $opts, @args) = @_;
    $log->put("$cmdname local")->push($node)->info;
    if (! $node->getserver->islocal) {
	$log->put("not local")->push($node->getserver)->push($node);
	return undef;
    }
    if ($cmdname eq "startnode") {
	return $node->startlocal($opts);
    }
    my $lock = $node->getlock;
    anon: {
	my $ret = $lock->test;
	defined($ret) or $log->push, return undef;
	if (! $ret) {
	    if ($cmdname eq "statnode") {
		return "down";
	    }
	    $log->put("not running")->push($node)->user;
	    return $cmdname eq "writenode" ? undef : 1;
	}
    }
    my $server;
    anon: {
	my $path = $node->getsocketfile->getpath;
	if (! -e $path) {
	    $log->put("$path: no socket")->push($node);
	    return undef;
	}
	if ($^O ne 'MSWin32') {
	    $server = NDB::Net::ServerUNIX->new(id => 0, path => $path)
		or $log->push, return undef;
	} else {
	    $server = NDB::Net::ServerINET->new(id => 0, host => $node->getserver->getcanon, port => $node->getdb->getnodeport + $node->getid)
		or $log->push, return undef;
	}
    }
    my $argv = [
	$cmdname,
	$opts, $node->getdb->getname, $node->getid, @args ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
        or $log->push, return undef;
    my $ret = $server->request($cmd)
	or $log->push, return undef;
    $log->put("$cmdname done")->push($node)->info;
    return $ret;
}

sub start {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("start")->push($node)->info;
    my $do = $opts->{local} ? "dolocal" : "doremote";
    return $node->$do("startnode", $opts);
}

sub stop {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("stop")->push($node)->info;
    my $do = $opts->{local} ? "dolocal" : "doremote";
    return $node->$do("stopnode", $opts);
}

sub kill {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("kill")->push($node)->info;
    my $do = $opts->{local} ? "dolocal" : "doremote";
    return $node->$do("killnode", $opts);
}

sub stat {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("stat")->push($node)->info;
    my $do = $opts->{local} ? "dolocal" : "doremote";
    return $node->$do("statnode", $opts);
}

sub write {
    my $node = shift;
    @_ == 2 or confess 0+@_;
    my($opts, $text) = @_;
    $log->put("write: $text")->push($node)->info;
    my $do = $opts->{local} ? "dolocal" : "doremote";
    return $node->$do("writenode", $opts, $text);
}

1;
# vim:set sw=4:
