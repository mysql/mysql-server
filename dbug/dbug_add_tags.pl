#!/usr/bin/perl

# Copyright (c) 2002 MySQL AB, 2009 Sun Microsystems, Inc.
# Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

die "No files specified\n" unless $ARGV[0];

$ctags="exctags -x -f - --c-types=f -u";

sub get_tag {
  local $.; local $_=<TAGS>;
  ($symbol, $line)= /^(.*\S)\s+function\s+(\d+)/;
  $symbol=$1 if /[\s*]([^\s*]+)\s*\(/;
  $line=1e50 unless $line;
}

while($src=shift)
{
  warn "==> $src\n";
 
  $dst=$src.$$;
  open(TAGS, "$ctags $src|") || die "Cannot exec('$ctags $src'): $!";
  open(SRC, "<$src") || die "Cannot open $src: $!";
  open(DST, ">$dst") || die "Cannot create $dst: $!";
  select DST;

  &get_tag;
  $in_func=0;
  while(<SRC>)
  {
    my $orig=$_;
    if ($in_func)
    {
      if (/\breturn\b/ && !/\/\*.*\breturn\b.*\*\// && !/;/ )
      {
        $_.=<SRC> until /;/;
      }
      s/(?<=\s)return\s*;/DBUG_VOID_RETURN;/;
      s/(?<=\s)return\s*(.+)\s*;/DBUG_RETURN(\1);/s;
      $ret_line=$. if /DBUG_(VOID_)?RETURN/; #{{
      print "$tab  DBUG_VOID_RETURN;\n" if /^$tab}/ && $ret_line < $.-1;
      $in_func=0 if /^$tab}/;
      warn "$src:".($.-1)."\t$orig" if /\breturn\b/;
    }
    print;
    next if $. < $line;
    die "Something wrong: \$.=$., \$line=$line, \$symbol=$symbol\n" if $. > $line;
    &get_tag && next if /^\s*inline /;
    print $_=<SRC> until /{/; $tab=$`;
    &get_tag && next if /}/; # skip one-liners
    $semicolon=1;
    while(<SRC>)
    {
      $skip=!$semicolon;
      $semicolon= /;\s*$/;
      print && next if $skip ||
        (/^\s+\w+((::\w+)?|<\w+>)\s+\**\w+/ && !/^\s*return\b/);
      last if /DBUG_ENTER/;
      print "$tab  DBUG_ENTER(\"$symbol\");\n";
      print "\n" unless $_ eq "\n";
      last;
    }
    $in_func=1;
    &get_tag;
    redo;
  }
  close SRC;
  close DST;
  close TAGS;
  unlink("$src.orig");
  rename($src, "$src.orig") || die "Cannot rename $src to $src.orig: $!";
  rename($dst, $src) || die "Cannot rename $dst to $src: $!";
}

warn "All done!\n";

