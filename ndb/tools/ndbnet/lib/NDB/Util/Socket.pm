package NDB::Util::Socket;

use strict;
use Carp;
use Symbol;
use Socket;
use Errno;

require NDB::Util::IO;

use vars qw(@ISA);
@ISA = qw(NDB::Util::IO);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::Socket->attributes(
    domain => sub { $_ == PF_INET || $_ == PF_UNIX },
    type => sub { $_ == SOCK_STREAM },
    proto => sub { /^(0|tcp)$/ },
);

sub desc {
    my $socket = shift;
    return $socket->SUPER::desc;
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $socket = $class->SUPER::new(%attr);
    $socket->setdomain($attr{domain})
	or $log->push, return undef;
    $socket->settype($attr{type})
	or $log->push, return undef;
    $socket->setproto($attr{proto})
	or $log->push, return undef;
    my $nproto;
    if ($socket->getproto =~ /^\d+/) {
	$nproto = $socket->getproto;
    }
    else {
	$nproto = getprotobyname($socket->getproto);
	unless (defined($nproto)) {
	    $log->put("%s: getprotobyname failed", $socket->getproto);
	    return undef;
	}
    }
    my $fh = gensym();
    if (! socket($fh, $socket->getdomain, $socket->gettype, $nproto)) {
	$log->put("create socket failed: $!");
	return undef;
    }
    $socket->setfh($fh)
	or $log->push, return undef;
    return $socket;
}

sub setopt {
    my $socket = shift;
    @_ >= 2 or confess 'oops';
    my $level = shift;
    my $optname = shift;
    my $optval = @_ ? pack("l*", @_) : undef;
    my $fh = $socket->getfh;
    if (! setsockopt($fh, $level, $optname, $optval)) {
	$log->put("setsockopt failed: $!")->push($socket);
	return undef;
    }
    return 1;
}

sub connect {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($paddr) = @_;
    my $fh = $socket->getfh;
    if (! connect($fh, $paddr)) {
	$log->put("connect failed: $!")->push($socket);
	return undef;
    }
    $log->put("connect done")->push($socket)->debug;
    return 1;
}

sub bind {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($paddr) = @_;
    my $fh = $socket->getfh;
    if (! bind($fh, $paddr)) {
	$log->put("bind failed: $!")->push($socket);
	return undef;
    }
    return 1;
}

sub listen {
    my $socket = shift;
    @_ == 0 or confess 0+@_;
    my $fh = $socket->getfh;
    if (! listen($fh, SOMAXCONN)) {
	$log->put("listen failed: $!")->push($socket);
	return undef;
    }
    return 1;
}

sub accept {
    my $socket = shift;
    @_ == 1 or confess 0+@_;
    my($timeout) = @_;
    $timeout =~ /^\d+$/ or confess 'oops';
    my $fh = $socket->getfh;
    my $gh = gensym();
    my $paddr;
    eval {
	if ($^O ne 'MSWin32' && $timeout > 0) {
	    local $SIG{ALRM} = sub { die("timed out\n") };
	    alarm($timeout);
	    $paddr = accept($gh, $fh);
	    alarm(0);
	}
	else {
	    $paddr = accept($gh, $fh);
	}
    };
    if ($@) {
	$log->put("accept failed: $@")->push($socket);
	return undef;
    }
    if (! $paddr) {
	my $errno = 0+$!;
	if ($errno == Errno::EINTR) {
	    $log->put("accept interrupted")->push($socket);
	    return 0;
	}
	$log->put("accept failed: $!")->push($socket);
	return undef;
    }
    my $csocket = $socket->clone(fh => $gh);
    $csocket->acceptaddr($paddr);
    return $csocket;
}

sub DESTROY {
    my $socket = shift;
    $socket->close;
}

1;
# vim:set sw=4:
