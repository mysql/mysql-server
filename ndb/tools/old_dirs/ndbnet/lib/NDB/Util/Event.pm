package NDB::Util::Event;

use strict;
use Carp;
use Errno;

require NDB::Util::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Util::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Util::Event->attributes();

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $event = $class->SUPER::new(%attr);
    return $event;
}

# set and test bits

sub check {
    my $event = shift;
    my($file, $type) = @_;
    my $fileno;
    if (ref($file) eq 'GLOB') {
	$fileno = fileno($file);
    }
    elsif (ref($file)) {
	$file->can("getfh") or confess 'oops';
	$fileno = fileno($file->getfh);
    }
    else {
	$fileno = $file;
    }
    defined($fileno) or confess 'oops';
    $fileno =~ s/^\s+|\s+$//g;
    $fileno =~ /^\d+$/ or confess 'oops';
    $type =~ /^[rwe]$/ or confess 'oops';
    return ($fileno, $type);
}

sub set {
    my $event = shift;
    @_ == 2 or confess 0+@_;
    my($fileno, $type) = $event->check(@_);
    vec($event->{"i_$type"}, $fileno, 1) = 1;
}

sub clear {
    my $event = shift;
    @_ == 2 or confess 0+@_;
    my($fileno, $type) = $event->check(@_);
    vec($event->{"i_$type"}, $fileno, 1) = 0;
}

sub test {
    my $event = shift;
    @_ == 2 or confess 0+@_;
    my($fileno, $type) = $event->check(@_);
    return vec($event->{"o_$type"}, $fileno, 1);
}

# poll

sub poll {
    my $event = shift;
    @_ <= 1 or confess 'oops';
    my $timeout = shift;
    if (defined($timeout)) {
	$timeout =~ /^\d+$/ or confess 'oops';
    }
    $event->{o_r} = $event->{i_r};
    $event->{o_w} = $event->{i_w};
    $event->{o_e} = $event->{i_e};
    my $n;
    $n = select($event->{o_r}, $event->{o_w}, $event->{o_e}, $timeout);
    if ($n < 0 || ! defined($n)) {
	if ($! == Errno::EINTR) {
	    $log->put("select interrupted");
	    return 0;
	}
	$log->put("select failed: $!");
	return undef;
    }
    if (! $n) {
	$log->put("select timed out");
    }
    return $n;
}

1;
# vim:set sw=4:
