#!/usr/bin/perl
# Copyright (C) 2000-2002, 2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# This is a utility for MySQL. It is not needed by any standard part
# of MySQL.

# Usage: mysql_zap [-signal] [-f] [-t] pattern

# Configuration parameters.

$sig = "";			# Default to try all signals
$ans = "y";
$opt_f= 0;
$opt_t= 0;
$opt_a = "";

$BSD = -f '/vmunix' || $ENV{"OS"} eq "SunOS4";
$LINUX = $^O eq 'linux' || $^O eq 'darwin';
$pscmd = $BSD ? "/bin/ps -auxww" : $LINUX ? "/bin/ps axuw" : "/bin/ps -ef";

open(TTYIN, "</dev/tty") || die "can't read /dev/tty: $!";
open(TTYOUT, ">/dev/tty") || die "can't write /dev/tty: $!";
select(TTYOUT);
$| = 1;
select(STDOUT);
$SIG{'INT'} = 'cleanup';

while ($#ARGV >= $[ && $ARGV[0] =~ /^-/) {
    if ($ARGV[0] =~ /(ZERO|HUP|INT|QUIT|ILL|TRAP|ABRT|EMT|FPE|KILL|BUS|SEGV|SYS|PIPE|ALRM|TERM|URG|STOP|TSTP|CONT|CLD|TTIN|TTOU|IO|XCPU|XFSZ|VTALRM|PROF|WINCH|LOST|USR1|USR2)/ || $ARGV[0] =~ /-(\d+)$/) {
	$sig = $1;
    } elsif ($ARGV[0] eq "-f") {
	$opt_f=1;
    } elsif ($ARGV[0] eq "-t") {
	$opt_t=1;
	$ans = "n";
    }
    elsif ($ARGV[0] eq "-a")
    {
	$opt_a = 1;
    }
    elsif ($ARGV[0] eq "-?" || $ARGV[0] eq "-I" || $ARGV[0] eq "--help")
    {
	&usage;
    }
    else {
	print STDERR "$0: illegal argument $ARGV[0] ignored\n";
    }
    shift;
}

&usage if $#ARGV < 0;

if (!$opt_f)
{
    if ($BSD) {
	system "stty cbreak </dev/tty >/dev/tty 2>&1";
    }
    else {
	system "stty", 'cbreak',
	system "stty", 'eol', '^A';
    }
}

open(PS, "$pscmd|") || die "can't run $pscmd: $!";
$title = <PS>;
print TTYOUT $title;

# Catch any errors with eval.  A bad pattern, for instance.
eval <<'EOF';
process: while ($cand = <PS>)
{
    chop($cand);
    ($user, $pid) = split(' ', $cand);
    next if $pid == $$;
    $found = !@ARGV;
    if ($opt_a) { $found = 1; }
    foreach $pat (@ARGV)
    {
	if ($opt_a)
	{
	    if (! ($cand =~ $pat))
	    {
		next process;
	    }
	}
	else
	{
	    $found = 1 if $cand =~ $pat;
	}
    }
    next if (!$found);
    if (! $opt_f && ! $opt_t)
    {
	print TTYOUT "$cand? ";
	read(TTYIN, $ans, 1);
	print TTYOUT "\n" if ($ans ne "\n");
    }
    else
    {
	print TTYOUT "$cand\n";
    }
    if ($ans =~ /^y/i) { &killpid($sig, $pid); }
    if ($ans =~ /^q/i) { last; }
}
EOF

&cleanup;


sub usage {
    print <<EOF;
Usage:   $0 [-signal] [-?Ift] [--help] pattern
Options: -I or -? "info"  -f "force" -t "test".

Version 1.0
Kill processes that match the pattern.
If -f isn't given, ask user for confirmation for each process to kill.
If signal isn't given, try first with signal 15, then with signal 9.
If -t is given, the processes are only shown on stdout.
EOF
    exit(1);
}

sub cleanup {
    if ($BSD) {
	system "stty -cbreak </dev/tty >/dev/tty 2>&1";
    }
    else {
	system "stty", 'icanon';
	system "stty", 'eol', '^@';
    }
    print "\n";
    exit;
}

sub killpid {
    local($signal,$pid) = @_;
    if ($signal)
    {
	kill $signal,$pid;
    }
    else
    {
	print "kill -15\n";
	kill 15, $pid;
	for (1..5) {
	    sleep 2;
	    return if kill(0, $pid) == 0;
	}
	print "kill -9\n";
	kill 9, $pid;
	for (1..5) {
	    sleep 2;
	    return if kill(0, $pid) == 0;
	}
	print "$pid will not die!\n";
    }
}
