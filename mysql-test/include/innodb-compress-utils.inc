### utilities related to transparent page compression

use strict;
use File::Spec::Functions;
use warnings;
no warnings ('uninitialized');
use English;
 
### verify tables are compressed as expected
### no parameters, no return value
### uses environmental variables as input:
### COMPR_TABLES: comma delimited list of table names as database/table
### COMPR_SIZES: comma delimited list of elements corresponding to the tables:
###   each element is a blank delimited list: 
###     table_name file_size allocated_size
### COMPR_EXPECT: comma delimited list of 0 or 1 per table,
###   1 means compression is expected, 0 otherwise
### Prints output whether a table is compressed, for each table
### If compression is not per expectations, prints out error messages
sub table_compression_verify {
  my @lexpect = sort(split(/\s*,\s*/,$ENV{'COMPR_EXPECT'}));
  my @ltables = sort(split(/\s*,\s*/,$ENV{'COMPR_TABLES'}));
  my @lsizes = split(/\s*,\s*/,$ENV{'COMPR_SIZES'});
  my %hsizes;
  foreach (@lsizes) {
      next if (/^\s*$/);
      my @l = split(/\s+/,$_);
      $hsizes{$l[0]} = [$l[1],$l[2]];
  }

  my $ris = 'is compressed';
  my $risnot = 'is not compressed';
  my $nexp = -1;
  foreach (@ltables) {
      ++$nexp;
      if (not exists($hsizes{$_})) {
          printf("%s does not exist\n",$_);
          next;
      }

      my $plt = $hsizes{$_};
      my $compr = ($plt->[0] > $plt->[1]);
      printf("%s %s\n",$_,($compr? $ris : $risnot));

### check if compression is what we expect
      if ($compr and not $lexpect[$nexp]) {
          printf(
 "ERROR: %s is compressed while it should not be, ".
 "file size '%s', allocated size '%s'\n",
            $_,$plt->[0],$plt->[1]);
      } elsif (not $compr and $lexpect[$nexp]) {
          printf(
 "ERROR: %s is not compressed while it should be, ".
 "file size '%s', allocated size '%s'\n",
            $_,$plt->[0],$plt->[1]);
      }

### check file size
      my $file = catfile($ENV{'MYSQLD_DATADIR'},"$_.ibd");
      if ($plt->[0] != (-s $file)) {
          printf(
 "ERROR: %s: incorrect file size %s reported by ".
 "I_S.innodb_sys_tablespaces.file_size, actual size %s\n",
            $_,$plt->[0],(-s $file));
      }

### check allocated file size
      if ($plt->[1] != (my $alloc = file_allocated_size($file))) {
          printf(
 "ERROR: %s: incorrect allocated file size %s reported by ".
 "I_S.innodb_sys_tablespaces.allocated_size, actual allocated size %s\n",
            $_,$plt->[1],$alloc);
      }
  }

}

### in: 1: file pathname
###     2: suggested size, this is what is returned on Win for now
### out: file allocated size in bytes
sub file_allocated_size {
  my ($file) = @_;
  if ($OSNAME eq 'MSWin32') {
      eval('use Win32::API');
      my $GetCompressedFileSize = Win32::API->new(
        "kernel32","GetCompressedFileSize",['P', 'P'], 'N')
        or die('failed to create GetCompressedFileSize object');
      my $hiword = pack("L", 0);
      my $alloc =  $GetCompressedFileSize->Call($file, $hiword);
      my $gb = unpack('L',$hiword);
      return($alloc+$gb*4*1024**3);
  }
  my @ls = stat($file);
  return($ls[12]*512);
}

### verify compression for files in a directory
### returns nothing.
### in: 1: directory, usually datadir
###     2: ref HASH
###   compr => list of files that are expected to be compressed.
###            File format is database/table, e.g. test/t1
###   nc    => list of patterns that are expected not to be compressed,
###            for example, test/*.ibd
###            However, if a file appears both in the results of 'compr'
###            and 'nc' processing, it is expected to be compressed.
### out: prints a message for each file in 'compr'.
###      Prints error messages for each compressed file in 'nc'
sub dir_compression_verify {
  my ($dir,$phcompr) = @_;
  my @lcompr = @{$phcompr->{'compr'}};
  foreach (@lcompr) {
      my $file = catfile($dir,"$_.ibd");
      my $size = (-s $file);
      my $alloc = file_allocated_size($file);
      if ($size > $alloc) {
          printf("%s is compressed\n",$_);
          next;
      }
      printf("ERROR: %s is not compressed, size %s, allocated %s\n",
        $file,$size,$alloc);
  }
  my @lncglob = @{$phcompr->{'nc'}};
  my @lnc = ();
  foreach (@lncglob) {
      push(@lnc,glob(catfile($dir,$_)));
  }
  foreach (sort(@lnc)) {
      my $file = $_;
      if (grep {my $cofile = catfile($dir,"$_.ibd");
                $file eq $cofile} @lcompr) {
          next;
      }
      my $size = (-s $file);
      my $alloc = file_allocated_size($file);
      if ($size <= $alloc) {
          next;
      }
      printf("ERROR: %s is compressed, size %s, allocated %s\n",
        $file,$size,$alloc);
  }
}

1;
