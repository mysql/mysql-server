#!/usr/bin/perl

die "No files specified\n" unless $ARGV[0];

$ctags="exctags -x -f - --c-types=f -u";

sub get_tag {
  local $.; local $_=<TAGS>;
  ($symbol, $line)= /^(.*\S)\s+function\s+(\d+)/;
  $symbol=$1 if /\s(\S+)\s*\(/;
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
        (/^\s+\w+((::\w+)?|<\w+>)\s+\**\w+/ && !/^\s*return/);
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

