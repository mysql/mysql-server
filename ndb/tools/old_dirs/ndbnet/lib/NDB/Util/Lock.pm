package NDB::Util::Lock;

use strict;
use Carp;
use Symbol;
use Fcntl qw(:flock);
use Errno;
use File::Basename;

require NDB::Util::File;

use vars qw(@ISA);
@ISA = qw(NDB::Util::File);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::Lock->attributes(
    pid => sub { $_ != 0 },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $lock = $class->SUPER::new(%attr);
    return $lock;
}

sub desc {
    my $lock = shift;
    return $lock->getpath;
}

# test / set

sub test {
    my $lock = shift;
    @_ == 0 or confess 0+@_;
    my $fh = gensym();
    if (! open($fh, "+<$lock->{path}")) {
	if ($! != Errno::ENOENT) {
	    $log->put("$lock->{path}: open failed: $!");
	    return undef;
	}
	return 0;	# file does not exist
    }
    if (flock($fh, LOCK_EX|LOCK_NB)) {
	close($fh);
	return 0;	# file was not locked
    }
    if ($^O eq 'MSWin32') {
	close($fh);
	if (! open($fh, "<$lock->{path}x")) {
	    $log->put("$lock->{path}x: open failed: $!");
	    return undef;
	}
    }
    my $pid = <$fh>;
    close($fh);
    ($pid) = split(' ', $pid);
    if ($pid+0 == 0) {
	$log->put("$lock->{path}: locked but pid='$pid' is zero");
	return undef;
    }
    $lock->{pid} = $pid;
    return 1;		# file was locked
}

sub set {
    my $lock = shift;
    @_ == 0 or confess 0+@_;
    my $fh = gensym();
    if (! open($fh, "+<$lock->{path}")) {
	if ($! != Errno::ENOENT) {
	    $log->put("$lock->{path}: open failed: $!");
	    return undef;
	}
	close($fh);
	if (! open($fh, ">$lock->{path}")) {
	    $log->put("$lock->{path}: create failed: $!");
	    return undef;
	}
    }
    if (! flock($fh, LOCK_EX|LOCK_NB)) {
	$log->put("$lock->{path}: flock failed: $!");
	close($fh);
	return 0;	# file was probably locked
    }
    my $line = "$$\n";
    if ($^O eq 'MSWin32') {
	my $gh = gensym();
	if (! open($gh, ">$lock->{path}x")) {
	    $log->put("$lock->{path}x: open for write failed: $!");
	    close($fh);
	    return undef;
	}
	if (! syswrite($gh, $line)) {
	    close($fh);
	    close($gh);
	    $log->put("$lock->{path}x: write failed: $!");
	    return undef;
	}
	close($gh);
    } else {
	if (! truncate($fh, 0)) {
	    close($fh);
	    $log->put("$lock->{path}: truncate failed: $!");
	    return undef;
	}
	if (! syswrite($fh, $line)) {
	    close($fh);
	    $log->put("$lock->{path}: write failed: $!");
	    return undef;
	}
    }
    $lock->{fh} = $fh;
    return 1;		# file is now locked by us
}

sub close {
    my $lock = shift;
    @_ == 0 or confess 0+@_;
    my $fh = delete $lock->{fh};
    if ($fh) {
	close($fh);
    }
}

1;
# vim:set sw=4:
