#!/usr/bin/perl

# Copyright (C) 2003 MySQL AB
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

# compares out-files (as created by Ereport.pl) from dir1/*.out and dir2/*.out
# for each effectiveness column computes the probability of the hypothesis
# "Both files have the same effectiveness"

# sign test is used to verify that test results are statistically
# significant to support the hypothesis. Function is computed on the fly.

# basic formula is \sum_{r=0}^R C_N^r 2^{-N}
# As N can be big, we'll work with logarithms
$log2=log(2);
sub probab {
  my $N=shift, $R=shift;

  my $r, $sum=0;

  for $r (0..$R) {
    $sum+=exp(logfac($N)-logfac($r)-logfac($N-$r)-$N*$log2);
  }
  return $sum;
}

# log(N!)
# for N<20 exact value from the table (below) is taken
# otherwise, Stirling approximation for N! is used
sub logfac {
  my $n=shift; die "n=$n<0" if $n<0;
  return $logfactab[$n] if $n<=$#logfactab;
  return $n*log($n)-$n+log(2*3.14159265358*$n)/2;
}
@logfactab=(
0,    0,         0.693147180559945, 1.79175946922805, 3.17805383034795,
4.78749174278205, 6.57925121201010, 8.52516136106541, 10.6046029027453,
12.8018274800815, 15.1044125730755, 17.5023078458739, 19.9872144956619,
22.5521638531234, 25.1912211827387, 27.8992713838409, 30.6718601060807,
33.5050734501369, 36.3954452080331, 39.3398841871995, 42.3356164607535,
);

############################# main () ###############################
#$p=shift; $m=shift; $p-=$m;
#if($p>$m) {
#  print "1 > 2 [+$p-$m]: ", probab($p+$m, $m), "\n";
#} elsif($p<$m) {
#  print "1 < 2 [+$p-$m]: ", probab($p+$m, $p), "\n";
#} else {
#  print "1 = 2 [+$p-$m]: ", probab($p+$m, $m), "\n";
#}
#exit;

die "Use: $0 dir1 dir2\n" unless @ARGV==2 &&
                             -d ($dir1=shift) && -d ($dir2=shift);
$_=`cd $dir1; echo *.out`;
s/\.out\b//g;
$total="";

for $file (split) {
  open(OUT1,$out1="$dir1/$file.out") || die "Cannot open $out1: $!";
  open(OUT2,$out2="$dir2/$file.out") || die "Cannot open $out2: $!";

  @p=@m=();
  while(!eof(OUT1) || !eof(OUT2)) {
    $_=<OUT1>; @l1=split; shift @l1;
    $_=<OUT2>; @l2=split; shift @l2;

    die "Number of columns differ in line $.\n" unless $#l1 == $#l2;

    for (0..$#l1) {
      $p[$_]+= $l1[$_] > $l2[$_];
      $m[$_]+= $l1[$_] < $l2[$_];
    }
  }

  for (0..$#l1) {
    $pp[$_]+=$p[$_]; $mm[$_]+=$m[$_];
    $total[$_].=rep($file, ($#l1 ? $_ : undef), $p[$_], $m[$_]);
  }
  close OUT1;
  close OUT2;
}

for (0..$#l1) {
  rep($total[$_], ($#l1 ? $_ : undef), $pp[$_], $mm[$_]);
}

sub rep {
  my ($test, $n, $p, $m, $c, $r)=@_;
  
  if   ($p>$m) { $c=">"; $r="+"; }
  elsif($p<$m) { $c="<"; $r="-"; }
  else         { $c="="; $r="="; }
  $n=" $n: " if defined $n;
  printf "%-8s $n $dir1 $c $dir2 [+%03d-%03d]: %16.15f\n",
          $test, $p, $m, probab($p+$m, ($p>=$m ? $m : $p));
  $r;
}
