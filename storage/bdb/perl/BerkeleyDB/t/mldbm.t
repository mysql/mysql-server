#!/usr/bin/perl -w

use strict ;

BEGIN 
{
    if ($] < 5.005) {
	print "1..0 # Skip: this is Perl $], skipping test\n" ;
	exit 0 ;
    }

    eval { require Data::Dumper ; };
    if ($@) {
	print "1..0 # Skip: Data::Dumper is not installed on this system.\n";
	exit 0 ;
    }
    if ($Data::Dumper::VERSION < 2.08) {
	print "1..0 # Skip: Data::Dumper 2.08 or better required (found $Data::Dumper::VERSION).\n";
	exit 0 ;
    }
    eval { require MLDBM ; };
    if ($@) {
	print "1..0 # Skip: MLDBM is not installed on this system.\n";
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
    
    my $struct = [@o{qw(a b c)}];
    ::ok 3, ::_compare([$a, $b, $c], $struct);
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

    my $struct = [@o{qw(a b c)}];
    ::ok 9, ::_compare([$a, $b, $c], $struct);
    ::ok 10, $o{d} eq "{once upon a time}" ;
    ::ok 11, $o{e} == 1024 ;
    ::ok 12, $o{f} eq 1024.1024 ;

}
