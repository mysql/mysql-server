#@perl@
# mysqldumpslow - parse and summarize the MySQL slow query log

use strict;
use Getopt::Long;

# t=time, l=lock time, r=rows
# at, al, and ar are the corresponding averages

my %opt = (
	s => 'at',
	h => '*',
);

GetOptions(\%opt,
	'v+',		# verbose
	'd+',		# debug
	's=s',		# what to sort by (t, at, l, al, r, ar etc)
	'a!',		# don't abstract all numbers to N and strings to 'S'
	'g=s',		# grep: only consider stmts that include this string
	'h=s',		# hostname of db server (can be wildcard)
) or die "Bad option";

my %stmt;

my $datadir = "/var/lib/mysql";	# XXX should fetch dynamically
@ARGV = <$datadir/$opt{h}-slow.log>;

$/ = "\n#";		# read entire statements using paragraph mode
while (<>) {
	print "[$_]\n" if $opt{v};
	s/^#// unless %stmt;

	s/\s*Time: (\d+)  Lock_time: (\d+)  Rows_sent: (\d+).*\n//;
	my ($t, $l, $r) = ($1, $2, $3);

	s/^use \w+;\n//;	# not consistently added
	s/^SET timestamp=\d+;\n//;

	s/^[ 	]*\n//mg;	# delete blank lines
	s/^[ 	]*/  /mg;	# normalize leading whitespace
	s/\s*;\s*(#\s*)?$//;	# remove traing semicolon(+newline-hash)

	next if $opt{g} and !m/$opt{g}/i;

	unless ($opt{a}) {
		s/\b\d+\b/N/g;
		s/\b0x[0-9A-Fa-f]+\b/N/g;
		s/'.*?'/'S'/g;
		s/".*?"/"S"/g;
	}

	$stmt{$_}->{c} += 1;
	$stmt{$_}->{t} += $t;
	$stmt{$_}->{l} += $l;
	$stmt{$_}->{r} += $r;

	warn "[$_]" if $opt{d};
}

foreach (keys %stmt) {
	my $v = $stmt{$_} || die;
	my ($c, $t, $l, $r) = @{ $v }{qw(c t l r)};
	$v->{at} = $t / $c;
	$v->{al} = $l / $c;
	$v->{ar} = $r / $c;
}

my @sorted = sort { $stmt{$a}->{$opt{s}} <=> $stmt{$b}->{$opt{s}} } keys %stmt;

foreach (@sorted) {
	my $v = $stmt{$_} || die;
	my ($c, $t,$at, $l,$al, $r,$ar) = @{ $v }{qw(c t at l al r ar)};
	printf "Count: %d  Time: %.2f (%d)  Lock_time: %.2f (%d)   Rows_sent: %.1f (%d) \n%s\n\n",
			$c, $at,$t, $al,$l, $ar,$r, $_;
}
