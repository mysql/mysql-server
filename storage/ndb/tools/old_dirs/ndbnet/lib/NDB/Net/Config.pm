package NDB::Net::Config;

use strict;
use Carp;
use Symbol;
use Socket;
use Errno;
use XML::Parser;

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

NDB::Net::Config->attributes(
    file => sub { /^\S+$/ },
    loadtime => sub { /^\d+$/ },
);

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my(%attr) = @_;
    my $netcfg = $class->SUPER::new(%attr);
    $netcfg->setfile($attr{file})
	or $log->put, return undef;
    return $netcfg;
}

sub desc {
    my $netcfg = shift;
    return $netcfg->getfile;
}

use vars qw(@context);

sub handle_start {
    my($parser, $tag, %attr) = @_;
    my $p = $context[-1];
    my $q = {};
    $p->{$tag} ||= [];
    push(@{$p->{$tag}}, $q);
    for my $k (keys %attr) {
	$q->{$k} = $attr{$k};
    }
    push(@context, $q);
    return 1;
}

sub handle_end {
    my($parser, $tag, %attr) = @_;
    pop(@context);
    return 1;
}

sub load {
    my $netcfg = shift;
    my $file = $netcfg->getfile;
    my @s;
    while (1) {
	if (@s = stat($file)) {
	    last;
	}
	$log->put("$file: stat failed: $!");
	if (! $!{ESTALE}) {
	    return undef;
	}
	$log->put("(retry)")->info;
	sleep 1;
    }
    if ($s[9] <= $netcfg->getloadtime(0)) {
	return 1;
    }
    my $fh = gensym();
    if (! open($fh, "<$file")) {
	$log->put("$file: open for read failed: $!");
	return undef;
    }
    my $text = "";
    my $line;
    while (defined($line = <$fh>)) {
	$text .= $line;
    }
    close($fh);
    my $parser = XML::Parser->new(
	ParseParamEnt => 1,
    	Handlers => {
	    Start => \&handle_start,
	    End => \&handle_end,
	},
    );
    delete $netcfg->{config};
    local @context = ($netcfg);
    $parser->parse($text);
    $netcfg->{text} = $text;
    $netcfg->{config} = $netcfg->{config}[0];
    $netcfg->setloadtime(time)
	or $log->push, return undef;
    NDB::Net::Server->deleteall;
    NDB::Net::Database->deleteall;
    NDB::Net::Node->deleteall;
    return 1;
}

sub getservers {
    my $netcfg = shift;
    @_ == 0 or confess 0+@_;
    my $servers = [];
    my $slist = $netcfg->{config}{server} || [];
    for my $s (@$slist) {
	my $server;
	$server = NDB::Net::ServerINET->get($s->{id});
	if (! $server) {
	    $server = NDB::Net::ServerINET->new(%$s);
	    if (! $server) {
		$log->push($netcfg)->warn;
		next;
	    }
	}
	push(@$servers, $server);
    }
    return $servers;
}

sub getdatabases {
    my $netcfg = shift;
    @_ == 0 or confess 0+@_;
    my $databases = [];
    my $dlist = $netcfg->{config}{database} || [];
    for my $d (@$dlist) {
	if ($d->{isproto} eq "y") {
	    next;
	}
	if ($d->{name} !~ /^\w(\w|-)*$/) {
	    $log->put("$d->{name}: invalid db name")->push($netcfg)->warn;
	    next;
	}
	my $db = $netcfg->getdatabase($d->{name});
	if (! $db) {
	    $log->push->warn;
	    next;
	}
	push(@$databases, $db);
    }
    return $databases;
}

sub getdatabase {
    my $netcfg = shift;
    @_ == 1 or confess 0+@_;
    my($name) = @_;
    $netcfg->getservers or return undef;	# cache them
    my $default = $netcfg->{config}{default}[0] || {};
    my $db;
    my $dlist = $netcfg->{config}{database} || [];
    my $nlist;
    for my $d (@$dlist) {
	($d->{name} ne $name) && next;
	if ($d->{isproto} eq "y") {
	    next;
	}
	my %attr = (%$default, %$d);
	$db = NDB::Net::Database->new(%attr);
	if (! $db) {
	    $log->push($netcfg);
	    return undef;
	}
	if ($d->{proto}) {
	    if ($d->{isproto} eq "y") {
		$log->put("$name: prototypes cannot be recursive");
		return undef;
	    }
	    for my $d2 (@$dlist) {
		($d2->{name} ne $d->{proto}) && next;
		if ($d2->{isproto} ne "y") {
		    $log->put("$name: $d2->{name} is not a prototype");
		    return undef;
		}
		if (! $d->{node}) {
		    $d->{node} = $d2->{node};
		}
		last;
	    }
	}
	$nlist = $d->{node} || [];
	last;
    }
    if (! $db) {
	$log->put("$name: no such db")->push($netcfg);
	return undef;
    }
    if (! @$nlist) {
	$log->put("$name: empty node list")->push($netcfg);
	return undef;
    }
    for my $n (@$nlist) {
	my $node;
	try: {
	    my $server = NDB::Net::Server->get($n->{server})
		or last try;
	    my %attr = (%$n, db => $db, server => $server);
	    my $type = $attr{type};
	    if ($type eq 'db') {
		$node = NDB::Net::NodeDb->new(%attr)
		    or last try;
	    }
	    if ($type eq 'mgmt') {
		$node = NDB::Net::NodeMgmt->new(%attr)
		    or last try;
	    }
	    if ($type eq 'api') {
		$node = NDB::Net::NodeApi->new(%attr)
		    or last try;
	    }
	    $log->put("bad node type '$type'");
	}
	if (! $node) {
	    $log->push($netcfg);
	    $db->delete;
	    return undef;
	}
    }
    return $db;
}

1;
# vim:set sw=4:
