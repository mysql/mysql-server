package NDB::Util::SocketUNIX;

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

NDB::Util::SocketUNIX->attributes(
    path => sub { /^\S+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $socket = $class->SUPER::new(%attr,
	domain => PF_UNIX, type => SOCK_STREAM, proto => 0)
	or $log->push, return undef;
    return $socket;
}

sub connect {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($path) = @_;
    $path =~ /^\S+$/ or confess 'oops';
    my $paddr = pack_sockaddr_un($path);
    $socket->SUPER::connect($paddr)
	or $log->push, return undef;
    $socket->setpath($path)
	or $log->push, return undef;
    return 1;
}

sub bind {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($path) = @_;
    $path =~ /^\S+$/ or confess 'oops';
    my $paddr = pack_sockaddr_un($path);
    $socket->SUPER::bind($paddr)
	or $log->push, return undef;
    $socket->setpath($path)
	or $log->push, return undef;
    return 1;
}

sub acceptaddr {
    my $csocket = shift;
    @_ == 1 or confess 0+@_;
    my($paddr) = @_;
    return 1;		# crash
    my $path = unpack_sockaddr_un($paddr);
    $csocket->setpath($path)
	or $log->push, return undef;
    $log->put("%s accept: path=%s",
	$csocket->getpath)->push($csocket)->debug;
    return 1;
}

1;
# vim:set sw=4:
