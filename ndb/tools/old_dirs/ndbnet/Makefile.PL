# -*- perl -*-

use strict;
use Config;
use ExtUtils::MakeMaker qw(WriteMakefile);
use Test::Harness;

require 5.005;

my $base;
if ($base ||= $ENV{NDB_BASE}) {
	warn "Using NDB_BASE=$base\n";
}
$base or die "FATAL: need env.variable NDB_BASE\n";

my $top;
if ($top ||= $ENV{NDB_TOP}) {
	warn "Using NDB_TOP=$top\n";
}
$top or die "FATAL: need env.variable NDB_TOP\n";

my @scripts = qw(ndbnet.pl ndbnetd.pl);

for my $f (@scripts) {
	my $p = $f;
	$p =~ s/\.pl$//;
	unlink("$p.sh");
	open(G, ">$p.sh") or die "$p.sh: $!";
	if ($Config{osname} ne 'MSWin32') {
		print G <<END;
#! /bin/sh

# installed in \$NDB_BASE
# selects which $p to run (normally from latest release)
# created in source directory by "make install-base"

NDB_BASE=$base
export NDB_BASE

PATH=\$NDB_BASE/bin:\$PATH
export PATH

LD_LIBRARY_PATH=\$NDB_BASE/lib:\$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

PERL5LIB=\$NDB_BASE/lib/perl5:\$PERL5LIB
export PERL5LIB

NDB_TOP=$top
export NDB_TOP

PATH=\$NDB_TOP/bin:\$PATH
export PATH

LD_LIBRARY_PATH=\$NDB_TOP/lib:\$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

PERL5LIB=\$NDB_TOP/lib/perl5:\$PERL5LIB
export PERL5LIB

exec perl \$NDB_TOP/lib/perl5/$p.pl "\$@"
END
	} else {
		print G <<END;
rem  installed in \$NDB_BASE
rem  selects which $p to run (normally from latest release)
rem  created in source directory by "make install-base"

set NDB_BASE=$base
set PATH=%NDB_BASE%\\bin;%PATH%
set PERL5LIB=%NDB_BASE%\\lib\\perl5;%PERL5LIB%
set NDB_TOP=$top
set PATH=%NDB_TOP%\\bin;%PATH%
set PERL5LIB=%NDB_TOP%\\lib\\perl5;%PERL5LIB%
perl %NDB_TOP%\\lib\\perl5\\$p.pl %1 %2 %3 %4 %5 %6 %7 %8 %9
END
	}
	close G;
}

unshift(@INC, 'lib');
$main::onlymodules = 1;
require lib::NDB::Util;
require lib::NDB::Net;
require lib::NDB::Run;

my @modules = (
	q(NDB::Util),
	@NDB::Util::modules,
	q(NDB::Net),
	@NDB::Net::modules,
	q(NDB::Run),
	@NDB::Run::modules,
);

my @modulepaths = map { s!::!/!g; s!$!.pm!; $_ } @modules;

my %pm = ();
for my $pl (@scripts) {
	$pm{"$pl"} = "\$(INST_LIBDIR)/$pl";
}
for my $pm (@modulepaths) {
	$pm{"lib/$pm"} = "\$(INST_LIBDIR)/$pm";
}

WriteMakefile(
	NAME=>		'NDB',
	PM=>		\%pm,
	EXE_FILES=>	[ qw(ndbrun) ],
# install
	PREFIX=>	$top,
	LIB=>		"$top/lib/perl5",
);

sub MY::postamble {
	my $mk = "";
	$mk .= "\n" . <<END;
# NDB make targets
libs: all install
bins:
links:
depend:
clean_dep:
#clean:
cleanall:
tidy:
#distclean:
check:
	perl -Ilib -cw -e "use NDB::Util"
	perl -Ilib -cw -e "use NDB::Net"
	perl -Ilib -cw -e "use NDB::Run"
	perl -Ilib -cw ndbnetd.pl
	perl -Ilib -cw ndbnet.pl
END
	if ($Config{osname} ne 'MSWin32') {
		$mk .= "\n" . <<END;
# install startup scripts to \$NDB_BASE
install-base:
	test "\$\$NDB_BASE"
	mkdir -p \$\$NDB_BASE/bin
	rm -f \$\$NDB_BASE/bin/ndbnet
	cp -p ndbnet.sh \$\$NDB_BASE/bin/ndbnet
	chmod +x \$\$NDB_BASE/bin/ndbnet
	rm -f \$\$NDB_BASE/bin/ndbnetd
	cp -p ndbnetd.sh \$\$NDB_BASE/bin/ndbnetd
	chmod +x \$\$NDB_BASE/bin/ndbnetd
END
	} else {
		$mk .= "\n" . <<END;
install-base:
	copy ndbnet.sh $base\\bin\\ndbnet.bat
	copy ndbnetd.sh $base\\bin\\ndbnetd.bat
END
	}
	return $mk;
}

1;
