#!./perl -w

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

use BerkeleyDB; 
use File::Path qw(rmtree);

BEGIN 
{
    if ($BerkeleyDB::db_version < 3) {
	print "1..0 # Skipping test, this needs Berkeley DB 3.x or better\n" ;
	exit 0 ;
    }
}

print "1..43\n";

my %DB_errors = (
	'DB_INCOMPLETE'	=> "DB_INCOMPLETE: Sync was unable to complete",
	'DB_KEYEMPTY'	=> "DB_KEYEMPTY: Non-existent key/data pair",
	'DB_KEYEXIST'	=> "DB_KEYEXIST: Key/data pair already exists",
	'DB_LOCK_DEADLOCK' => "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock",
	'DB_LOCK_NOTGRANTED' => "DB_LOCK_NOTGRANTED: Lock not granted",
	'DB_NOTFOUND'	=> "DB_NOTFOUND: No matching key/data pair found",
	'DB_OLD_VERSION'	=> "DB_OLDVERSION: Database requires a version upgrade",
	'DB_RUNRECOVERY'	=> "DB_RUNRECOVERY: Fatal error, run database recovery",
	) ;

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

sub addData
{
    my $db = shift ;
    my @data = @_ ;
    die "addData odd data\n" unless @data /2 != 0 ;
    my ($k, $v) ;
    my $ret = 0 ;
    while (@data) {
        $k = shift @data ;
        $v = shift @data ;
        $ret += $db->db_put($k, $v) ;
    }

    return ($ret == 0) ;
}

my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;

# Berkeley DB 3.x specific functionality

# Check for invalid parameters
{
    # Check for invalid parameters
    my $db ;
    eval ' BerkeleyDB::db_remove  -Stupid => 3 ; ' ;
    ok 1, $@ =~ /unknown key value\(s\) Stupid/  ;

    eval ' BerkeleyDB::db_remove -Bad => 2, -Filename => "fred", -Stupid => 3; ' ;
    ok 2, $@ =~ /unknown key value\(s\) (Bad |Stupid ){2}/  ;

    eval ' BerkeleyDB::db_remove -Filename => "a", -Env => 2 ' ;
    ok 3, $@ =~ /^Env not of type BerkeleyDB::Env/ ;

    eval ' BerkeleyDB::db_remove -Subname => "a"' ;
    ok 4, $@ =~ /^Must specify a filename/ ;

    my $obj = bless [], "main" ;
    eval ' BerkeleyDB::db_remove -Filename => "x", -Env => $obj ' ;
    ok 5, $@ =~ /^Env not of type BerkeleyDB::Env/ ;
}

{
    # subdatabases

    # opening a subdatabse in an exsiting database that doesn't have
    # subdatabases at all should fail

    my $lex = new LexFile $Dfile ;

    ok 6, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				        -Flags    => DB_CREATE ;

    # Add a k/v pair
    my %data = qw(
    			red	sky
			blue	sea
			black	heart
			yellow	belley
			green	grass
    		) ;

    ok 7, addData($db, %data) ;

    undef $db ;

    $db = new BerkeleyDB::Hash -Filename => $Dfile, 
			       -Subname  => "fred" ;
    ok 8, ! $db ;				    

    ok 9, -e $Dfile ;
    ok 10, ! BerkeleyDB::db_remove(-Filename => $Dfile)  ;
}

{
    # subdatabases

    # opening a subdatabse in an exsiting database that does have
    # subdatabases at all, but not this one

    my $lex = new LexFile $Dfile ;

    ok 11, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				         -Subname  => "fred" ,
				         -Flags    => DB_CREATE ;

    # Add a k/v pair
    my %data = qw(
    			red	sky
			blue	sea
			black	heart
			yellow	belley
			green	grass
    		) ;

    ok 12, addData($db, %data) ;

    undef $db ;

    $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				    -Subname  => "joe" ;

    ok 13, !$db ;				    

}

{
    # subdatabases

    my $lex = new LexFile $Dfile ;

    ok 14, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				        -Subname  => "fred" ,
				        -Flags    => DB_CREATE ;

    # Add a k/v pair
    my %data = qw(
    			red	sky
			blue	sea
			black	heart
			yellow	belley
			green	grass
    		) ;

    ok 15, addData($db, %data) ;

    undef $db ;

    ok 16, $db = new BerkeleyDB::Hash -Filename => $Dfile, 
				    -Subname  => "fred" ;

    ok 17, my $cursor = $db->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $status ;
    while (($status = $cursor->c_get($k, $v, DB_NEXT)) == 0) {
        if ($data{$k} eq $v) {
	    delete $data{$k} ;
	}
    }
    ok 18, $status == DB_NOTFOUND ;
    ok 19, keys %data == 0 ;
}

{
    # subdatabases

    # opening a database with multiple subdatabases - handle should be a list
    # of the subdatabase names

    my $lex = new LexFile $Dfile ;
  
    ok 20, my $db1 = new BerkeleyDB::Hash -Filename => $Dfile, 
				        -Subname  => "fred" ,
				        -Flags    => DB_CREATE ;

    ok 21, my $db2 = new BerkeleyDB::Btree -Filename => $Dfile, 
				        -Subname  => "joe" ,
				        -Flags    => DB_CREATE ;

    # Add a k/v pair
    my %data = qw(
    			red	sky
			blue	sea
			black	heart
			yellow	belley
			green	grass
    		) ;

    ok 22, addData($db1, %data) ;
    ok 23, addData($db2, %data) ;

    undef $db1 ;
    undef $db2 ;
  
    ok 24, my $db = new BerkeleyDB::Unknown -Filename => $Dfile ,
				         -Flags    => DB_RDONLY ;

    #my $type = $db->type() ; print "type $type\n" ;
    ok 25, my $cursor = $db->db_cursor() ;
    my ($k, $v) = ("", "") ;
    my $status ;
    my @dbnames = () ;
    while (($status = $cursor->c_get($k, $v, DB_NEXT)) == 0) {
        push @dbnames, $k ;
    }
    ok 26, $status == DB_NOTFOUND ;
    ok 27, join(",", sort @dbnames) eq "fred,joe" ;
    undef $db ;

    ok 28, BerkeleyDB::db_remove(-Filename => $Dfile, -Subname => "harry") != 0;
    ok 29, BerkeleyDB::db_remove(-Filename => $Dfile, -Subname => "fred") == 0 ;
    
    # should only be one subdatabase
    ok 30, $db = new BerkeleyDB::Unknown -Filename => $Dfile ,
				         -Flags    => DB_RDONLY ;

    ok 31, $cursor = $db->db_cursor() ;
    @dbnames = () ;
    while (($status = $cursor->c_get($k, $v, DB_NEXT)) == 0) {
        push @dbnames, $k ;
    }
    ok 32, $status == DB_NOTFOUND ;
    ok 33, join(",", sort @dbnames) eq "joe" ;
    undef $db ;

    # can't delete an already deleted subdatabase
    ok 34, BerkeleyDB::db_remove(-Filename => $Dfile, -Subname => "fred") != 0;
    
    ok 35, BerkeleyDB::db_remove(-Filename => $Dfile, -Subname => "joe") == 0 ;
    
    # should only be one subdatabase
    ok 36, $db = new BerkeleyDB::Unknown -Filename => $Dfile ,
				         -Flags    => DB_RDONLY ;

    ok 37, $cursor = $db->db_cursor() ;
    @dbnames = () ;
    while (($status = $cursor->c_get($k, $v, DB_NEXT)) == 0) {
        push @dbnames, $k ;
    }
    ok 38, $status == DB_NOTFOUND ;
    ok 39, @dbnames == 0 ;
    undef $db ;

    ok 40, -e $Dfile ;
    ok 41, BerkeleyDB::db_remove(-Filename => $Dfile)  == 0 ;
    ok 42, ! -e $Dfile ;
    ok 43, BerkeleyDB::db_remove(-Filename => $Dfile) != 0 ;
}

# db_remove with env
