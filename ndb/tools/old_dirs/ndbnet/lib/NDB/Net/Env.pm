package NDB::Net::Env;

use strict;
use File::Spec;
use Carp;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# environment variables
#
# NDB_TOP	source dir or installation dir
# NDB_BASE	base dir not tied to any release or database
# NDB_NETCFG	ndbnet config file, default $NDB_BASE/etc/ndbnet.xml
#
# ndbnet explicitly unsets NDB_TOP and NDB_HOME because they are
# specific to each database or database node

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Net::Env->attributes(
    base => sub { /^\S+$/ },
    netcfg => sub { /^\S+$/ },
    hostname => sub { /^\S+$/ },
);

my $instance;

sub desc {
    my $netenv = shift;
    return "net environment";;
}

sub instance {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    if ($instance) {
	return $instance;
    }
    for my $var (qw(NDB_TOP NDB_HOME)) {
	my $top = delete $ENV{$var};
	if (defined($top)) {
	    if ($^O ne 'MSWin32') {
		$ENV{PATH} =~ s!(^|:)$top/bin($|:)!$1$2!g;
		$ENV{LD_LIBRARY_PATH} =~ s!(^|:)$top/lib($|:)!$1$2!g;
		$ENV{PERL5LIB} =~ s!(^|:)$top/lib/perl5($|:)!$1$2!g;
	    }
	}
    }
    my $netenv = $class->SUPER::new(%attr);
    for my $base ($attr{base}, $ENV{NDB_BASE}) {
	if (defined($base)) {
	    $netenv->setbase($base)
		or $log->push, return undef;
	}
    }
    for my $netcfg ($attr{netcfg}, $ENV{NDB_NETCFG}) {
	if (defined($netcfg)) {
	    $netenv->setnetcfg($netcfg)
		or $log->push, return undef;
	}
    }
    if ($netenv->hasbase && ! $netenv->hasnetcfg) {
	$netenv->setnetcfg(File::Spec->catfile($netenv->getbase, "etc", "ndbnet.xml"))
	    or $log->push, return undef;
    }
    my $uname;
    if ($^O ne 'MSWin32') {
	chomp($uname = `uname -n`);
    } else {
	chomp($uname = `hostname`);
    }
    my($hostname) = gethostbyname($uname);
    if (! defined($hostname)) {
	$uname =~ s!\..*$!!;
	($hostname) = gethostbyname($uname);
    }
    $netenv->sethostname($hostname)
	or $log->push, return undef;
    $instance = $netenv;
    return $instance;
}

1;
# vim:set sw=4:
