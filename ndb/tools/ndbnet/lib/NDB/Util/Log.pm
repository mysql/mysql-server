package NDB::Util::Log;

use strict;
use Carp;
use Symbol;
use Data::Dumper ();

require NDB::Util::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Util::Base);

# constructors

my $instance = undef;
my %attached = ();

my %priolevel = qw(user 0 fatal 1 error 2 warn 3 notice 4 info 5 debug 6);
my %partlist = qw(time 1 pid 2 prio 3 text 4 line 5);

NDB::Util::Log->attributes(
    prio => sub { defined($priolevel{$_}) },
    parts => sub { ref eq 'HASH' },
    stack => sub { ref eq 'ARRAY' },
    io => sub { ref && $_->isa('NDB::Util::IO') },
    active => sub { defined },
    censor => sub { ref eq 'ARRAY' },
);

sub setpart {
    my $log = shift;
    @_ % 2 == 0 or confess 0+@_;
    while (@_) {
	my $part = shift;
	my $onoff = shift;
	$partlist{$part} or confess 'oops';
	$log->getparts->{$part} = $onoff;
    }
}

sub getpart {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($part) = @_;
    $partlist{$part} or confess 'oops';
    return $log->getparts->{$part};
}

sub instance {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    if (! $instance) {
	$instance = $class->SUPER::new(%attr);
	$instance->setprio(q(info));
	$instance->setparts({ text => 1 });
	$instance->setstack([]);
	$instance->setcensor([]);
	my $io = NDB::Util::IO->new(fh => \*STDERR, %attr)
	    or confess 'oops';
	$instance->setio($io);
    }
    return $instance;
}

# attached logs are written in parallel to main log
# user log is a special server-to-client log

sub attach {
    my $log = shift;
    @_ % 2 == 1 or confess 0+@_;
    my($key, %attr) = @_;
    $attached{$key} and confess 'oops';
    my $alog = $attached{$key} = $log->clone(%attr);
    return $alog;
}

sub detach {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($key) = @_;
    $attached{$key} or return undef;
    my $alog = delete $attached{$key};
    return $alog;
}

sub attachuser {
    my $log = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    %attr = (
	prio => q(user),
	parts => { text => 1 },
	censor => [ qw(NDB::Net::Client NDB::Util::IO) ],
	%attr);
    my $alog = $log->attach(q(user), %attr);
    return $alog;
}

sub detachuser {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    my $alog = $log->detach(q(user));
    return $alog;
}

# input / output

sub setfile {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my $file = shift;
    if (! open(STDOUT, ">>$file")) {
	$log->put("$file: open for append failed: $!");
	return undef;
    }
    select(STDOUT);
    $| = 1;
    open(STDERR, ">&STDOUT");
    select(STDERR);
    $| = 1;
    return 1;
}

sub close {
    my $log = shift;
    $log->getio->close;
}

sub closeall {
    my $class = shift;
    for my $key (sort keys %attached) {
	my $log = $attached{$key};
	$log->close;
    }
    $instance->close;
}

# private

sub entry {
    my $log = shift;
    my($clear, $file, $line, @args) = @_;
    $file =~ s!^.*\bNDB/!!;
    $file =~ s!^.*/bin/([^/]+)$!$1!;
    my $text = undef;
    if (@args) {
	$text = shift(@args);
	if (! ref($text)) {
	    if (@args) {
		$text = sprintf($text, @args);
	    }
	    while (chomp($text)) {}
	}
    }
    if ($clear) {
	$#{$log->getstack} = -1;
    }
    push(@{$log->getstack}, {
	line => "$file($line)",
	text => $text,
    });
}

sub matchlevel {
    my $log = shift;
    my $msgprio = shift;
    my $logprio = $log->getprio;
    my $msglevel = $priolevel{$msgprio};
    my $loglevel = $priolevel{$logprio};
    defined($msglevel) && defined($loglevel)
	or confess 'oops';
    if ($msglevel == 0 && $loglevel == 0) {
	return $msgprio eq $logprio;
    }
    if ($msglevel == 0 && $loglevel != 0) {
	return $loglevel >= $priolevel{q(info)};
    }
    if ($msglevel != 0 && $loglevel == 0) {
	return $msglevel <= $priolevel{q(notice)};
    }
    if ($msglevel != 0 && $loglevel != 0) {
	return $msglevel <= $loglevel;
    }
    confess 'oops';
}

sub print {
    my $log = shift;
    @_ == 2 or confess 0+@_;
    my($prio, $tmpstack) = @_;
    if ($log->hasactive) {	# avoid recursion
	return;
    }
    if (! $log->matchlevel($prio)) {
	return;
    }
    $log->setactive(1);
    my @text = ();
    if ($log->getpart(q(time))) {
	my @t = localtime(time);
	push(@text, sprintf("%02d-%02d/%02d:%02d:%02d",
	    1+$t[4], $t[3], $t[2], $t[1], $t[0]));
    }
    if ($log->getpart(q(pid))) {
	push(@text, "[$$]");
    }
    if ($log->getpart(q(prio)) &&
	(0 == $priolevel{$prio} || $priolevel{$prio} <= $priolevel{notice}))
    {
	push(@text, "[$prio]");
    }
    if ($log->getpart(q(text))) {
	my @stack = @$tmpstack;
	while (@stack) {
	    my $s = pop(@stack);
	    my $text = $s->{text};
	    if (ref($text)) {
		if (grep($text->isa($_), @{$log->getcensor})) {
		    next;
		}
		$text = $text->desc;
	    }
	    push(@text, $text) if length($text) > 0;
	}
    }
    if ($log->getpart(q(line)) &&
	(0 < $priolevel{$prio} && $priolevel{$prio} <= $priolevel{warn}))
    {
	push(@text, "at");
	my @stack = @$tmpstack;
	while (@stack) {
	    my $s = shift(@stack);
	    defined($s->{line}) or confess 'oops';
	    if ($text[-1] ne $s->{line}) {
		push(@text, $s->{line});
	    }
	}
    }
    $log->getio->write("@text\n");
    $log->delactive;
}

sub printall {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($prio) = @_;
    my $logstack = $log->getstack;
    if (! @$logstack) {
	$log->put("[missing log message]");
    }
    my @tmpstack = ();
    while (@$logstack) {
	push(@tmpstack, shift(@$logstack));
    }
    for my $key (sort keys %attached) {
	my $alog = $attached{$key};
	$alog->print($prio, \@tmpstack);
    }
    $instance->print($prio, \@tmpstack);
}

# public

sub push {
    my $log = shift;
    my(@args) = @_;
    my($pkg, $file, $line) = caller;
    $log->entry(0, $file, $line, @args);
    return $log;
}

sub put {
    my $log = shift;
    my(@args) = @_;
    my($pkg, $file, $line) = caller;
    $log->entry(1, $file, $line, @args);
    return $log;
}

sub fatal {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(fatal));
    exit(1);
}

sub error {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(error));
    return $log;
}

sub warn {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(warn));
    return $log;
}

sub notice {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(notice));
    return $log;
}

sub info {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(info));
    return $log;
}

sub debug {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(debug));
    return $log;
}

sub user {
    my $log = shift;
    @_ == 0 or confess 0+@_;
    $log->printall(q(user));
    return $log;
}

# return values from server to client

sub putvalue {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($value) = @_;
    my $d = Data::Dumper->new([$value], [qw($value)]);
    $d->Indent(0);
    $d->Useqq(1);
    my $dump = $d->Dump;
    $dump =~ /^\s*\$value\s*=\s*(.*);\s*$/ or confess $dump;
    $log->push("[value $1]");
}

sub hasvalue {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($line) = @_;
    return $line =~ /\[value\s+(.*)\]/;
}

sub getvalue {
    my $log = shift;
    @_ == 1 or confess 0+@_;
    my($line) = @_;
    $line =~ /\[value\s+(.*)\]/ or confess $line;
    my $expr = $1;
    my($value);
    eval "\$value = $expr";
    if ($@) {
	$log->put("$line: eval error: $@");
	return undef;
    }
    return [$value];
}

1;
# vim:set sw=4:
