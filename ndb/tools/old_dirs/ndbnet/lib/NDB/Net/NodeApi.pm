package NDB::Net::NodeApi;

use strict;
use Carp;
use Symbol;

require NDB::Net::Node;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Node);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Net::NodeApi->attributes();

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $node = $class->SUPER::new(%attr, type => 'api')
	or $log->push, return undef;
    return 1;
}

# run methods

sub handleprepare {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    my $netenv = NDB::Net::Env->instance;
    my $envdefs = $node->getenvdefs($opts);
    defined($envdefs) or return undef;
    my $nodedir = $node->getnodedir;
    my $shellfile = $node->getshellfile;
    my $run;
    if ($node->hasrun) {
	$run = $node->getrun;
    }
    if (defined($opts->{run})) {
	$run = $opts->{run};
    }
    if (defined($run)) {
	$log->put("run: $run")->push($node)->user;
    }
    if ($^O ne 'MSWin32') {
	$shellfile->puttext(<<END) or $log->push, return undef;
$envdefs
cd @{[ $nodedir->getpath ]} || exit 1
set -x
exec \$DEBUGGER $run
END
    } else {
	$shellfile->puttext(<<END) or $log->push, return undef;
$envdefs
cd @{[ $nodedir->getpath ]}
call $run
END
    }
    return 1;
}

sub cmd_stopnode_fg {
    my($node, $cmd) = @_;
    my $pid = $node->getpid;
    unless ($pid > 1) {
	$log->put("bad pid=$pid")->push($node);
	return undef;
    }
    $log->put("kill -15 $pid")->push($node)->user;
    kill(15, $pid);
    $node->setstate('stop')
	or log->push($node), return undef;
    return 1;
}

1;
# vim:set sw=4:
