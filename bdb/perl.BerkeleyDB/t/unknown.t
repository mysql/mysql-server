#!./perl -w

# ID: %I%, %G%   

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use File::Path qw(rmtree);

print "1..41\n";

{
    package LexFile ;

    sub new
    {
	my $self = shift ;
	unlink @_ ;
 	bless [ @_ ], $self ;
    }

    sub DESTROY
    {
	my $self = shift ;
	unlink @{ $self } ;
    }
}


sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}

sub writeFile
{
    my $name = shift ;
    open(FH, ">$name") or return 0 ;
    print FH @_ ;
    close FH ;
    return 1 ;
}

my $Dfile = "dbhash.tmp";
unlink $Dfile;

umask(0) ;


# Check for invalid parameters
{
    # Check for invalid parameters
    my $db ;
    eval ' $db = new BerkeleyDB::Unknown  -Stupid => 3 ; ' ;
    ok 1, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' $db = new BerkeleyDB::Unknown -Bad => 2, -Mode => 0345, -Stupid => 3; ' ;
    ok 2, $@ =~ /unknown key value\(s\) (Bad |Stupid ){2}/  ;

    eval ' $db = new BerkeleyDB::Unknown -Env => 2 ' ;
    ok 3, $@ =~ /^Env not of type BerkeleyDB::Env/ ;

    eval ' $db = new BerkeleyDB::Unknown -Txn => "fred" ' ;
    ok 4, $@ =~ /^Txn not of type BerkeleyDB::Txn/ ;

    my $obj = bless [], "main" ;
    eval ' $db = new BerkeleyDB::Unknown -Env => $obj ' ;
    ok 5, $@ =~ /^Env not of type BerkeleyDB::Env/ ;
}

# check the interface to a rubbish database
{
    # first an empty file
    my $lex = new LexFile $Dfile ;
    ok 6, writeFile($Dfile, "") ;

    ok 7, ! (new BerkeleyDB::Unknown -Filename => $Dfile); 

    # now a non-database file
    writeFile($Dfile, "\x2af6") ;
    ok 8, ! (new BerkeleyDB::Unknown -Filename => $Dfile); 
}

# check the interface to a Hash database

{
    my $lex = new LexFile $Dfile ;

    # create a hash database
    ok 9, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				    -Flags    => DB_CREATE ;

    # Add a few k/v pairs
    my $value ;
    my $status ;
    ok 10, $db->db_put("some key", "some value") == 0  ;
    ok 11, $db->db_put("key", "value") == 0  ;

    # close the database
    undef $db ;

    # now open it with Unknown
    ok 12, $db = new BerkeleyDB::Unknown -Filename => $Dfile; 

    ok 13, $db->type() == DB_HASH ;
    ok 14, $db->db_get("some key", $value) == 0 ;
    ok 15, $value eq "some value" ;
    ok 16, $db->db_get("key", $value) == 0 ;
    ok 17, $value eq "value" ;

    my @array ;
    eval { $db->Tie(\@array)} ;
    ok 18, $@ =~ /^Tie needs a reference to a hash/ ;

    my %hash ;
    $db->Tie(\%hash) ;
    ok 19, $hash{"some key"} eq "some value" ;

}

# check the interface to a Btree database

{
    my $lex = new LexFile $Dfile ;

    # create a hash database
    ok 20, my $db = new BerkeleyDB::Btree -Filename => $Dfile, 
				    -Flags    => DB_CREATE ;

    # Add a few k/v pairs
    my $value ;
    my $status ;
    ok 21, $db->db_put("some key", "some value") == 0  ;
    ok 22, $db->db_put("key", "value") == 0  ;

    # close the database
    undef $db ;

    # now open it with Unknown
    # create a hash database
    ok 23, $db = new BerkeleyDB::Unknown -Filename => $Dfile; 

    ok 24, $db->type() == DB_BTREE ;
    ok 25, $db->db_get("some key", $value) == 0 ;
    ok 26, $value eq "some value" ;
    ok 27, $db->db_get("key", $value) == 0 ;
    ok 28, $value eq "value" ;


    my @array ;
    eval { $db->Tie(\@array)} ;
    ok 29, $@ =~ /^Tie needs a reference to a hash/ ;

    my %hash ;
    $db->Tie(\%hash) ;
    ok 30, $hash{"some key"} eq "some value" ;


}

# check the interface to a Recno database

{
    my $lex = new LexFile $Dfile ;

    # create a recno database
    ok 31, my $db = new BerkeleyDB::Recno -Filename => $Dfile, 
				    -Flags    => DB_CREATE ;

    # Add a few k/v pairs
    my $value ;
    my $status ;
    ok 32, $db->db_put(0, "some value") == 0  ;
    ok 33, $db->db_put(1, "value") == 0  ;

    # close the database
    undef $db ;

    # now open it with Unknown
    # create a hash database
    ok 34, $db = new BerkeleyDB::Unknown -Filename => $Dfile; 

    ok 35, $db->type() == DB_RECNO ;
    ok 36, $db->db_get(0, $value) == 0 ;
    ok 37, $value eq "some value" ;
    ok 38, $db->db_get(1, $value) == 0 ;
    ok 39, $value eq "value" ;


    my %hash ;
    eval { $db->Tie(\%hash)} ;
    ok 40, $@ =~ /^Tie needs a reference to an array/ ;

    my @array ;
    $db->Tie(\@array) ;
    ok 41, $array[1] eq "value" ;


}

# check i/f to text
