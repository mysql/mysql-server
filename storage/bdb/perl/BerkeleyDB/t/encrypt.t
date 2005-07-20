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
use t::util ;

BEGIN
{
    if ($BerkeleyDB::db_version < 4.1) {
        print "1..0 # Skip: this needs Berkeley DB 4.1.x or better\n" ;
        exit 0 ;
    }

    # Is encryption available?
    my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => {Password => "abc",
	                  Flags    => DB_ENCRYPT_AES
	                 };

    if ($BerkeleyDB::Error =~ /Operation not supported/)
    {
        print "1..0 # Skip: encryption support not present\n" ;
        exit 0 ;
    }
}     

umask(0);

print "1..80\n";        

{    
    eval
    {
        my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => 1,
             -Flags => DB_CREATE ;
     };
     ok 1, $@ =~ /^Encrypt parameter must be a hash reference at/;

    eval
    {
        my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => {},
             -Flags => DB_CREATE ;
     };
     ok 2, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => {Password => "fred"},
             -Flags => DB_CREATE ;
     };
     ok 3, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => {Flags => 1},
             -Flags => DB_CREATE ;
     };
     ok 4, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Env @StdErrFile,
             -Encrypt => {Fred => 1},
             -Flags => DB_CREATE ;
     };
     ok 5, $@ =~ /^\Qunknown key value(s) Fred at/;

}

{
    # new BerkeleyDB::Env -Encrypt =>

    # create an environment with a Home
    my $home = "./fred" ;
    #mkdir $home;
    ok 6, my $lexD = new LexDir($home) ;
    ok 7, my $env = new BerkeleyDB::Env @StdErrFile,
             -Home => $home,
             -Encrypt => {Password => "abc",
	                  Flags    => DB_ENCRYPT_AES
	                 },
             -Flags => DB_CREATE | DB_INIT_MPOOL ;



    my $Dfile = "abc.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 8, my $db = new BerkeleyDB::Hash -Filename => $Dfile, 
	                             -Env         => $env,
				     -Flags       => DB_CREATE, 
				     -Property    => DB_ENCRYPT ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 9, $ret == 0 ;

    # check there are three records
    ok 10, countRecords($db) == 3 ;

    undef $db;

    # once the database is created, do not need to specify DB_ENCRYPT
    ok 11, my $db1 = new BerkeleyDB::Hash -Filename => $Dfile, 
	                              -Env      => $env,
				      -Flags    => DB_CREATE ;
    $v = '';				      
    ok 12, ! $db1->db_get("red", $v) ;
    ok 13, $v eq $data{"red"},
    undef $db1;
    undef $env;

    # open a database without specifying encryption
    ok 14,  ! new BerkeleyDB::Hash -Filename => "$home/$Dfile"; 

    ok 15,  ! new BerkeleyDB::Env 
             -Home => $home,
             -Encrypt => {Password => "def",
	                  Flags    => DB_ENCRYPT_AES
	                 },
             -Flags => DB_CREATE | DB_INIT_MPOOL ;
}

{    
    eval
    {
        my $env = new BerkeleyDB::Hash 
             -Encrypt => 1,
             -Flags => DB_CREATE ;
     };
     ok 16, $@ =~ /^Encrypt parameter must be a hash reference at/;

    eval
    {
        my $env = new BerkeleyDB::Hash 
             -Encrypt => {},
             -Flags => DB_CREATE ;
     };
     ok 17, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Hash 
             -Encrypt => {Password => "fred"},
             -Flags => DB_CREATE ;
     };
     ok 18, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Hash 
             -Encrypt => {Flags => 1},
             -Flags => DB_CREATE ;
     };
     ok 19, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Hash 
             -Encrypt => {Fred => 1},
             -Flags => DB_CREATE ;
     };
     ok 20, $@ =~ /^\Qunknown key value(s) Fred at/;

}

{    
    eval
    {
        my $env = new BerkeleyDB::Btree 
             -Encrypt => 1,
             -Flags => DB_CREATE ;
     };
     ok 21, $@ =~ /^Encrypt parameter must be a hash reference at/;

    eval
    {
        my $env = new BerkeleyDB::Btree 
             -Encrypt => {},
             -Flags => DB_CREATE ;
     };
     ok 22, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Btree 
             -Encrypt => {Password => "fred"},
             -Flags => DB_CREATE ;
     };
     ok 23, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Btree 
             -Encrypt => {Flags => 1},
             -Flags => DB_CREATE ;
     };
     ok 24, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Btree 
             -Encrypt => {Fred => 1},
             -Flags => DB_CREATE ;
     };
     ok 25, $@ =~ /^\Qunknown key value(s) Fred at/;

}

{    
    eval
    {
        my $env = new BerkeleyDB::Queue 
             -Encrypt => 1,
             -Flags => DB_CREATE ;
     };
     ok 26, $@ =~ /^Encrypt parameter must be a hash reference at/;

    eval
    {
        my $env = new BerkeleyDB::Queue 
             -Encrypt => {},
             -Flags => DB_CREATE ;
     };
     ok 27, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Queue 
             -Encrypt => {Password => "fred"},
             -Flags => DB_CREATE ;
     };
     ok 28, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Queue 
             -Encrypt => {Flags => 1},
             -Flags => DB_CREATE ;
     };
     ok 29, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Queue 
             -Encrypt => {Fred => 1},
             -Flags => DB_CREATE ;
     };
     ok 30, $@ =~ /^\Qunknown key value(s) Fred at/;

}

{    
    eval
    {
        my $env = new BerkeleyDB::Recno 
             -Encrypt => 1,
             -Flags => DB_CREATE ;
     };
     ok 31, $@ =~ /^Encrypt parameter must be a hash reference at/;

    eval
    {
        my $env = new BerkeleyDB::Recno 
             -Encrypt => {},
             -Flags => DB_CREATE ;
     };
     ok 32, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Recno 
             -Encrypt => {Password => "fred"},
             -Flags => DB_CREATE ;
     };
     ok 33, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Recno 
             -Encrypt => {Flags => 1},
             -Flags => DB_CREATE ;
     };
     ok 34, $@ =~ /^Must specify Password and Flags with Encrypt parameter at/;

    eval
    {
        my $env = new BerkeleyDB::Recno 
             -Encrypt => {Fred => 1},
             -Flags => DB_CREATE ;
     };
     ok 35, $@ =~ /^\Qunknown key value(s) Fred at/;

}


{
    # new BerkeleyDB::Hash -Encrypt =>

    my $Dfile = "abcd.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 36, my $db = new BerkeleyDB::Hash 
                           -Filename => $Dfile, 
		           -Flags    => DB_CREATE, 
                           -Encrypt  => {Password => "beta",
	                                 Flags    => DB_ENCRYPT_AES
	                                },
		           -Property => DB_ENCRYPT ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 37, $ret == 0 ;

    # check there are three records
    ok 38, countRecords($db) == 3 ;

    undef $db;

    # attempt to open a database without specifying encryption
    ok 39, ! new BerkeleyDB::Hash -Filename => $Dfile, 
				      -Flags    => DB_CREATE ;


    # try opening with the wrong password				      
    ok 40, ! new BerkeleyDB::Hash -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "def",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    # read the encrypted data				      
    ok 41, my $db1 = new BerkeleyDB::Hash -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "beta",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    $v = '';				      
    ok 42, ! $db1->db_get("red", $v) ;
    ok 43, $v eq $data{"red"};
    # check there are three records
    ok 44, countRecords($db1) == 3 ;
    undef $db1;
}

{
    # new BerkeleyDB::Btree -Encrypt =>

    my $Dfile = "abcd.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 45, my $db = new BerkeleyDB::Btree 
                           -Filename => $Dfile, 
		           -Flags    => DB_CREATE, 
                           -Encrypt  => {Password => "beta",
	                                 Flags    => DB_ENCRYPT_AES
	                                },
		           -Property => DB_ENCRYPT ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 46, $ret == 0 ;

    # check there are three records
    ok 47, countRecords($db) == 3 ;

    undef $db;

    # attempt to open a database without specifying encryption
    ok 48, ! new BerkeleyDB::Btree -Filename => $Dfile, 
				      -Flags    => DB_CREATE ;


    # try opening with the wrong password				      
    ok 49, ! new BerkeleyDB::Btree -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "def",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    # read the encrypted data				      
    ok 50, my $db1 = new BerkeleyDB::Btree -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "beta",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    $v = '';				      
    ok 51, ! $db1->db_get("red", $v) ;
    ok 52, $v eq $data{"red"};
    # check there are three records
    ok 53, countRecords($db1) == 3 ;
    undef $db1;
}

{
    # new BerkeleyDB::Queue -Encrypt =>

    my $Dfile = "abcd.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 54, my $db = new BerkeleyDB::Queue 
                           -Filename => $Dfile, 
                           -Len      => 5,
                           -Pad      => "x",
		           -Flags    => DB_CREATE, 
                           -Encrypt  => {Password => "beta",
	                                 Flags    => DB_ENCRYPT_AES
	                                },
		           -Property => DB_ENCRYPT ;

    # create some data
    my %data =  (
		1	=> 2,
		2	=> "house",
		3	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 55, $ret == 0 ;

    # check there are three records
    ok 56, countRecords($db) == 3 ;

    undef $db;

    # attempt to open a database without specifying encryption
    ok 57, ! new BerkeleyDB::Queue -Filename => $Dfile, 
                                   -Len      => 5,
                                   -Pad      => "x",
				   -Flags    => DB_CREATE ;


    # try opening with the wrong password				      
    ok 58, ! new BerkeleyDB::Queue -Filename => $Dfile, 
                                   -Len      => 5,
                                   -Pad      => "x",
                                   -Encrypt => {Password => "def",
	                                        Flags    => DB_ENCRYPT_AES
	                                       },
		                   -Property    => DB_ENCRYPT ;


    # read the encrypted data				      
    ok 59, my $db1 = new BerkeleyDB::Queue -Filename => $Dfile, 
                                           -Len      => 5,
                                           -Pad      => "x",
                                           -Encrypt => {Password => "beta",
	                                        Flags    => DB_ENCRYPT_AES
	                                       },
		                           -Property    => DB_ENCRYPT ;


    $v = '';				      
    ok 60, ! $db1->db_get(3, $v) ;
    ok 61, $v eq fillout($data{3}, 5, 'x');
    # check there are three records
    ok 62, countRecords($db1) == 3 ;
    undef $db1;
}

{
    # new BerkeleyDB::Recno -Encrypt =>

    my $Dfile = "abcd.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 63, my $db = new BerkeleyDB::Recno 
                           -Filename => $Dfile, 
		           -Flags    => DB_CREATE, 
                           -Encrypt  => {Password => "beta",
	                                 Flags    => DB_ENCRYPT_AES
	                                },
		           -Property => DB_ENCRYPT ;

    # create some data
    my %data =  (
		1	=> 2,
		2	=> "house",
		3	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 64, $ret == 0 ;

    # check there are three records
    ok 65, countRecords($db) == 3 ;

    undef $db;

    # attempt to open a database without specifying encryption
    ok 66, ! new BerkeleyDB::Recno -Filename => $Dfile, 
				      -Flags    => DB_CREATE ;


    # try opening with the wrong password				      
    ok 67, ! new BerkeleyDB::Recno -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "def",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    # read the encrypted data				      
    ok 68, my $db1 = new BerkeleyDB::Recno -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "beta",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    $v = '';				      
    ok 69, ! $db1->db_get(3, $v) ;
    ok 70, $v eq $data{3};
    # check there are three records
    ok 71, countRecords($db1) == 3 ;
    undef $db1;
}

{
    # new BerkeleyDB::Unknown -Encrypt =>

    my $Dfile = "abcd.enc";
    my $lex = new LexFile $Dfile ;
    my %hash ;
    my ($k, $v) ;
    ok 72, my $db = new BerkeleyDB::Hash 
                           -Filename => $Dfile, 
		           -Flags    => DB_CREATE, 
                           -Encrypt  => {Password => "beta",
	                                 Flags    => DB_ENCRYPT_AES
	                                },
		           -Property => DB_ENCRYPT ;

    # create some data
    my %data =  (
		"red"	=> 2,
		"green"	=> "house",
		"blue"	=> "sea",
		) ;

    my $ret = 0 ;
    while (($k, $v) = each %data) {
        $ret += $db->db_put($k, $v) ;
    }
    ok 73, $ret == 0 ;

    # check there are three records
    ok 74, countRecords($db) == 3 ;

    undef $db;

    # attempt to open a database without specifying encryption
    ok 75, ! new BerkeleyDB::Unknown -Filename => $Dfile, 
				      -Flags    => DB_CREATE ;


    # try opening with the wrong password				      
    ok 76, ! new BerkeleyDB::Unknown -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "def",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    # read the encrypted data				      
    ok 77, my $db1 = new BerkeleyDB::Unknown -Filename => $Dfile, 
                           -Filename => $Dfile, 
                           -Encrypt => {Password => "beta",
	                                Flags    => DB_ENCRYPT_AES
	                               },
		           -Property    => DB_ENCRYPT ;


    $v = '';				      
    ok 78, ! $db1->db_get("red", $v) ;
    ok 79, $v eq $data{"red"};
    # check there are three records
    ok 80, countRecords($db1) == 3 ;
    undef $db1;
}

