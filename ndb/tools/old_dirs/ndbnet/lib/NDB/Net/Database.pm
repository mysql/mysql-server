package NDB::Net::Database;

use strict;
use Carp;
use Symbol;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

my %dbcache = ();

NDB::Net::Database->attributes(
    name => sub { s/^\s+|\s+$//g; /^\S+$/ && ! m!/! },
    comment => sub { defined },
    version => sub { /^\d+(\.\d+)*$/ },
    base => sub { $^O eq 'MSWin32' || m!^/\S+$! },
    home => sub { $^O eq 'MSWin32' || m!^/\S+$! },
    nodeport => sub { $_ > 0 },
);

sub desc {
    my $db = shift;
    return $db->getname;
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $db = $class->SUPER::new(%attr);
    $db->setname($attr{name})
	or $log->push, return undef;
    if ($dbcache{$db->getname}) {
	$log->put("duplicate db")->push($db);
	return undef;
    }
    $db->setcomment($attr{comment});
    $db->setversion($attr{version})
	or $log->push, return undef;
    if (defined($attr{base})) {
	$db->setbase($attr{base})
	    or $log->push, return undef;
    }
    if (defined($attr{home})) {
	if ($^O ne 'MSWin32' && $attr{home} !~ m!^/! && $db->hasbase) {
	    $attr{home} = $db->getbase . "/$attr{home}";
	}
	$db->sethome($attr{home})
	    or $log->push, return undef;
    }
    if (defined($attr{nodeport})) {
	$db->setnodeport($attr{nodeport})
	    or $log->push, return undef;
    }
    if ($^O eq 'MSWin32' && ! $db->hasnodeport) {
	$log->put("nodeport required on windows")->push($db), return undef;
    }
    $db->{nodehash} = {};
    $dbcache{$db->getname} = $db;
    return $db;
}

sub delete {
    my $db = shift;
    my $nodelist = $db->getnodelist('all');
    for my $node (@$nodelist) {
	$node->delete;
    }
    delete $dbcache{$db->getname};
}

sub deleteall {
    my $class = shift;
    for my $name (sort keys %dbcache) {
	my $db = $dbcache{$name};
	$db->delete;
    }
}

# assume numerical dot separated version numbers like 1.1.2
sub cmpversion {
    my $db = shift;
    my $version = shift;
    my @x = split(/\./, $db->getversion);
    my @y = split(/\./, $version);
    while (@x || @y) {
	return -1 if $x[0] < $y[0];
	return +1 if $x[0] > $y[0];
	shift(@x);
	shift(@y);
    }
    return 0;
}

# nodes

sub addnode {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($node) = @_;
    unless (ref($node) && $node->isa('NDB::Net::Node')) {
	confess 'oops';
    }
    my $id = $node->getid;
    if ($db->{nodehash}{$id}) {
	$log->put("$id: duplicate node id")->push($db);
	return undef;
    }
    $db->{nodehash}{$id} = $node;
    return 1;
}

sub getnode {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($id) = @_;
    $id += 0;
    my $node = $db->{nodehash}{$id};
    if (! $node) {
	$log->put("$id: no such node id")->push($db);
	return undef;
    }
    return $node;
}

sub getnodelist {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($type) = @_;
    $type =~ /^(all|mgmt|db|api)$/ or confess 'oops';
    my @nodes = ();
    for my $id (sort { $a <=> $b } keys %{$db->{nodehash}}) {
	my $node = $db->{nodehash}{$id};
	if ($type eq 'all' or $type eq $node->gettype) {
	    push(@nodes, $node);
	}
    }
    return \@nodes;
}

# start /stop

sub start {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    if ($opts->{stop} || $opts->{kill}) {
	my $method = $opts->{stop} ? "stop" : "kill";
	my %opts = ();
	$db->$method(\%opts)
	    or $log->push, return undef;
    }
    $log->put("start")->push($db)->info;
    my $nodesmgmt = $db->getnodelist('mgmt');
    my $nodesdb = $db->getnodelist('db');
    my $nodesapi = $db->getnodelist('api');
    my $ret;
    try: {
	my %startopts = ();
	for my $k (qw(local init_rm nostart config old home clean proxy)) {
	    $startopts{$k} = $opts->{$k} if defined($opts->{$k});
	}
	my %writeopts = ();
	for my $k (qw(local)) {
	    $writeopts{$k} = $opts->{$k} if defined($opts->{$k});
	}
	if ($db->cmpversion("1.0") > 0) {
	    for my $node (@$nodesmgmt) {
		$node->start(\%startopts) or last try;
	    }
	    for my $node (@$nodesdb) {
		$node->start(\%startopts) or last try;
	    }
	    if (! $opts->{config}) {
		for my $node (@$nodesmgmt) {	# probably redundant
		    $node->write(\%writeopts, "all start") or last try;
		    last;
		}
	    }
	}
	else {
	    for my $node (@$nodesdb) {
		$node->start(\%startopts) or last try;
	    }
	    if (! $opts->{config}) {
		for my $node (@$nodesdb) {	# probably redundant
		    $node->write(\%writeopts, "start") or last try;
		}
	    }
	}
	for my $node (@$nodesapi) {
	    my %apiopts = %startopts;
	    if ($node->getruntype eq 'manual') {
		$apiopts{config} = 1;
	    }
	    $node->start(\%apiopts) or last try;
	}
	$ret = 1;
    }
    if (! $ret) {
	$log->push("start failed")->push($db);
	return undef;
    }
    my $msg = ! $opts->{config} ? "start done" : "config created";
    $log->put($msg)->push($db)->user;
    return 1;
}

sub stop {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("stop")->push($db)->info;
    my $nodesmgmt = $db->getnodelist('mgmt');
    my $nodesdb = $db->getnodelist('db');
    my $nodesapi = $db->getnodelist('api');
    my $ret;
    try: {
	for my $node (@$nodesapi) {
	    $node->stop($opts) or last try;
	}
	if ($db->cmpversion("1.0") > 0) {
	    for my $node (@$nodesmgmt) {
		$node->write($opts, "all stop") or last try;
		last;
	    }
	    for my $node (@$nodesdb) {
		$node->stop($opts) or last try;
	    }
	    for my $node (@$nodesmgmt) {
		$node->stop($opts) or last try;
	    }
	}
	else {
	    for my $node (@$nodesdb) {
		$node->write($opts, "stop") or last try;
	    }
	    for my $node (@$nodesdb) {
		$node->stop($opts) or last try;
	    }
	}
	$ret = 1;
    }
    if (! $ret) {
	$log->push("stop failed")->push($db);
	return undef;
    }
    $log->put("stop done")->push($db)->user;
    return 1;
}

sub kill {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    $log->put("kill")->push($db)->info;
    my $nodesmgmt = $db->getnodelist('mgmt');
    my $nodesdb = $db->getnodelist('db');
    my $nodesapi = $db->getnodelist('api');
    my $ret = 1;
    try: {
	for my $node (@$nodesapi) {
	    $node->kill($opts) || ($ret = undef);
	}
	for my $node (@$nodesdb) {
	    $node->kill($opts) || ($ret = undef);
	}
	for my $node (@$nodesmgmt) {
	    $node->kill($opts) || ($ret = undef);
	}
    }
    if (! $ret) {
	$log->push("kill failed")->push($db);
	return undef;
    }
    $log->put("kill done")->push($db)->user;
    return 1;
}

sub list {
    my $db = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    my $dbsts = {};
    $dbsts->{comment} = $db->getcomment("");
    $dbsts->{home} = $db->gethome;
    $log->put("status")->push($db)->info;
    my $mgmsts;
    for my $node (@{$db->getnodelist('mgmt')}) {
	$mgmsts = $node->get_status or
	    $log->push->error;
	last;
    }
    $mgmsts ||= {};
    for my $node (@{$db->getnodelist('all')}) {
	my $id = $node->getid;
	my $nodests = $dbsts->{node}{$id} ||= {};
	my $stat = $node->stat($opts) or
	    $log->push->error;
	$nodests->{id} = $id;
	$nodests->{type} = $node->gettype;
	$nodests->{comment} = $node->getcomment("");
	$nodests->{host} = $node->getserver->gethost;
	$nodests->{run} = $stat || "error";
	$nodests->{status} = $mgmsts->{node}{$id};
    }
    return $dbsts;
}

1;
# vim:set sw=4:
