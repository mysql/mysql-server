#!/usr/bin/perl

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


