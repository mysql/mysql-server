package NDB::Util::IO;

use strict;
use Carp;

require NDB::Util::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Util::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::IO->attributes(
    readbuf => sub { defined },
    readend => sub { defined },
    writebuf => sub { defined },
    writeend => sub { defined },
    iosize => sub { $_ > 0 },
    timeout => sub { /^\d+$/ },
    fh => sub { ref($_) eq 'GLOB' && defined(fileno($_)) },
);

sub desc {
    my $io = shift;
    my $fileno = $io->hasfh ? fileno($io->getfh) : -1;
    return "fd=$fileno";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $io = $class->SUPER::new(%attr);
    $io->setreadbuf("")
	or $log->push, return undef;
    $io->setreadend(0)
	or $log->push, return undef;
    $io->setwritebuf("")
	or $log->push, return undef;
    $io->setwriteend(0)
	or $log->push, return undef;
    $io->setiosize(1024)
	or $log->push, return undef;
    $io->settimeout(0)
	or $log->push, return undef;
    if (defined($attr{fh})) {
	$io->setfh($attr{fh})
	    or $log->push, return undef;
    }
    return $io;
}

# input / output

sub read {
    my $io = shift;
    @_ == 0 or confess 0+@_;
    if ($io->getreadend) {
	return "";
    }
    my $size = $io->getiosize;
    my $timeout = $io->hastimeout ? $io->gettimeout : 0;
    my $fh = $io->getfh;
    my $n;
    my $data;
    eval {
	if ($^O ne 'MSWin32' && $timeout > 0) {
	    local $SIG{ALRM} = sub { die("timed out\n") };
	    alarm($timeout);
	    $n = sysread($fh, $data, $size);
	    alarm(0);
	}
	else {
	    $n = sysread($fh, $data, $size);
	}
    };
    if ($@) {
	$log->put("read error: $@")->push($io);
	return undef;
    }
    if (! defined($n)) {
	$log->put("read failed: $!")->push($io);
	return undef;
    }
    if ($n == 0) {
	$io->setreadend(1)
	    or $log->push, return undef;
	$log->put("read EOF")->push($io)->debug;
	return "";
    }
    (my $show = $data) =~ s!\n!\\n!g;
    $log->put("read: $show")->push($io)->debug;
    return $data;
}

sub readbuf {
    my $io = shift;
    @_ == 0 or confess 0+@_;
    my $data = $io->read;
    defined($data) or
	$log->push, return undef;
    if (length($data) == 0) {
	return 0;
    }
    $io->setreadbuf($io->getreadbuf . $data)
	or $log->push, return undef;
    return 1;
}

sub readupto {
    my $io = shift;
    @_ == 1 or confess 0+@_;
    my($code) = @_;
    ref($code) eq 'CODE' or confess 'oops';
    my $k = &$code($io->getreadbuf);
    if (! defined($k)) {
	$log->push($io);
	return undef;
    }
    if ($k == 0) {
	my $n = $io->readbuf;
	defined($n) or
	    $log->push, return undef;
	if ($n == 0) {
	    if ($io->getreadbuf eq "") {
		return "";
	    }
	    $log->put("incomplete input: %s", $io->getreadbuf)->push($io);
	    return undef;
	}
	$k = &$code($io->getreadbuf);
	if (! defined($k)) {
	    $log->push($io);
	    return undef;
	}
	if ($k == 0) {
	    return "";
	}
    }
    my $head = substr($io->getreadbuf, 0, $k);
    my $tail = substr($io->getreadbuf, $k);
    $io->setreadbuf($tail)
	or $log->push, return undef;
    return $head;
}

sub readline {
    my $io = shift;
    @_ == 0 or confess 0+@_;
    my $code = sub {
	my $i = index($_[0], "\n");
	return $i < 0 ? 0 : $i + 1;
    };
    return $io->readupto($code);
}

sub write {
    my $io = shift;
    @_ == 1 or confess 0+@_;
    my($data) = @_;
    my $timeout = $io->hastimeout ? $io->gettimeout : 0;
    my $fh = $io->getfh;
    (my $show = $data) =~ s!\n!\\n!g;
    $log->put("write: $show")->push($io)->debug;
    my $n;
    my $size = length($data);
    eval {
	local $SIG{PIPE} = sub { die("broken pipe\n") };
	if ($^O ne 'MSWin32' && $timeout > 0) {
	    local $SIG{ALRM} = sub { die("timed out\n") };
	    alarm($timeout);
	    $n = syswrite($fh, $data, $size);
	    alarm(0);
	}
	else {
	    $n = syswrite($fh, $data, $size);
	}
    };
    if ($@) {
	$log->put("write error: $@")->push($io);
	return undef;
    }
    if (! defined($n)) {
	$log->put("write failed: $!")->push($io);
	return undef;
    }
    if ($n > $size) {
	$log->put("impossible write: $n > $size")->push($io);
	return undef;
    }
    if ($n != $size) {		# need not be error
	$log->put("short write: $n < $size")->push($io);
    }
    return $n;
}

sub close {
    my $io = shift;
    @_ == 0 or confess 0+@_;
    if (! close($io->delfh)) {
	$log->put("close failed: $!")->push($io);
	return undef;
    }
    return 1;
}

1;
