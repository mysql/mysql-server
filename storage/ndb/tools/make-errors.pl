#! /usr/local/bin/perl

# Copyright (C) 2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use strict;
use Getopt::Long;
use XML::Parser;

(my $progname = $0) =~ s!^.*/!!;

sub usage {
    my $errstr = "@_";
    while (chomp($errstr)) {}
    print <<END;
$progname: $errstr
$progname -- read codes.xml and write codes.hpp and codes.cpp
usage: $progname [options] codes.xml
-c  check xml file only
-d  check xml file and show diff against old hpp and cpp
END
    exit(1);
}

my $opts = {};
opts: {
    local $SIG{__WARN__} = \&usage;
    GetOptions($opts, qw(c d));
}
@ARGV == 1 or usage("one filename argument expected");
my $filexml = shift;
$filexml =~ /^(.*)\.xml$/ or usage("$filexml does not end in .xml");
my $filehpp = "$1.hpp";
my $filecpp = "$1.cpp";

my $temphpp = "$filehpp-new";
my $tempcpp = "$filecpp-new";
unlink $temphpp, $tempcpp;
open(HPP, ">$temphpp") or die "$temphpp: $!\n";
open(CPP, ">$tempcpp") or die "$tempcpp: $!\n";

my $i2 = " " x 2;
my $i4 = " " x 4;
my $lb = "{";
my $rb = "}";

sub disclaimer {
    my $filename = shift;
    return <<END;
/*
 * $filename -- DO NOT EDIT !!
 *
 * To create a new version (both *.hpp and *.cpp):
 *
 * 1) edit $filexml
 * 2) perl tools/$progname $filexml
 * 3) check all files (*.xml *.hpp *.cpp) into CVS
 *
 * On RedHat linux requires perl-XML-Parser package.
 */
END
}

my $classname = $filehpp;
$classname =~ s!^.*/!!;
$classname =~ s/\.hpp$//;

sub handle_init {
    my($parser) = @_;
    my $guard = $filehpp;
    $guard =~ s!^.*/!!;
    $guard =~ s!([a-z])([A-Z])!${1}_${2}!g;
    $guard =~ s!\.!_!g;
    $guard = uc($guard);
    print HPP "#ifndef $guard\n#define $guard\n\n";
    print HPP disclaimer($filehpp), "\n";
    print HPP "class $classname $lb\n";
    print HPP "${i2}enum Value $lb\n";
    print CPP disclaimer($filecpp), "\n";
    print CPP "/* included in Ndberror.cpp */\n\n";
}

my %classhash = (
    ApplicationError => 1,
    NoDataFound => 1,
    ConstraintViolation => 1,
    SchemaError => 1,
    UserDefinedError => 1,
    InsufficientSpace => 1,
    TemporaryResourceError => 1,
    NodeRecoveryError => 1,
    OverloadError => 1,
    TimeoutExpired => 1,
    UnknownResultError => 1,
    InternalError => 1,
    FunctionNotImplemented => 1,
    UnknownErrorCode => 1,
    NodeShutdown => 1,
);

my $section = undef;
my %codehash = ();
my %namehash = ();

sub handle_start {
    my($parser, $tag, %attr) = @_;
    if ($tag eq 'Error') {
	return;
    }
    if ($tag eq 'Section') {
	$section = $attr{name};
	$section =~ /^\w+$/ or
	    $parser->xpcroak("invalid or missing section name");
	return;
    }
    if ($tag eq 'Code') {
	print HPP ",\n" if %codehash;
	print CPP ",\n" if %codehash;
	my $name = $attr{name};
	my $class = $attr{class};
	my $code = $attr{code};
	my $message = $attr{message};
	$name =~ /^\w+$/ or
	    $parser->xpcroak("invalid or missing error name '$name'");
	$namehash{$name}++ and
	    $parser->xpcroak("duplicate error name '$name'");
	$classhash{$class} or
	    $parser->xpcroak("invalid or missing error class '$class'");
	$code =~ /^\d+$/ or
	    $parser->xpcroak("invalid or missing error code '$code'");
	$codehash{$code}++ and
	    $parser->xpcroak("duplicate error code '$code'");
	$message =~ /\S/ or
	    $parser->xpcroak("invalid or missing error message '$message'");
	$message =~ s/^\s+|\s+$//g;
	my $enum = "${section}_${name}";
	print HPP "${i4}$enum = $code";
	print CPP "${i2}$lb ${classname}::$enum,\n";
	print CPP "${i4}NdbError::$class,\n";
	print CPP "${i4}\"$message\"\n";
	print CPP "${i2}$rb";
	return;
    }
    $parser->xpcroak("unknown tag $tag");
}

sub handle_end {
    my($parser, $tag) = @_;
}

sub handle_final {
    print HPP "\n" if %codehash;
    print HPP "${i2}$rb;\n";
    print HPP "$rb;\n\n#endif\n";
    print CPP ",\n" if 1;
    return 1;
}

my $parser = new XML::Parser(
    ParseParamEnt => 1,
    Handlers => {
	Init => \&handle_init,
	Start => \&handle_start,
	End => \&handle_end,
	Final => \&handle_final,
    },
    ErrorContext => 0,
);
eval {
    $parser->parsefile($filexml);
};
if ($@) {
    my $errstr = join("\n", grep(m!\S! && ! m!^\s*at\s!, split(/\n/, $@)));
    die "$filexml:\n$errstr\n";
}

close(HPP);
close(CPP);
rename($temphpp, $filehpp);
rename($tempcpp, $filecpp);

1;

# vim:set sw=4:
