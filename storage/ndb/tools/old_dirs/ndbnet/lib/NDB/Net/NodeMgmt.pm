package NDB::Net::NodeMgmt;

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

NDB::Net::NodeMgmt->attributes(
    port => sub { s/^\s+|\s+$//g; /^\d+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $node = $class->SUPER::new(%attr, type => 'mgmt')
	or $log->push, return undef;
    $node->setport($attr{port})
	or $log->push, return undef;
    return 1;
}

# socket parser methods

sub socketcommand {
    my $node = shift;
    my $socket;
    $socket = NDB::Util::SocketINET->new or
	$log->push($node), return undef;
    $socket->settimeout(10);
    $socket->connect($node->getserver->getcanon, $node->getport) or
	$log->push($node), return undef;
    $socket->write("GET STATUS\r\nBYE\r\n") or
	$log->push($node), return undef;
    my $out = "";
    my $data;
    while ($data = $socket->read) {
	$out .= $data;
    }
    $socket->close;
    $out =~ s/\015//g;
    return $out;
}

sub get_status {
    my $node = shift;
    my $out = $node->socketcommand or
	$log->push, return undef;
    my @out = split(/\n/, $out);
    $out[0] =~ /^get\s+status\s+(\d+)/i or
	$log->put("bad line 0: $out[0]"), return undef;
    my $cnt = $1;
    my $ret = {};
    for (my $i = 1; $i <= $cnt; $i++) {
	$out[$i] =~ /^$i\s+(.*)/ or
	    $log->put("bad line $i: $out[$i]"), return undef;
	my $text = $1;
	$text =~ s/^\s+|\s+$//g;
	if ($text =~ /^ndb\s+(no_contact)\s+(\d+)$/i) {
	    $text = lc "$1";
	} elsif ($text =~ /^ndb\s+(starting)\s+(\d+)$/i) {
	    $text = lc "$1/$2";
	} elsif ($text =~ /^ndb\s+(started)\s+(\d+)$/i) {
	    $text = lc "$1";
	} elsif ($text =~ /^ndb\s+(shutting_down)\s+(\d+)$/i) {
	    $text = lc "$1";
	} elsif ($text =~ /^ndb\s+(restarting)\s+(\d+)$/i) {
	    $text = lc "$1";
	} elsif ($text =~ /^ndb\s+(unknown)\s+(\d+)$/i) {
	    $text = lc "$1";
	}
	$ret->{node}{$i} = $text;
    }
    return $ret;
}

# run methods

sub getautoinifile {
    my $node = shift;
    @_ == 0 or confess 0+@_;
    my $name = "config.txt";
    my $file = $node->getnodedir->getfile($name);
    return $file;
}

sub writeautoinifile {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    my $db = $node->getdb;
    my $nodelist = $db->getnodelist('all');
    my $computers = {};
    for my $n (@$nodelist) {
	$computers->{$n->getserver->getid} ||= {
	    id => $n->getserver->getid,
	    hostname => $n->getserver->getcanon,
	};
    }
    my $section = "";		# e.g. PROCESSES
    my $auto;
    my $edit = sub {
	chomp;
	s/^\s+|\s+$//g;
	if (/^(\w+)$/) {
	    $section = uc($1);
	}
	elsif (/^\@loop$/i) {
	    $_ = "#$_";
	    if ($auto) {
		$log->put("nested \@loop");
		return undef;
	    }
	    $auto = {};
	}
	elsif (/^\@base\s+(\S+)\s*$/) {
	    my $arg = $1;
	    $_ = "#$_";
	    if (! $auto) {
		$log->put("unexpected \@base");
		return undef;
	    }
	    if ($arg !~ /^\d+$/) {
		$log->put("non-numerical \@base");
		return undef;
	    }
	    $auto->{base} = $arg;
	}
	elsif (/^\@end$/i) {
	    $_ = "#$_";
	    if (! $auto) {
		$log->put("unmatched \@end");
		return undef;
	    }
	    if ($section eq 'COMPUTERS') {
		for my $id (sort { $a <=> $b } keys %$computers) {
		    my $computer = $computers->{$id};
		    $_ .= "\n";
		    $_ .= "\nId: " . $computer->{id};
		    $_ .= "\nHostName: " . $computer->{hostname};
		    if ($auto->{list}) {
			$_ .= "\n#defaults";
			for my $s (@{$auto->{list}}) {
			    $_ .= "\n$s";
			}
		    }
		}
	    }
	    elsif ($section eq 'PROCESSES') {
		for my $n (@$nodelist) {
		    if ($auto->{type} && $n->gettype ne lc($auto->{type})) {
			next;
		    }
		    $_ .= "\n";
		    $_ .= "\nType: " . uc($n->gettype);
		    $_ .= "\nId: " . $n->getid;
		    $_ .= "\nExecuteOnComputer: " . $n->getserver->getid;
		    if ($auto->{list}) {
			$_ .= "\n#defaults";
			for my $s (@{$auto->{list}}) {
			    $_ .= "\n$s";
			}
		    }
		}
	    }
	    elsif ($section eq 'CONNECTIONS') {
		if (! $auto->{type}) {
		    $log->put("cannot generate CONNECTIONS without type");
		    return undef;
		}
		if (! defined($auto->{base})) {
		    $log->put("need \@base for CONNECTIONS");
		    return undef;
		}
		my $key = $auto->{base};
		for (my $i1 = 0; $i1 <= $#$nodelist; $i1++) {
		    for (my $i2 = $i1+1; $i2 <= $#$nodelist; $i2++) {
			my $n1 = $nodelist->[$i1];
			my $n2 = $nodelist->[$i2];
			if ($n1->gettype ne 'db' && $n2->gettype ne 'db') {
			    next;
			}
			$_ .= "\n";
			$_ .= "\nType: $auto->{type}";
			$_ .= "\nProcessId1: " . $n1->getid;
			$_ .= "\nProcessId2: " . $n2->getid;
			$key++;
			if ($auto->{type} eq 'TCP') {
			    $_ .= "\nPortNumber: $key";
			    if (my $list = $opts->{proxy}) {
				my $id1 = $n1->getid;
				my $id2 = $n2->getid;
				if ($list =~ /\b$id1\b.*-.*\b$id2\b/) {
				    $key++;
				    $_ .= "\nProxy: $key";
				} elsif ($list =~ /\b$id2\b.*-.*\b$id1\b/) {
				    $key++;
				    $_ .= "\nProxy: $key";
				}
			    }
			}
			elsif ($auto->{type} eq 'SHM') {
			    $_ .= "\nShmKey: $key";
			}
			else {
			    $log->put("cannot handle CONNECTIONS type $auto->{type}");
			    return undef;
			}
			if ($auto->{list}) {
			    $_ .= "\n#defaults";
			    for my $s (@{$auto->{list}}) {
				$_ .= "\n$s";
			    }
			}
		    }
		}
	    }
	    else {
		$log->put("found \@end in unknown section '$section'");
		return undef;
	    }
	    undef $auto;
	}
	elsif (/^$/) {
	}
	elsif ($auto) {
	    if (/^Type:\s*(\w+)$/i) {
		$auto->{type} = uc($1);
	    }
	    else {
		$auto->{list} ||= [];
		push(@{$auto->{list}}, $_);
	    }
	    $_ = "";
	    return 1;	# no output
	}
	$_ .= "\n";
	return 1;
    };
    $node->getautoinifile->mkdir
	or $log->push, return undef;
    $node->getinifile->copyedit($node->getautoinifile, $edit)
	or $log->push, return undef;
    return 1;
}

sub handleprepare {
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($opts) = @_;
    my $envdefs = $node->getenvdefs($opts);
    defined($envdefs) or return undef;
    my $nodedir = $node->getnodedir;
    my $shellfile = $node->getshellfile;
    my $port = $node->getport;
    my $lpath = $node->getlocalcfg->getbasename;
    $node->writeautoinifile($opts)
	or $log->push, return undef;
    my $ipath = $node->getautoinifile->getbasename;
    $node->getbincfg->mkdir or $log->push, return undef;
    my $cpath = $node->getbincfg->getbasename;
    my $run;
    if ($^O ne 'MSWin32') {
	$run = "\$NDB_TOP/bin/mgmtsrvr";
    } else {
	$run = "mgmtsrvr";
    }
    my $statport = $port + 1;
    $run .= " -l $lpath -c $ipath";
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
    my $node = shift;
    @_ == 1 or confess 0+@_;
    my($cmd) = @_;
    $log->put("write: quit")->push($node)->user;
    $node->getiow->write("quit\n");
    $node->setstate('stop')
	or log->push($node), return undef;
    return 1;
}

1;
# vim:set sw=4:
