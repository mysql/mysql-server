package NDB::Run::Env;

use strict;
use Carp;

require NDB::Run::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Run::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Run::Env->attributes(
    server => sub { ref && $_->isa('NDB::Net::Server') },
);

sub desc {
    "env";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $env = $class->SUPER::new(%attr);
    return $env;
}

sub getdb {
    my $env = shift;
    @_ == 1 or croak q(usage: $db = $env->getdb($name));
    my($name) = @_;
    my $db = NDB::Run::Database->new(env => $env, name => $name)
	or $log->push, return undef;
    return $db;
}

# commands

sub init {
    my $env = shift;
    my $netenv = NDB::Net::Env->instance;
    my $netcfg = NDB::Net::Config->new(file => $netenv->getnetcfg)
	or $log->push, return undef;
    $netcfg->load
	or $log->push, return undef;
    my $servers = $netcfg->getservers
	or $log->push, return undef;
    my $server;
    for my $s (@$servers) {
	if (! $s->testconnect) {
	    $log->push->warn;
	    next;
	}
	$server = $s;
	last;
    }
    if (! $server) {
	$log->put("no available server")->push($netcfg);
	return undef;
    }
    $env->setserver($server)
	or $log->push, return undef;
    $log->put("selected")->push($server)->info;
    return 1;
}

sub docmd {
    my $env = shift;
    my $cmd = shift;
    my $ret = $env->getserver->request($cmd);
    defined($ret)
	or $log->push, return undef;
    return $ret;
}

1;
# vim:set sw=4:
