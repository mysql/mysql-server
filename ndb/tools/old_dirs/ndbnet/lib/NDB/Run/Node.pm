package NDB::Run::Node;

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

NDB::Run::Node->attributes(
    env => sub { ref && $_->isa('NDB::Run::Env') },
    db => sub { ref && $_->isa('NDB::Run::Database') },
    dbname => sub { s/^\s+|\s+$//g; /^\S+$/ && ! m!/! },
    id => sub { s/^\s+|\s+$//g; s/^0+(\d+)$/$1/; /^\d+$/ && $_ > 0 },
    type => sub { s/^\s+|\s+$//g; /^(mgmt|db|api)$/ },
);

sub desc {
    my $node = shift;
    my $dbname = $node->getdb->getname;
    my $id = $node->getid;
    my $type = "?"; # $node->gettype;
    return "$dbname.$id-$type";
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $node = $class->SUPER::new(%attr);
    $node->setdb($attr{db})
	or $log->push, return undef;
    $node->setenv($node->getdb->getenv)
	or $log->push, return undef;
    $node->setdbname($node->getdb->getname)
	or $log->push, return undef;
    $node->setid($attr{id})
	or $log->push, return undef;
#   $node->settype($attr{type})
#	or $log->push, return undef;
    return $node;
}

# commands

sub start {
    my $node = shift;
    my $opts = shift;
    my $argv = [ 'startnode', $node->getdb->getname, $node->getid, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $node->getenv->docmd($cmd)
	or $log->push, return undef;
    return $ret;
}

sub stop {
    my $node = shift;
    my $opts = shift;
    my $argv = [ 'stopnode', $node->getdb->getname, $node->getid, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $node->getenv->docmd($cmd)
	or $log->push, return undef;
    return $ret;
}

sub kill {
    my $node = shift;
    my $opts = shift;
    my $argv = [ 'killnode', $node->getdb->getname, $node->getid, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $node->getenv->docmd($cmd)
	or $log->push, return undef;
    return $ret;
}

sub stat {
    my $node = shift;
    my $opts = shift;
    my $argv = [ 'statnode', $node->getdb->getname, $node->getid, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $node->getenv->docmd($cmd)
	or $log->push, return undef;
    return $ret;
}

sub write {
    my $node = shift;
    my $text = shift;
    my $opts = shift;
    my $argv = [ 'writenode', $node->getdb->getname, $node->getid, $text, $opts ];
    my $cmd = NDB::Net::Command->new(argv => $argv)
	or $log->push, return undef;
    my $ret = $node->getenv->docmd($cmd)
	or $log->push, return undef;
    ref($ret) eq 'HASH' && defined($ret->{output})
	or confess 'oops';
    return $ret;
}

1;
# vim:set sw=4:
