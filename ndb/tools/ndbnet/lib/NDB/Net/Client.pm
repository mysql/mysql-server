package NDB::Net::Client;

use strict;
use Carp;
use POSIX();
use Socket;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors 

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

my %clientcache = ();
my $clientid = 0;

NDB::Net::Client->attributes(
    id => sub { /^\d+$/ },
    addtime => sub { /^\d+$/ },
    state => sub { /^(new|input|cmd)$/ },
    socket => sub { ref && $_->isa('NDB::Util::Socket') },
    serversocket => sub { ref && $_->isa('NDB::Util::Socket') },
    serverlock => sub { ref && $_->isa('NDB::Util::Lock') },
    event => sub { ref && $_->isa('NDB::Util::Event') },
    context => sub { defined },
    cmd => sub { ref && $_->isa('NDB::Net::Command') },
);

sub desc {
    my $client = shift;
    my $id = $client->getid;
    my $fileno = fileno($client->getsocket->getfh);
    return "client $id fd=$fileno";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $client = $class->SUPER::new(%attr);
    $client->setid(++$clientid)
	or $log->push, return undef;
    $client->setaddtime(time)
	or $log->push, return undef;
    $client->setstate(q(new))
	or $log->push, return undef;
    $client->setsocket($attr{socket})
	or $log->push, return undef;
    $client->setserversocket($attr{serversocket})
	or $log->push, return undef;
    $client->setserverlock($attr{serverlock})
	or $log->push, return undef;
    $client->setevent($attr{event})
	or $log->push, return undef;
    $client->setcontext($attr{context})
	or $log->push, return undef;
    $log->put("add")->push($client)->info;
    $clientcache{$client->getid} = $client;
    return $client;
}

sub listall {
    my $class = shift;
    my $list = [];
    for my $id (sort { $a <=> $b } keys %clientcache) {
	my $client = $clientcache{$id};
	push(@$list, $client);
    }
    return $list;
}

sub exists {
    my $client = shift;
    return exists($clientcache{$client->getid});
}

sub delete {
    my $client = shift;
    $log->put("delete")->push($client)->info;
    $client->getevent->clear($client->getsocket, 'r');
    $client->getsocket->close;
    delete $clientcache{$client->getid} or confess 'oops';
}

sub deleteother {
    my $thisclient = shift;
    for my $id (sort { $a <=> $b } keys %clientcache) {
	my $client = $clientcache{$id};
	if ($client ne $thisclient) {
	    $client->delete;
	}
    }
}

sub deleteall {
    my $class = shift;
    for my $id (sort { $a <=> $b } keys %clientcache) {
	my $client = $clientcache{$id};
	$client->delete;
    }
}

# processing

sub processnew {
    my $client = shift;
    @_ == 0 or confess 0+@_;
    $log->put("process new")->push($client)->debug;
    $client->getevent->set($client->getsocket, 'r');
    $log->attachuser(io => $client->getsocket);
    $client->setstate(q(input))
	or $log->push, return undef;
    return 1;
}

sub processinput {
    my $client = shift;
    @_ == 0 or confess 0+@_;
    $log->put("process input")->push($client)->debug;
    my $line = $client->getsocket->readline;
    if (! defined($line)) {
	$log->push;
	return undef;
    }
    if (length($line) == 0) {
	if ($client->getsocket->getreadend) {
	    $log->put("no command")->push($client);
	    return undef;
	}
	$log->put("wait for input")->push($client)->debug;
	return 1;
    }
    $log->put("got line: $line")->push($client)->info;
    $client->getevent->clear($client->getsocket, 'r');
    my $cmd = NDB::Net::Command->new(line => $line)
	or $log->push, return undef;
    $log->put("command received")->push($cmd)->push($client)->debug;
    $client->setcmd($cmd)
	or $log->push, return undef;
    $client->setstate(q(cmd))
	or $log->push, return undef;
    return 1;
}

sub processcmd {
    my $client = shift;
    @_ == 0 or confess 0+@_;
    $log->put("process cmd")->push($client)->debug;
    my $cmd = $client->getcmd;
    my $context = $client->getcontext;
    my $name_fg = "cmd_" . $cmd->getname . "_fg";
    my $name_bg = "cmd_" . $cmd->getname . "_bg";
    my $fg = $context->can($name_fg);
    my $bg = $context->can($name_bg);
    unless ($fg || $bg) {
	$log->put("%s: unimplemented", $cmd->getname);
	return undef;
    }
    my $ret;
    if ($fg) {
	$log->put($name_fg)->push($cmd)->push($client)->info;
	if (! ref($context)) {
	    $ret = &$fg($cmd);
	}
	else {
	    $ret = &$fg($context, $cmd);
	}
	defined($ret)
	    or $log->push, return undef;
	if (! $bg) {
	    $log->push($name_fg)->putvalue($ret)->user;
	    return 1;
	}
    }
    if ($bg) {
	$log->put($name_bg)->push($cmd)->push($client)->info;
	my $pid = fork;
	if (! defined($pid)) {
	    $log->put("fork failed: $!");
	    return undef;
	}
	if ($pid == 0) {
	    $client->getserversocket->close;
	    $client->getserverlock->close;
	    $client->deleteother;
	    if (! ref($context)) {
		$ret = &$bg($cmd);
	    }
	    else {
		$ret = &$bg($context, $cmd);
	    }
	    if (! $ret) {
		$log->push($client)->error;
		$log->push($name_bg)->putvalue(undef)->user;
		exit(1);
	    }
	    $log->push($name_bg)->putvalue($ret)->user;
	    exit(0);
	}
    }
    return 1;
}

sub process {
    my $client = shift;
    @_ == 0 or confess 0+@_;
    try: {
	if ($client->getstate eq q(new)) {
	    $client->processnew
		or $log->push, last try;
	}
	if ($client->getstate eq q(input)) {
	    $client->processinput
		or $log->push, last try;
	}
	if ($client->getstate eq q(cmd)) {
	    $client->processcmd
		or $log->push, last try;
	    $log->detachuser;
	    $client->delete;
	    return 1;
	}
	return 1;
    }
    $log->push($client)->error;
    $log->putvalue(undef)->user;
    $log->detachuser;
    $client->delete;
    return undef;
}

sub processall {
    my $class = shift;
    @_ == 0 or confess 0+@_;
    my $list = $class->listall;
    for my $client (@$list) {
	$client->process;
    }
    while ((my $pid = waitpid(-1, &POSIX::WNOHANG)) > 0) {
	$log->put("harvested pid=$pid")->info;
    }
}

1;
# vim:set sw=4:
