package NDB::Run::Database;

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

NDB::Run::Database->attributes(
    name => sub { s/^\s+|\s+$//g; /^\S+$/ && ! m!/! },
    env => sub { ref && $_->isa('NDB::Run::Env') },
);

sub desc {
    my $db = shift;
    return $db->getname;
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $db = $class->SUPER::new(%attr);
    $db->setname($attr{name})
	or $log->push, return undef;
    $db->setenv($attr{env})
	or $log->push, return undef;
    return $db;
}

sub getnode {
    my $db = shift;
    @_ == 1 or croak q(usage: $node = $db->getnode($id));
    my($id) = @_;
    my $node = NDB::Run::Node->new(db => $db, id => $id)
	or $log->push, return undef;
    return $node;
}

# commands

sub start {
    my $db = shift;
    my $opts = shift;
    my $argv = [ 'start', $db->getname, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $db->getenv->docmd($cmd);
    defined($ret)
	or $log->push, return undef;
    return $ret;
}

sub stop {
    my $db = shift;
    my $opts = shift;
    my $argv = [ 'stop', $db->getname, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $db->getenv->docmd($cmd);
    defined($ret)
	or $log->push, return undef;
    return $ret;
}

sub kill {
    my $db = shift;
    my $opts = shift;
    my $argv = [ 'kill', $db->getname, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $db->getenv->docmd($cmd);
    defined($ret)
	or $log->push, return undef;
    return $ret;
}

1;
# vim:set sw=4:
