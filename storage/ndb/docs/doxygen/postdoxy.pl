# Copyright (C) 2003, 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#
#  Written by Lars Thalmann, lars@mysql.com, 2003.
#

use strict;
umask 000;

# -----------------------------------------------------------------------------
#   Settings
# -----------------------------------------------------------------------------

$ENV{LD_LIBRARY_PATH} = "/usr/local/lib:/opt/as/local/lib";
$ENV{LD_LIBRARY_PATH} = $ENV{LD_LIBRARY_PATH} . ":/opt/as/forte6/SUNWspro/lib";
$ENV{PATH} = $ENV{PATH} . ":/usr/local/bin:/opt/as/local/bin";
$ENV{PATH} = $ENV{PATH} . ":/opt/as/local/teTeX/bin/sparc-sun-solaris2.8";

my $destdir = @ARGV[0];
my $title = ""; # $ARGV[1];

my $release;
if (defined $ENV{'NDB_RELEASE'}) {
    $release = $ENV{'NDB_RELEASE'};
    print "----------------------------------------------------------------\n";
    print "Relase = " . $release . "\n";
    print "----------------------------------------------------------------\n";
} else {
    print "----------------------------------------------------------------\n";
    print "NDB Documentation is being modified to statndard format\n";
    print "(If you want this automatic, use env variable NDB_RELEASE.)\n";
    print "Enter release (Examples: \"1.43.0 (alpha)\" or \"2.1.0 (gamma)\"): ";
    $release = <stdin>;
    print "----------------------------------------------------------------\n";
}

# -----------------------------------------------------------------------------
#  Change a little in refman.tex
# -----------------------------------------------------------------------------

open (INFILE, "< ${destdir}/refman.tex") 
    or die "Error opening ${destdir}/refman.tex.\n";
open (OUTFILE, "> ${destdir}/refman.tex.new") 
    or die "Error opening ${destdir}/refman.tex.new.\n";

while (<INFILE>) 
{
    if (/(.*)(RELEASE)(.*)$/) {
	print OUTFILE $1 . $release . $3;
    } elsif (/(.*)(DATE)(.*)$/) {
	print OUTFILE $1 . localtime() . $3;
    } elsif (/\\chapter\{File Index\}/) {
	# Erase
    } elsif (/\\input\{files\}/) {
	# Erase
    } elsif (/\\chapter\{Hierarchical Index\}/) {
	# Erase
    } elsif (/\\input\{hierarchy\}/) {
	# Erase
    } elsif (/\\chapter\{Page Index\}/) {
	# Erase
    } elsif (/\\input\{pages\}/) {
	# Erase
    } else {
	print OUTFILE;
    }
}

close INFILE;
close OUTFILE;
    
system("mv ${destdir}/refman.tex.new ${destdir}/refman.tex");

# -----------------------------------------------------------------------------
#  Change a little in doxygen.sty
# -----------------------------------------------------------------------------

open (INFILE, "< ${destdir}/doxygen.sty") 
    or die "Error opening INFILE.\n";
open (OUTFILE, "> ${destdir}/doxygen.sty.new") 
    or die "Error opening OUTFILE.\n";

while (<INFILE>) 
{
    if (/\\rfoot/) {
	print OUTFILE "\\rfoot[\\fancyplain{}{\\bfseries\\small \\copyright~Copyright (C) 2003, 2024, Oracle and/or its affiliates.\\hfill support-cluster\@mysql.com}]{}\n";
    } elsif (/\\lfoot/) {
	print OUTFILE "\\lfoot[]{\\fancyplain{}{\\bfseries\\small support-cluster\@mysql.com\\hfill \\copyright~Copyright (C) 2003, 2024, Oracle and/or its affiliates.}}\n";
    } else {
	print OUTFILE;
    }
}

close INFILE;
close OUTFILE;
    
system("mv ${destdir}/doxygen.sty.new ${destdir}/doxygen.sty");

