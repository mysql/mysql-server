package NDB::Util::SocketINET;

use strict;
use Carp;
use Symbol;
use Socket;
use Errno;

require NDB::Util::Socket;

use vars qw(@ISA);
@ISA = qw(NDB::Util::Socket);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::SocketINET->attributes(
    host => sub { /^\S+$/ },
    port => sub { /^\d+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $socket = $class->SUPER::new(%attr,
	domain => PF_INET, type => SOCK_STREAM, proto => 'tcp')
	or $log->push, return undef;
    return $socket;
}

sub connect {
    my $socket = shift;
    @_ == 2 or confess 0+@_;
    my($host, $port) = @_;
    $port =~ /^\d+$/ or confess 'oops';
    my $iaddr = inet_aton($host);
    if (! $iaddr) {
	$log->put("host $host not found")->push($socket);
	return undef;
    }
    my $paddr = pack_sockaddr_in($port, $iaddr);
    $socket->SUPER::connect($paddr)
	or $log->push, return undef;
    $socket->sethost($host)
	or $log->push, return undef;
    $socket->setport($port)
	or $log->push, return undef;
    return 1;
}

sub bind {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($port) = @_;
    $port =~ /^\d+$/ or confess 'oops';
    my $paddr = pack_sockaddr_in($port, INADDR_ANY);
    $socket->SUPER::bind($paddr)
	or $log->push, return undef;
    $socket->setport($port)
	or $log->push, return undef;
    return 1;
}

sub acceptaddr {
    my $csocket = shift;
    @_ == 1 or confess 0+@_;
    my($paddr) = @_;
    my($port, $iaddr) = unpack_sockaddr_in($paddr);
    my $host = gethostbyaddr($iaddr, AF_INET);
    $csocket->sethost($host)
	or $log->push, return undef;
    $csocket->setport($port)
	or $log->push, return undef;
    $log->put("accept: host=%s port=%d",
	$csocket->gethost, $csocket->getport)->push($csocket)->debug;
    return 1;
}

1;
# vim:set sw=4:
