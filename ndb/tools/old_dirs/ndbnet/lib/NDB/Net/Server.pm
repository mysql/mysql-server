package NDB::Net::Server;

use strict;
use Carp;
use Socket;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors 

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

my %servercache = ();

NDB::Net::Server->attributes(
    id => sub { s/^\s+|\s+$//g; m/^\S+$/ && ! m!/! },
    domain => sub { $_ == PF_UNIX || $_ == PF_INET },
);

sub desc {
    my $server = shift;
    my $id = $server->getid;
    return "server $id";
}

sub add {
    my $server = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    if ($servercache{$server->getid}) {
	$log->put("duplicate server")->push($server);
	return undef;
    }
    $servercache{$server->getid} = $server;
    return 1;
}

sub get {
    my $class = shift;
    @_ == 1 or confess 0+@_;
    my($id) = @_;
    $id =~ s/^\s+|\s+$//g;
    my $server = $servercache{$id};
    if (! $server) {
	$log->put("$id: undefined server");
	return undef;
    }
    $log->put("found")->push($server)->debug;
    return $server;
}

sub delete {
    my $server = shift;
    delete $servercache{$server->getid};
}

sub deleteall {
    my $class = shift;
    for my $id (sort keys %servercache) {
	my $server = $servercache{$id};
	$server->delete;
    }
}

# local server is this server process

my $localserver;

sub setlocal {
    my $server = shift;
    @_ == 0 or confess 0+@_;
    $localserver = $server;
}

sub islocal {
    my $server = shift;
    @_ == 0 or confess 0+@_;
    return $localserver eq $server;
}

# client side

sub testconnect {
    my $server = shift;
    @_ == 0 or confess 0+@_;
    my $socket = $server->connect or
	$log->push($server), return undef;
    $socket->close;
    return 1;
}

sub request {
    my $server = shift;
    @_ == 1 or confess 0+@_;
    my($cmd) = @_;
    unless (ref($cmd) && $cmd->isa('NDB::Net::Command')) {
	confess 'oops';
    }
    my $socket = $server->connect
	or $log->push($server), return undef;
    anon: {
	my $line = $cmd->getline;
	my $n = $socket->write("$line\n");
	defined($n) && $n == length("$line\n")
	    or $log->push($server), return undef;
	shutdown($socket->{fh}, 1);
    }
    my $value;
    try: {
	my $last;
	loop: {
	    my $line = $socket->readline;
	    defined($line)
		or $log->push($server), last try;
	    if ($socket->getreadend) {
		last loop;
	    }
	    while (chomp($line)) {}
	    $log->put($line)->user
		unless $log->hasvalue($line);
	    $last = $line;
	    redo loop;
	}
	if (! $log->hasvalue($last)) {
	    $log->put("missing return value in \"$last\"")->push($server);
	    last try;
	}
	$value = $log->getvalue($last);
	defined($value)
	    or $log->push, last try;
	$value = $value->[0];
	if (! defined($value)) {
	    $log->put("failed")->push($cmd);
	    last try;
	}
    }
    $socket->close;
    return $value;
}

1;
# vim:set sw=4:
