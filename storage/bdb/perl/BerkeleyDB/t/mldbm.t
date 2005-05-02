#!/usr/bin/perl -w

use strict ;

BEGIN 
{
    if ($] < 5.005) {
	print "1..0 # This is Perl $], skipping test\n" ;
	exit 0 ;
    }

    eval { require Data::Dumper ; };
    if ($@) {
	print "1..0 # Data::Dumper is not installed on this system.\n";
	exit 0 ;
    }
    if ($Data::Dumper::VERSION < 2.08) {
	print "1..0 # Data::Dumper 2.08 or better required (found $Data::Dumper::VERSION).\n";
	exit 0 ;
    }
    eval { require MLDBM ; };
    if ($@) {
	print "1..0 # MLDBM is not installed on this system.\n";
	exit 0 ;
    }
}

use t::util ;

print "1..12\n";

{
    package BTREE ;
    
    use BerkeleyDB ;
    use MLDBM qw(BerkeleyDB::Btree) ; 
    use Data::Dumper;
    
    my $filename = "";
    my $lex = new LexFile $filename;
    
    $MLDBM::UseDB = "BerkeleyDB::Btree" ;
    my %o ;
    my $db = tie %o, 'MLDBM', -Filename => $filename,
    		     -Flags    => DB_CREATE
    		or die $!;
    ::ok 1, $db ;
    ::ok 2, $db->type() == DB_BTREE ;
    
    my $c = [\'c'];
    my $b = {};
    my $a = [1, $b, $c];
    $b->{a} = $a;
    $b->{b} = $a->[1];
    $b->{c} = $a->[2];
    @o{qw(a b c)} = ($a, $b, $c);
    $o{d} = "{once upon a time}";
    $o{e} = 1024;
    $o{f} = 1024.1024;
    my $first = Data::Dumper->new([@o{qw(a b c)}], [qw(a b c)])->Quotekeys(0)->Dump;
    my $second = <<'EOT';
$a = [
       1,
       {
         a => $a,
         b => $a->[1],
         c => [
                \'c'
              ]
       },
       $a->[1]{c}
     ];
$b = {
       a => [
              1,
              $b,
              [
                \'c'
              ]
            ],
       b => $b,
       c => $b->{a}[2]
     };
$c = [
       \'c'
     ];
EOT
    
    ::ok 3, $first eq $second ;
    ::ok 4, $o{d} eq "{once upon a time}" ;
    ::ok 5, $o{e} == 1024 ;
    ::ok 6, $o{f} eq 1024.1024 ;
    
}

{

    package HASH ;

    use BerkeleyDB ;
    use MLDBM qw(BerkeleyDB::Hash) ; 
    use Data::Dumper;

    my $filename = "";
    my $lex = new LexFile $filename;

    unlink $filename ;
    $MLDBM::UseDB = "BerkeleyDB::Hash" ;
    my %o ;
    my $db = tie %o, 'MLDBM', -Filename => $filename,
		         -Flags    => DB_CREATE
		    or die $!;
    ::ok 7, $db ;
    ::ok 8, $db->type() == DB_HASH ;


    my $c = [\'c'];
    my $b = {};
    my $a = [1, $b, $c];
    $b->{a} = $a;
    $b->{b} = $a->[1];
    $b->{c} = $a->[2];
    @o{qw(a b c)} = ($a, $b, $c);
    $o{d} = "{once upon a time}";
    $o{e} = 1024;
    $o{f} = 1024.1024;
    my $first = Data::Dumper->new([@o{qw(a b c)}], [qw(a b c)])->Quotekeys(0)->Dump;
    my $second = <<'EOT';
$a = [
       1,
       {
         a => $a,
         b => $a->[1],
         c => [
                \'c'
              ]
       },
       $a->[1]{c}
     ];
$b = {
       a => [
              1,
              $b,
              [
                \'c'
              ]
            ],
       b => $b,
       c => $b->{a}[2]
     };
$c = [
       \'c'
     ];
EOT

    ::ok 9, $first eq $second ;
    ::ok 10, $o{d} eq "{once upon a time}" ;
    ::ok 11, $o{e} == 1024 ;
    ::ok 12, $o{f} eq 1024.1024 ;

}
