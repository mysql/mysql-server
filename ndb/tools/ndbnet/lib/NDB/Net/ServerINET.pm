package NDB::Net::ServerINET;

use strict;
use Carp;
use Socket;

require NDB::Net::Server;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Server);

# constructors 

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Net::ServerINET->attributes(
    host => sub { s/^\s+|\s+$//g; /^\S+$/ },
    port => sub { s/^\s+|\s+$//g; /^\d+$/ },
    canon => sub { s/^\s+|\s+$//g; /^\S+$/ },
    aliases => sub { ref($_) eq 'ARRAY' },
);


sub desc {
    my $server = shift;
    my $id = $server->getid;
    my $host = $server->gethost;
    return "server $id at $host";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $server = $class->SUPER::new(%attr);
    $server->setid($attr{id})
	or $log->push, return undef;
    $server->setdomain(PF_INET)
	or $log->push, return undef;
    $server->sethost($attr{host})
	or $log->push, return undef;
    $server->setport($attr{port})
	or $log->push, return undef;
    my($canon, $aliases) = gethostbyname($server->gethost);
    if (! defined($canon)) {
	$log->put("%s: unknown host", $server->gethost);
	return undef;
    }
    $server->setcanon($canon)
	or $log->push, return undef;
    $server->setaliases([ split(' ', $aliases) ])
	or $log->push, return undef;
    $server->add or
    	$log->push, return undef;
    $log->put("added")->push($server)->debug;
    return $server;
}

# find matching servers

sub match {
    my $class = shift;
    @_ == 3 or confess 0+@_;
    my($host, $port, $servers) = @_;
    if (! defined($port) && $host =~ /:/) {
	($host, $port) = split(/:/, $host, 2);
    }
    $host =~ s/^\s+|\s+$//g;
    my($canon) = gethostbyname($host);
    unless (defined($canon)) {
	$log->put("$host: unknown host");
	return undef;
    }
    my $hostport = $host;
    if (defined($port)) {
	$port =~ s/^\s+|\s+$//g;
	$port =~ /\d+$/
	    or $log->put("$port: non-numeric port"), return undef;
	$hostport .= ":$port";
    }
    my @server = ();
    for my $s (@$servers) {
	($s->getdomain == PF_INET) || next;
	($s->getcanon eq $canon) || next;
	($port && $s->getport != $port) && next;
	push(@server, $s);
    }
    if (! @server) {
	$log->put("$hostport: no server found");
    }
    if (@server > 1) {
	$log->put("$hostport: multiple servers at ports ",
	    join(' ', map($_->getport, @server)));
    }
    return \@server;
}

# client side

sub connect {
    my $server = shift;
    @_ == 0 or confess 0+@_;
    my $socket;
    $socket = NDB::Util::SocketINET->new or
	$log->push, return undef;
    $socket->connect($server->gethost, $server->getport) or
	$log->push, return undef;
    return $socket;
}

1;
# vim:set sw=4:
