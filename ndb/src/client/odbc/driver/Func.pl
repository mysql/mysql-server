#

use strict;
select(STDOUT);
$| = 1;
use vars qw($func);

my $action = shift;
my @args = @ARGV;
if (! $action) {
    print <<END;
usage: perl $0 <data|name|code|move> ...
data -unixodbc -- write new Func.data to stdout
name [-[no]auto -type] [suffix] -- list function names
code -- write auto/*.cpp
diff -- diff against auto/*.cpp
move -- move auto/*.cpp to .
functab -- write struct entiries for SQLGetFunctions
END
	exit(0);
}

# indents
my $i1 = " " x (1*4);
my $i2 = " " x (2*4);
my $i3 = " " x (3*4);
my $i4 = " " x (4*4);

if ($action eq 'data') {
    my @entry = ();
    while (@args) {
	my $file = shift(@args);
	if ($file eq '-unixodbc') {
	    unshift(@args, </usr/local/include/{sql,sqlext}.h>);
	    next;
	}
	if ($file eq '-iodbc') {
	    unshift(@args, </usr/local/opt/iODBC/include/{sql,sqlext}.h>);
	    next;
	}
	warn "read $file\n";
	open(F, "<$file") || die "$file: $!";
	my $text = undef;
	my $odbcver = undef;
	while ($_ = <F>) {
	    chomp;
	    if (/^\s*$/) {
		next;
	    }
	    if (/^\s*#\s*if\s+(.*\bODBCVER\b.*)$/) {
		$odbcver = $1;
		$odbcver =~ s/^\s+|\s+$//g;
		$odbcver =~ s/^\(+|\)+$//g;
		next;
	    }
	    if (/^\s*#\s*endif\b/) {
		$odbcver = undef;
		next;
	    }
	    if (/^\s*SQLRETURN\b/) {
		$text = "";
	    }
	    if (defined($text)) {
		$text .= $_;
		if (/;\s*$/) {
		    push(@entry, {
			text => $text,
			odbcver => $odbcver || 'ODBCVER >= 0x0000',
		    });
		    $text = undef;
		}
	    }
	}
	close(F);
    }
    warn "@{[ scalar @entry ]} entries\n";
    $func = {};
    for my $e (@entry) {
	my $text = $e->{text};
	$text =~ s!/\*.+?\*/!!g;
	$text =~ s/^\s+|\s+$//g;
	$text =~ s/\s\s*/\040/g;
	$text =~ /^(SQLRETURN)\s+(SQL_API\s+)?(\w+)\s*\((.*)\)\s*;/
	    or warn "discard: $_\n", next;
	my $type = $1;
	my $name = $3;
	my $body = $4;
	my $f = {};
	$f->{type} = $type;
	$f->{name} = $name;
	my @body = split(/,/, $body);
	my $param = [];
	for my $s (@body) {
	    $s =~ s/^\s+|\s+$//g;
	    my($ptype, $pptr, $pname);
	    if ($s =~ /^(\w+)\s+(\w+)$/) {
		$ptype = $1;
		$pptr = 0;
		$pname = $2;
	    } elsif ($s =~ /^(\w+)\s*\*\s*(\w+)$/) {
		$ptype = $1;
		$pptr = 1;
		$pname = $2;
	    } else {
		warn "discard: $name: param $s\n";
		$param = undef;
		last;
	    }
	    my $pindex = scalar @$param;
	    push(@$param, {
		type => $ptype,
		ptr => $pptr,
		name => $pname,
		index => $pindex,
	    });
	}
	$param or next;
	$f->{param} = $param;
	$f->{odbcver} = $e->{odbcver};
	$func->{$name}
	    and warn "duplicate: $name\n", next;
	$func->{$name} = $f;
    }
    print "\$func = {\n";
    for my $name (sort keys %$func) {
	my $f = $func->{$name};
	print "${i1}$name => {\n";
	print "${i2}type => '$f->{type}',\n";
	print "${i2}name => '$f->{name}',\n";
	print "${i2}param => [\n";
	for my $p (@{$f->{param}}) {
	    print "${i3}\{\n";
	    print "${i4}type => '$p->{type}',\n";
	    print "${i4}ptr => $p->{ptr},\n";
	    print "${i4}name => '$p->{name}',\n";
	    print "${i4}index => $p->{index},\n";
	    print "${i3}\},\n";
	}
	print "${i2}],\n";
	print "${i2}odbcver => '$f->{odbcver}',\n";
	print "${i1}},\n";
    }
    printf "};\n";
    $action = undef;
}

if ($action eq 'name') {
    my %functab = ();	# bit in FuncTab
    my $functab = "../handles/FuncTab.cpp";
    if (! open(F, "<$functab")) {
	warn "$functab: $!";
    } else {
	while ($_ = <F>) {
	    if (/SQL_API_([A-Z]+)[\s,]*([01])/) {
		defined $functab{$1} and die "$_";
		$functab{$1} = $2;
	    }
	}
	close(F);
    }
    require './Func.data';
    my $auto = 1;
    my $noauto = 1;
    my $type = 0;
    while ($args[0] =~ /^-(\w+)$/) {
	$noauto = 0 if $1 eq 'auto';
	$auto = 0 if $1 eq 'noauto';
	$type = 1 if $1 eq 'type';
	shift(@args);
    }
    my $suffix = shift(@args);
    for my $name (sort keys %$func) {
	my $f = $func->{$name};
	local $/ = undef;
	my($x1);
	if (open(F, "<$name.cpp")) {
	    $x1 = <F>;
	    close(F);
	    if ($x1 =~ /\bauto_$name\b/) {
		$auto || next;
		print "A " if $type;
	    } else {
		$noauto || next;
		print "- " if $type;
	    }
	    if ($type) {
		my $y = $functab{uc $name};
		$y = "?" if $y !~ /^[01]$/;
		print "$y ";
		my $z = $f->{odbcver};
		$z =~ s/^.*(...)$/$1/;
		print "$z ";
	    }
	}
	print "$name$suffix\n";
    }
    $action = undef;
}

if ($action eq 'code') {
    require './Func.data';
    system("rm -rf auto; mkdir auto");
    for my $name (sort keys %$func) {
	my $f = $func->{$name};
	my $file = "auto/$name.cpp";
	open(F, ">$file") || die "$file: $!\n";
	print F "#include \"driver.hpp\"\n";
	print F "\n";
	printf F "#if $f->{odbcver}\n";
	print F "$f->{type} SQL_API\n";
	print F "$f->{name}(";
	for my $p (@{$f->{param}}) {
	    print F "," if $p->{index} > 0;
	    print F "\n${i1}$p->{type}";
	    for (my $i = 0; $i < $p->{ptr}; $i++) {
		print F "*";
	    }
	    print F " $p->{name}";
	}
	print F ")\n";
	print F "{\n";
	print F "${i1}const char* const sqlFunction = \"$f->{name}\";\n";
	print F "#ifndef auto_$name\n";
	print F "${i1}Ctx ctx;\n";
	print F "${i1}ctx.log(1, \"*** not implemented: %s\", sqlFunction);\n";
	print F "${i1}return SQL_ERROR;\n";
	print F "#else\n";
	my @ihandle = ();
	my @ohandle = ();
	for my $p (@{$f->{param}}) {
	    if ($p->{type} =~ /^SQLH(ENV|DBC|STMT|DESC)$/) {
		$p->{btype} = lc $1;
		my $h = ! $p->{ptr} ? \@ihandle : \@ohandle;
		push(@$h, $p);
	    }
	}
	if (! @ihandle) {		# use root handle instance
	    push(@ihandle, {
		type => 'SQLHROOT',
		name => '(SQLHANDLE*)0',
	    });
	}
	for my $p (@ihandle, @ohandle) {
	    $p->{htype} = "Handle" . (ucfirst lc $p->{btype});
	    $p->{hname} = "p" . (ucfirst lc $p->{btype});
	}
	if (@ihandle) {
	    print F "${i1}HandleRoot* const pRoot = HandleRoot::instance();\n";
	}
	for my $p (@ihandle) {
	    print F "${i1}$p->{htype}* $p->{hname} = ";
	    print F "pRoot->find" . ucfirst($p->{btype}). "($p->{name});\n";
	    print F "${i1}if ($p->{hname} == 0)\n";
	    print F "${i2}return SQL_INVALID_HANDLE;\n";
	}
	{
	    my $p = $ihandle[0];
	    print F "${i1}Ctx& ctx = $p->{hname}->initCtx();\n";
	    print F "${i1}ctx.logSqlEnter(sqlFunction);\n";
	}
	for my $p (@ohandle) {
	    print F "${i1}$p->{htype}* $p->{hname} = 0;\n";
	}
	{
	    my $p = $ihandle[0];
	    my $fname = $f->{name};
	    $fname =~ s/^SQL/sql/;	# keep sql prefix
	    print F "${i1}if (ctx.ok())\n";
	    print F "${i2}$p->{hname}->$fname(\n";
	    print F "${i3}ctx";
	}
	for my $p (@{$f->{param}}) {
	    if ($p == $ihandle[0]) {
		next;
	    }
	    print F ",";
	    print F "\n${i3}";
	    if (grep($_ == $p, @ihandle)) {
		print F "$p->{hname}";
	    } elsif (grep($_ == $p, @ohandle)) {
		print F "$p->{name} != 0 ? &$p->{hname} : 0";
	    } else {
		print F "&" if $p->{ptr} > 0;
		print F "$p->{name}";
	    }
	}
	print F "\n${i2});\n";
	for my $p (@ohandle) {
	    print F "${i1}if ($p->{name} != 0)\n";
	    print F "${i2}*$p->{name} = ";
	    print F "pRoot->from" . ucfirst($p->{btype}) . "($p->{hname});\n";
	}
	{
	    my $p = $ihandle[0];
	    print F "${i1}$p->{hname}->saveCtx(ctx);\n";
	}
	print F "${i1}ctx.logSqlExit();\n";
	print F "${i1}return ctx.getCode();\n";
	print F "#endif\n";
	print F "}\n";
	print F "#endif // $f->{odbcver}\n";
	close(F);
    }
    $action = undef;
}

if ($action eq 'diff' || $action eq 'move') {
    require './Func.data';
    for my $name (sort keys %$func) {
	local $/ = undef;
	my($x1, $x2);
	if (open(F, "<$name.cpp")) {
	    $x1 = <F>;
	    close(F);
	    if ($x1 !~ /\bauto_$name\b/) {
		warn "$name.cpp: not auto-generated\n" if $action eq 'move';
		next;
	    }
	}
	if (! open(F, "<auto/$name.cpp")) {
	    die "auto/$name.cpp: $!\n";
	}
	$x2 = <F>;
	close(F);
	if ($x1 eq $x2) {
	    warn "$name: no changes\n" if $action eq 'move';
	    next;
	}
	if ($action eq 'diff') {
	    print "=" x 40, "\n";
	    print "diff $name.cpp auto/", "\n";
	    system("diff $name.cpp auto/$name.cpp");
	} else {
	    rename("auto/$name.cpp", "$name.cpp")
		or die "rename $name: $!\n";
	    warn "$name: updated\n" if 0;
	}
    }
    $action = undef;
}

if ($action eq 'functab') {
    require './Func.data';
    for my $name (sort keys %$func) {
	printf "%4s{%3s%-30s, 0  },\n", "", "", uc "SQL_API_$name";
    }
    $action = undef;
}

$action && die "$action: undefined\n";

# vim: set sw=4:
