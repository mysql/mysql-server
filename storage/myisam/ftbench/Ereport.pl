#!/usr/bin/perl

# Copyright (c) 2003, 2024, Oracle and/or its affiliates.
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

die "Use: $0 eval_output qrels_file\n" unless @ARGV==2;

open(EOUT,$eout=shift) || die "Cannot open $eout: $!";
open(RELJ,$relj=shift) || die "Cannot open $relj: $!";

$_=<EOUT>;
die "$eout must start with a number!\n "unless /^[1-9][0-9]*\n/;
$ndocs=$_+0;

$qid=0;
$relj_str=<RELJ>;
$eout_str=<EOUT>;

while(!eof(RELJ) || !eof(EOUT)) {
  ++$qid;
  %dq=();
  $A=$B=$AB=0;
  $Ravg=$Pavg=0;

  while($relj_str =~ /^0*$qid\s+(\d+)/) {
    ++$A;
    $dq{$1+0}=1;
    last unless $relj_str=<RELJ>;
  }
  # Favg measure = 1/(a/Pavg+(1-a)/Ravg)
sub Favg { my $a=shift; $Pavg*$Ravg ? 1/($a/$Pavg+(1-$a)/$Ravg) : 0; }
  # F0    : a=0                 -- ignore precision
  # F5    : a=0.5
  # F1    : a=1                 -- ignore recall
  while($eout_str =~ /^$qid\s+(\d+)\s+(\d+(?:\.\d+)?)/) {
    $B++;
    $AB++ if $dq{$1+0};
    $Ravg+=$AB;
    $Pavg+=$AB/$B;
    last unless $eout_str=<EOUT>;
  }
  next unless $A;

  $Ravg/=$B*$A if $B;
  $Pavg/=$B    if $B;

  printf "%5d %1.12f %1.12f %1.12f\n", $qid, Favg(0),Favg(0.5),Favg(1);
}

exit 0;


