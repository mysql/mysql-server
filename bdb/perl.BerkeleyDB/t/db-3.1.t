#!./perl -w

# ID: %I%, %G%   

use strict ;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}

#use Config;
#
#BEGIN {
#    if(-d "lib" && -f "TEST") {
#        if ($Config{'extensions'} !~ /\bBerkeleyDB\b/ ) {
#            print "1..74\n";
#            exit 0;
#        }
#    }
#}

use BerkeleyDB; 
use File::Path qw(rmtree);

BEGIN
{
    if ($BerkeleyDB::db_version < 3.1) {
        print "1..0 # Skipping test, this needs Berkeley DB 3.1.x or better\n" ;
        exit 0 ;
    }
}     

print "1..25\n";

my %DB_errors = (
    'DB_INCOMPLETE'	=> "DB_INCOMPLETE: Sync was unable to complete",
    'DB_KEYEMPTY'	=> "DB_KEYEMPTY: Non-existent key/data pair",
    'DB_KEYEXIST'	=> "DB_KEYEXIST: Key/data pair already exists",
    'DB_LOCK_DEADLOCK'  => "DB_LOCK_DEADLOCK: Locker killed to resolve a deadlock",
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

my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $Dfile3 = "dbhash3.tmp";
unlink $Dfile;

umask(0) ;



{
    # c_count

    my $lex = new LexFile $Dfile ;
    my %hash ;
    ok 1, my $db = tie %hash, 'BerkeleyDB::Hash', -Filename => $Dfile,
				      -Property  => DB_DUP,
                                      -Flags    => DB_CREATE ;

    $hash{'Wall'} = 'Larry' ;
    $hash{'Wall'} = 'Stone' ;
    $hash{'Smith'} = 'John' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'mouse'} = 'mickey' ;

    ok 2, keys %hash == 6 ;

    # create a cursor
    ok 3, my $cursor = $db->db_cursor() ;

    my $key = "Wall" ;
    my $value ;
    ok 4, $cursor->c_get($key, $value, DB_SET) == 0 ;
    ok 5, $key eq "Wall" && $value eq "Larry" ;

    my $count ;
    ok 6, $cursor->c_count($count) == 0 ;
    ok 7, $count == 4 ;

    $key = "Smith" ;
    ok 8, $cursor->c_get($key, $value, DB_SET) == 0 ;
    ok 9, $key eq "Smith" && $value eq "John" ;

    ok 10, $cursor->c_count($count) == 0 ;
    ok 11, $count == 1 ;


    undef $db ;
    undef $cursor ;
    untie %hash ;

}

{
    # db_key_range

    my $lex = new LexFile $Dfile ;
    my %hash ;
    ok 12, my $db = tie %hash, 'BerkeleyDB::Btree', -Filename => $Dfile,
				      -Property  => DB_DUP,
                                      -Flags    => DB_CREATE ;

    $hash{'Wall'} = 'Larry' ;
    $hash{'Wall'} = 'Stone' ;
    $hash{'Smith'} = 'John' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'Wall'} = 'Brick' ;
    $hash{'mouse'} = 'mickey' ;

    ok 13, keys %hash == 6 ;

    my $key = "Wall" ;
    my ($less, $equal, $greater) ;
    ok 14, $db->db_key_range($key, $less, $equal, $greater) == 0 ;

    ok 15, $less != 0 ;
    ok 16, $equal != 0 ;
    ok 17, $greater != 0 ;

    $key = "Smith" ;
    ok 18, $db->db_key_range($key, $less, $equal, $greater) == 0 ;

    ok 19, $less == 0 ;
    ok 20, $equal != 0 ;
    ok 21, $greater != 0 ;

    $key = "NotThere" ;
    ok 22, $db->db_key_range($key, $less, $equal, $greater) == 0 ;

    ok 23, $less == 0 ;
    ok 24, $equal == 0 ;
    ok 25, $greater == 1 ;

    undef $db ;
    untie %hash ;

}
