package NDB::Net::Command;

use strict;
use Carp;
use Getopt::Long;
use Text::ParseWords ();
use Text::Tabs ();

require NDB::Net::Base;

use vars qw(@ISA);
@ISA = qw(NDB::Net::Base);

# constructors

my $log;

sub initmodule {
    $log = NDB::Util::Log->instance;
}

my($cmdtab, $aliastab);

NDB::Net::Command->attributes(
    name => sub { /^\s*\w+\b/ },
    argv => sub { ref eq 'ARRAY' },
    optspec => sub { ref eq 'ARRAY' },
    argspec => sub { /^\d+$/ || ref eq 'CODE' },
    short => sub { defined && ! ref },
    help => sub { defined && ! ref },
    opts => sub { ref eq 'HASH' },
    args => sub { ref eq 'ARRAY' },
);

sub desc {
    my $cmd = shift;
    return "command " . $cmd->getname("?");
};

sub processname {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my $cmdargv = $cmd->getargv;
    my $name = shift(@$cmdargv);
    my %seen = ();
    while ((my $entry) = grep($name eq $_->{name}, @$aliastab)) {
	$seen{$name}++ && last;
	unshift(@$cmdargv, split(' ', $entry->{value}));
	$name = shift(@$cmdargv);
    }
    if ((my $entry) = grep($_->{name} eq $name, @$cmdtab)) {
	$cmd->setname($entry->{name})
	    or $log->push, return undef;
	$cmd->setoptspec($entry->{optspec})
	    or $log->push, return undef;
	$cmd->setargspec($entry->{argspec})
	    or $log->push, return undef;
    }
    else {
	$log->put("$name: undefined")->push($cmd);
	return undef;
    }
    return 1;
}

sub getopttype {
    my $cmd = shift;
    my($key) = @_;
    if (grep(/^$key$/, @{$cmd->getoptspec})) {
	return 1;
    }
    if (grep(/^$key=/, @{$cmd->getoptspec})) {
	return 2;
    }
    return undef;
}

sub processargv {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my $cmdargv = $cmd->getargv;
    my @newargv = ();
    while (@$cmdargv) {
	my $v = shift(@$cmdargv);
	if (! defined($v)) {
	    next;
	}
	if (ref($v) eq 'ARRAY') {
	    unshift(@$cmdargv, @$v);		# push back
	    next;
	}
	if (ref($v) eq 'HASH') {
	    for my $k (sort keys %$v) {
		if ($cmd->getopttype($k) == 1) {
		    push(@newargv, "--$k");
		    next;
		}
		if ($cmd->getopttype($k) == 2) {
		    push(@newargv, "--$k", $v->{$k});
		    next;
		}
		$log->put("$k: undefined option")->push($cmd);
		return undef;
	    }
	    next;
	}
	if (ref($v)) {
	    confess 'oops';
	}
	push(@newargv, $v);
    }
    push(@$cmdargv, @newargv);
    return 1;
}

sub processopts {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my $cmdargv = $cmd->getargv;
    local(@ARGV) = @$cmdargv;
    try: {
	local $SIG{__WARN__} = sub {
	    my $errstr = "@_";
	    while (chomp($errstr)) {}
	    $log->put($errstr)->push($cmd);
	};
	$cmd->setopts({})
	    or $log->push, return undef;
	Getopt::Long::Configure(qw(
	    default no_getopt_compat no_ignore_case
	));
	GetOptions($cmd->getopts, @{$cmd->getoptspec})
	    or return undef;
    }
    $cmd->setargs([ @ARGV ])
	or $log->push, return undef;
    return 1;
}

sub processargs {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my $cmdargs = $cmd->getargs;
    if ($cmd->getargspec =~ /^\d+$/) {
	if (@$cmdargs != $cmd->getargspec) {
	    $log->put("invalid arg count %d != %d",
		scalar(@$cmdargs), $cmd->getargspec)->push($cmd);
	    return undef;
	}
    }
    if (ref($cmd->getargspec) eq 'CODE') {
	local $_ = scalar(@$cmdargs);
	if (! &{$cmd->getargspec}()) {
	    $log->put("invalid arg count %d",
		scalar(@$cmdargs))->push($cmd);
	    return undef;
	}
    }
    return 1;
}

sub new {
    my $class = shift;
    @_ % 2 == 0 or confess 0+@_;
    my %attr = @_;
    my $cmd = $class->SUPER::new(%attr);
    my $cmdargv = [];
    $cmd->setargv($cmdargv)
	or $log->push, return undef;
    my $line = $attr{line};
    my $argv = $attr{argv};
    defined($line) != defined($argv)	# exactly one
	or confess 'oops';
    if (defined($line)) {
	! ref($line) or confess 'oops';
	push(@$cmdargv, Text::ParseWords::shellwords($line));
    }
    if (defined($argv)) {
	ref($argv) eq 'ARRAY' or confess 'oops';
	push(@$cmdargv, @$argv);
    }
    if (! @$cmdargv) {
	$log->put("empty command");
	return undef;
    }
    $cmd->processname
	or $log->push, return undef;
    $cmd->processargv
	or $log->push, return undef;
    $cmd->processopts
	or $log->push, return undef;
    $cmd->processargs
	or $log->push, return undef;
    return $cmd;
}

sub getline {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my @text = ($cmd->getname);
    for my $k (sort keys %{$cmd->getopts}) {
	if ($cmd->getopttype($k) == 1) {
	    push(@text, "--$k");
	    next;
	}
	if ($cmd->getopttype($k) == 2) {
	    push(@text, "--$k", quotemeta($cmd->getopts->{$k}));
	    next;
	}
	confess 'oops';
    }
    for my $s (@{$cmd->getargs}) {
	push(@text, quotemeta($s));
    }
    return "@text";
}

sub setopt {
    my $cmd = shift;
    my($key, $value) = @_;
    if ($cmd->getopttype($key) == 1) {
	@_ == 1 or confess 0+@_;
	$cmd->getopts->{$key} = 1;
    }
    elsif ($cmd->getopttype($key) == 2) {
	@_ == 2 or confess 0+@_;
	$cmd->getopts->{$key} = $value;
    }
    else {
	confess 'oops';
    }
}

sub getopt {
    my $cmd = shift;
    @_ == 1 or confess 0+@_;
    my($key) = @_;
    $cmd->getopttype($key) or confess 'oops';
    return $cmd->getopts->{$key};
}

sub setarg {
    my $cmd = shift;
    @_ == 2 or confess 0+@_;
    my($idx, $value) = @_;
    $cmd->getargs->[$idx] = $value;
}

sub getarg {
    my $cmd = shift;
    @_ == 1 or confess 0+@_;
    my($idx) = @_;
    return $cmd->getargs->[$idx];
}

sub getarglist {
    my $cmd = shift;
    @_ == 1 or confess 0+@_;
    my($idx) = @_;
    my @args = @{$cmd->getargs};
    @args = @args[$idx..$#args];
    return \@args;
}

sub helptext {
    my $cmd = shift;
    @_ <= 1 or confess 0+@_;
    my $name = $cmd->getargs->[0];
    my $text = "";
    my $indent = " "x4;
    if (defined($name)) {
	for my $entry (@$aliastab) {
	    if ($entry->{name} eq $name) {
		$text .= "alias $name=\"$entry->{value}\"\n";
		($name) = split(' ', $entry->{value});
		last;
	    }
	}
    }
    else {
	$text .= "COMMANDS\n";
    }
    for my $entry (@$cmdtab) {
	if (defined($name)) {
	    if ($entry->{name} eq $name) {
		$text .= uc($name) . "\n";
		for my $t (split(/\n/, $entry->{help})) {
		    $text .= $indent;
		    $text .= Text::Tabs::expand($t) . "\n";
		}
		last;
	    }
	}
	else {
	    $text .= $indent;
	    $text .= sprintf("%-16s%s\n", $entry->{name}, $entry->{short});
	}
    }
    if (! $text) {
	$log->put("$name: undefined");
	return undef;
    }
    return $text;
}

sub aliastext {
    my $cmd = shift;
    @_ == 0 or confess 0+@_;
    my $text = "";
    my $indent = " "x4;
    $text .= "ALIASES\n";
    for my $entry (@$aliastab) {
	$text .= $indent;
	$text .= sprintf("%-16s%s\n", $entry->{name}, $entry->{value});
    }
    return $text;
}

# commands
#	name	command name (unique)
#	optspec	option spec in Getopt::Long style
#	argspec	arg count (number or sub)
#	short	one line summary
#	help	long help text
#	opts	options HASH (after parse)
#	args	arguments ARRAY (after parse)

$cmdtab = [
    {
	name => "help",
	optspec => [ qw() ],
	argspec => sub { $_[0] <= 1 },
	short => "print help (try: h h)",
	help => <<END,
help [name]
name		command name or alias

Print help summary or longer help text for one command.

General:

Options can be placed anywhere on command line and can be abbreviated.
Example: "start db11 -i" instead of "start --init_rm db11".

Several commands have internal option --local which makes current server
do the work, instead of passing it to other servers.  This option should
not be used explicitly, except for testing.
END
    },
    {
	name => "alias",
	optspec => [ qw() ],
	argspec => 0,
	short => "list aliases",
	help => <<END,
alias

List built-in aliases.  New ones cannot be defined (yet).
END
    },
    {
	name => "quit",
	optspec => [ qw() ],
	argspec => 0,
	short => "exit ndbnet",
	help => <<END,
quit

Exit ndbnet client.
END
    },
    {
	name => "server",
	optspec => [ qw(all direct pass parallel script=s local) ],
	argspec => sub { $_ >= 1 },
	short => "net server commands",
	help => <<END,
server action id... [options]
action		start restart stop ping
id		net server id from net config
--all		do all servers listed in net config
--direct	do not use a server
--pass		pass current ndb environment to remote command
--parallel	run in parallel when possible
--script path	remote script instead of "ndbnetd"
--local		for internal use by servers

Each host needs one net server (ndbnetd).   It should be started
from latest ndb installation, for example at system boot time.
A "server ping" is used to check that all servers are up (option
--all is added if no server ids are given).

Other actions are mainly for testing.  A "server start" tries to
start servers via "ssh".  This does not work if "ssh" is not allowed
or if the remote command does not get right environment.

Option --direct makes this ndbnet client do the work.  It is assumed
for "server start" and it requires that a local net config exists.
Option --pass is useful in a homogeneous (NFS) environment.

There are aliases "startserver" for "server start", etc.
END
    },
    {
	name => "start",
	optspec => [ qw(init_rm nostart stop kill config old home=s clean proxy=s) ],
	argspec => 1,
	short => "start database",
	help => <<END,
start dbname [options]
dbname		database name
--init_rm	destroy existing database files on each node
--nostart	for DB nodes only do "ndb -n"
--stop		do "stop dbname" first
--kill		do "kill dbname" first
--config	create run config but start no processes
--old		use existing config files
--home dir	override home (product dir) from config
--clean		passed to startnode
--proxy list	generate proxy ports (read the source)

Start a database as follows:

- start mgmt servers on all mgmt nodes
- start ndb processes on all db nodes
- send "all start" to first mgmt server (redundant)
- start processes on all api nodes (if runtype!="manual")

Older database versions (v1.0) are started similarly except that there
are no management servers.

The --proxy option is used for testing network problems.
END
    },
    {
	name => "startnode",
	optspec => [ qw(init_rm nostart config old run=s home=s local clean proxy=s) ],
	argspec => 2,
	short => "start database node",
	help => <<END,
startnode dbname nodeid [options]
dbname		database name
nodeid		node number
--init_rm	destroy existing database files (if db node)
--nostart	if DB node only do "ndb -n"
--config	create run config but start no processes
--old		use existing config files
--run cmd	run this shell command, default from config file
--home dir	override home (product dir) from config
--local 	node must be local to this ndbnet server
--clean		remove old node dir first
--proxy list	processed by mgmt nodes, see "start" command

Start the process on one database node.  The node can be of any type
(mgmt/db/api).  If already running, does nothing.

The --run option specifies a simple shell command (not pipeline etc).
Defaults:

- mgmt node => mgmtsrvr -p port -l Ndb.cfg -i config.txt -c config.bin
  where port comes from ndbnet.xml
- db node => ndb
- api node => based on ndbnet config, default empty

The node server exits when the command exits (unless runtype is set to
auto).  Command exit status is not available.

Used internally by db "start" command.
END
    },
    {
	name => "stop",
	optspec => [ qw() ],
	argspec => 1,
	short => "stop database",
	help => <<END,
stop dbname [options]
dbname		database name

Stop a database as follows (see also "stopnode" command):

- send SIGTERM to api processes, wait for them to exit
- send "all stop" command to first mgmt server
- wait for db processes to exit
- send "quit" to mgmt servers, wait for them to exit
END
    },
    {
	name => "stopnode",
	optspec => [ qw(local) ],
	argspec => 2,
	short => "stop process on one node",
	help => <<END,
stopnode dbname nodeid [options]
dbname		database name
nodeid		node number
--local 	node must be local to this server

Stop process on one database node.  Action depends on node type:

- api node: send SIGTERM to the process, wait for it to exit
- db node: no action, wait for the ndb process to exit
- mgmt node: send "quit" command to mgmt server, wait for it to exit

Used internally by db "stop" command.
END
    },
    {
	name => "kill",
	optspec => [ qw() ],
	argspec => 1,
	short => "kill processes on all nodes",
	help => <<END,
kill dbname [options]
dbname		database name

Send SIGKILL to processes on all nodes and wait for them to exit.
END
    },
    {
	name => "killnode",
	optspec => [ qw(local) ],
	argspec => 2,
	short => "kill process on one node",
	help => <<END,
killnode dbname nodeid [options]
dbname		database name
nodeid		node number
--local 	node must be local to this server

Send SIGKILL to the process on the node and wait for it to exit.

Used internally by db "kill" command.
END
    },
    {
	name => "statnode",
	optspec => [ qw(local) ],
	argspec => 2,
	short => "get node run status (internal)",
	help => <<END,
statnode dbname nodeid [options]
dbname		database name
nodeid		node number
--local		node must be local to this server

Get node run status (up/down) as a process.  Used internally
and may not produce any output in ndbnet command.
END
    },
    {
	name => "list",
	optspec => [ qw(quick short) ],
	argspec => sub { 1 },
	short => "list databases",
	help => <<END,
list [dbname] [options]
dbname		database name, default is to list all
--quick		only output config, do not query status
--short		do list nodes

List databases and nodes.  Internally returns a data structure
of process and mgmt server status values for each node.  Externally
(in ndbnet command) this is formatted as a listing.
END
    },
    {
	name => "writenode",
	optspec => [ qw(wait=i local) ],
	argspec => 3,
	short => "write line of text to the process on a node",
	help => <<END,
writenode dbname nodeid "some text"
dbname		database name
nodeid		node number
"some text"	arbitrary text (quote if spaces)
--wait n	wait n seconds for any response
--local 	node must be local to this server

Write the text and a newline to the standard input of the process
running on the node.  If wait > 0 is specified, prints whatever
the process wrote to stdout/stderr during that time.

Used internally by "start" and other commands.
END
    },
];

# aliases
#	name	alias
#	value	expansion

$aliastab = [
    {
	name => "h",
	value => "help",
    },
    {
	name => "q",
	value => "quit",
    },
    {
	name => "EOF",
	value => "quit",
    },
    {
	name => "startserver",
	value => "server start",
    },
    {
	name => "ss",
	value => "server start",
    },
    {
	name => "restartserver",
	value => "server restart",
    },
    {
	name => "rss",
	value => "server restart",
    },
    {
	name => "stopserver",
	value => "server stop",
    },
    {
	name => "pingserver",
	value => "server ping",
    },
    {
	name => "ps",
	value => "server ping",
    },
    {
	name => "l",
	value => "list",
    },
];

1;
# vim:set sw=4:
