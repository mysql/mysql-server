package NDB::Net::NodeDb;

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

NDB::Net::NodeDb->attributes(
    fsdir => sub { s/^\s+|\s+$//g; /^\S+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $node = $class->SUPER::new(%attr, type => 'db')
	or $log->push, return undef;
    $node->setfsdir($attr{fsdir})
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
    my $fsdir = NDB::Util::Dir->new(
	path => sprintf("%s/%s/%s-%s.fs",
	    $node->getfsdir, $node->getdb->getname, $node->getid, $node->gettype));
    $fsdir->mkdir or $log->push, return undef;
    my $init_rm;
    my $run;
    if ($^O ne 'MSWin32') {
	$init_rm = "# no -i";
	if ($opts->{init_rm}) {
	    $init_rm = 'rm -f $NDB_FILESYSTEM/*/DBDIH/P0.sysfile';
	}
	$run = "\$NDB_TOP/bin/ndb";
    } else {
	$init_rm = "rem no -i";
	if ($opts->{init_rm}) {
	    $init_rm =
	    	'del/f %NDB_FILESYSTEM%\D1\DBDIH\P0.sysfile' . "\n" .
		'del/f %NDB_FILESYSTEM%\D2\DBDIH\P0.sysfile';
	}
	$run = "ndb";
    }
    if ($node->getdb->cmpversion("1.0") <= 0) {
	$run .= " -s";
    }
    if ($opts->{nostart}) {
	$run .= " -n";
    }
    if ($node->hasrun) {
	$run = $node->getrun;
    }
    if (defined($opts->{run})) {
	$run = $opts->{run};
    }
    $log->put("run: $run")->push($node)->user;
    if ($^O ne 'MSWin32') {
	$shellfile->puttext(<<END) or $log->push, return undef;
$envdefs
NDB_FILESYSTEM=@{[ $fsdir->getpath ]}
export NDB_FILESYSTEM
# v1.0 compat
UAS_FILESYSTEM=\$NDB_FILESYSTEM
export UAS_FILESYSTEM
mkdir -p \$NDB_FILESYSTEM
$init_rm
cd @{[ $nodedir->getpath ]} || exit 1
exec \$debugger $run
END
    } else {
	$shellfile->puttext(<<END) or $log->push, return undef;
$envdefs
set NDB_FILESYSTEM=@{[ $fsdir->getpath ]}
rem v1.0 compat
set UAS_FILESYSTEM=%NDB_FILESYSTEM%
mkdir %NDB_FILESYSTEM%
$init_rm
cd @{[ $nodedir->getpath ]}
call $run
END
    }
    return 1;
}

sub cmd_stopnode_fg {
    my($node, $cmd) = @_;
    $node->setstate('stop')
	or log->push($node), return undef;
    return 1;
}

1;
# vim:set sw=4:
