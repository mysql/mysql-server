package NDB::Net::ServerUNIX;

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

NDB::Net::ServerUNIX->attributes(
    path => sub { s/^\s+|\s+$//g; /^\S+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $server = $class->SUPER::new(%attr);
    $server->setid($attr{id})
	or $log->push, return undef;
    $server->setdomain(PF_UNIX)
	or $log->push, return undef;
    $server->setpath($attr{path})
	or $log->push, return undef;
    $server->add or
    	$log->push, return undef;
    return $server;
}

# client side

sub connect {
    my $server = shift;
    @_ == 0 or confess 0+@_;
    my $socket;
    $socket = NDB::Util::SocketUNIX->new or
	$log->push, return undef;
    $socket->connect($server->getpath) or
	$log->push, return undef;
    return $socket;
}

1;
# vim:set sw=4:
