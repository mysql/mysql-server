#!/usr/bin/perl

# Based on a Emacs macro by david@mysql.com
# Implemented in Perl by jeremy@mysql.com
# 2001-11-20 Fixups by arjen@mysql.com, 2 keywords and 15 synonyms were missing

print STDERR "Scanning lex.h for symbols..\n";
open LEX, "<../sql/lex.h";
while($line = <LEX>) {
  if($line =~ /\{\s*\"([A-Z_]+)\"/) {
    $words{$1} = $1;
  } elsif($line =~ /sql_functions/) {
    last;
  };
};
close LEX;

print STDERR "Scanning sql_yacc.yy for non-reserved words...\n";
open YACC, "<../sql/sql_yacc.yy";
while(<YACC> !~ /^keyword:/) {};
while(($line = <YACC>) =~ /[\s|]+([A-Z_]+)/) {
  $keyword = $1;
  $keyword =~ s/_SYM//;
  delete $words{$keyword};
};
close YACC;


$list = sprintf("\@c Reserved word list updated %s by %s.\n".
                "\@c To regenerate, use Support/update-reserved-words.pl.\n\n",
                &pretty_date, $ENV{USER});


print STDERR "Copying reserved words to an array...\n";
foreach(keys %words) { push @words, $words{$_}; };

print STDERR "Sorting array...\n";
@words = sort @words;

printf STDERR "There are %i reserved words.\n", scalar @words;

@pre  = ("\@item", "\@tab", " \@tab", "\@tab");
@post = ("", "\n", "", "\n");

for($i=0; $word = shift(@words); $i++) {
  $list .= sprintf "%s %-30s %s", $pre[$i%4], "\@code\{$word\}", $post[$i%4];
}; $list .= "\n";

open OLD, "<manual.texi";
open NEW, ">manual-tmp.texi";

print STDERR "Copying beginning of manual.texi...\n";
while(($line = <OLD>) !~ /START_OF_RESERVED_WORDS/) { print NEW $line; };
print NEW "\@c START_OF_RESERVED_WORDS\n\n";
print STDERR "Inserting list of reserved words...\n";
print NEW "\@multitable \@columnfractions .25 .25 .25 .25\n";
print NEW $list;
print NEW "\@end multitable\n";
print STDERR "Skipping over old list...\n";
while(($line = <OLD>) !~ /END_OF_RESERVED_WORDS/) {};
print NEW "\n\@c END_OF_RESERVED_WORDS\n";
print STDERR "Copying end of manual.texi...\n";
while($line = <OLD>) { print NEW $line; };

close OLD;
close NEW;

print STDERR "Moving manual-tmp.texi to manual.texi...\n";
unlink "manual.texi";
rename "manual-tmp.texi", "manual.texi";

print STDERR "Reserved word list updated successfully!\n";

sub pretty_date {
  @time = ($time = shift)?((localtime($time))[0..6]):((localtime)[0..6]);

  ($sec, $min, $hour, $mday, $mon, $year, $wday) = @time;
  $wday = (Sun,Mon,Tue,Wed,Thu,Fri,Sat)[$wday];
  $mon = (Jan,Feb,Mar,Apr,May,Jun,Jul,Aug,Sep,Oct,Nov,Dec)[$mon];
  $year += 1900;

  $pretty = sprintf("%s %s %2i %02i:%02i:%02i %i",
                    $wday, $mon, $mday, $hour, $min, $sec, $year);

  return $pretty;
};

