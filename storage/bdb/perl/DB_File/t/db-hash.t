#!./perl 

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}
 
use warnings;
use strict;
use Config;
 
BEGIN {
    if(-d "lib" && -f "TEST") {
        if ($Config{'extensions'} !~ /\bDB_File\b/ ) {
            print "1..0 # Skip: DB_File was not built\n";
            exit 0;
        }
    }
}

use DB_File; 
use Fcntl;

print "1..143\n";

sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}

{
    package Redirect ;
    use Symbol ;

    sub new
    {
        my $class = shift ;
        my $filename = shift ;
	my $fh = gensym ;
	open ($fh, ">$filename") || die "Cannot open $filename: $!" ;
	my $real_stdout = select($fh) ;
	return bless [$fh, $real_stdout ] ;

    }
    sub DESTROY
    {
        my $self = shift ;
	close $self->[0] ;
	select($self->[1]) ;
    }
}

sub docat_del
{ 
    my $file = shift;
    local $/ = undef;
    open(CAT,$file) || die "Cannot open $file: $!";
    my $result = <CAT>;
    close(CAT);
    $result = normalise($result) ;
    unlink $file ;
    return $result;
}   

sub normalise
{
    my $data = shift ;
    $data =~ s#\r\n#\n#g 
        if $^O eq 'cygwin' ;
    return $data ;
}

sub safeUntie
{
    my $hashref = shift ;
    my $no_inner = 1;
    local $SIG{__WARN__} = sub {-- $no_inner } ;
    untie %$hashref;
    return $no_inner;
}


my $Dfile = "dbhash.tmp";
my $Dfile2 = "dbhash2.tmp";
my $null_keys_allowed = ($DB_File::db_ver < 2.004010 
				|| $DB_File::db_ver >= 3.1 );

unlink $Dfile;

umask(0);

# Check the interface to HASHINFO

my $dbh = new DB_File::HASHINFO ;

ok(1, ! defined $dbh->{bsize}) ;
ok(2, ! defined $dbh->{ffactor}) ;
ok(3, ! defined $dbh->{nelem}) ;
ok(4, ! defined $dbh->{cachesize}) ;
ok(5, ! defined $dbh->{hash}) ;
ok(6, ! defined $dbh->{lorder}) ;

$dbh->{bsize} = 3000 ;
ok(7, $dbh->{bsize} == 3000 );

$dbh->{ffactor} = 9000 ;
ok(8, $dbh->{ffactor} == 9000 );

$dbh->{nelem} = 400 ;
ok(9, $dbh->{nelem} == 400 );

$dbh->{cachesize} = 65 ;
ok(10, $dbh->{cachesize} == 65 );

my $some_sub = sub {} ;
$dbh->{hash} = $some_sub;
ok(11, $dbh->{hash} eq $some_sub );

$dbh->{lorder} = 1234 ;
ok(12, $dbh->{lorder} == 1234 );

# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
ok(13, $@ =~ /^DB_File::HASHINFO::STORE - Unknown element 'fred' at/ );
eval 'my $q = $dbh->{fred}' ;
ok(14, $@ =~ /^DB_File::HASHINFO::FETCH - Unknown element 'fred' at/ );


# Now check the interface to HASH
my ($X, %h);
ok(15, $X = tie(%h, 'DB_File',$Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );
die "Could not tie: $!" unless $X;

my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);

my %noMode = map { $_, 1} qw( amigaos MSWin32 NetWare cygwin ) ;

ok(16, ($mode & 0777) == (($^O eq 'os2' || $^O eq 'MacOS') ? 0666 : 0640) ||
   $noMode{$^O} );

my ($key, $value, $i);
while (($key,$value) = each(%h)) {
    $i++;
}
ok(17, !$i );

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
ok(18, $h{'abc'} eq 'ABC' );
ok(19, !defined $h{'jimmy'} );
ok(20, !exists $h{'jimmy'} );
ok(21, exists $h{'abc'} );

$h{'def'} = 'DEF';
$h{'jkl','mno'} = "JKL\034MNO";
$h{'a',2,3,4,5} = join("\034",'A',2,3,4,5);
$h{'a'} = 'A';

#$h{'b'} = 'B';
$X->STORE('b', 'B') ;

$h{'c'} = 'C';

#$h{'d'} = 'D';
$X->put('d', 'D') ;

$h{'e'} = 'E';
$h{'f'} = 'F';
$h{'g'} = 'X';
$h{'h'} = 'H';
$h{'i'} = 'I';

$h{'goner2'} = 'snork';
delete $h{'goner2'};


# IMPORTANT - $X must be undefined before the untie otherwise the
#             underlying DB close routine will not get called.
undef $X ;
untie(%h);


# tie to the same file again, do not supply a type - should default to HASH
ok(22, $X = tie(%h,'DB_File',$Dfile, O_RDWR, 0640) );

# Modify an entry from the previous tie
$h{'g'} = 'G';

$h{'j'} = 'J';
$h{'k'} = 'K';
$h{'l'} = 'L';
$h{'m'} = 'M';
$h{'n'} = 'N';
$h{'o'} = 'O';
$h{'p'} = 'P';
$h{'q'} = 'Q';
$h{'r'} = 'R';
$h{'s'} = 'S';
$h{'t'} = 'T';
$h{'u'} = 'U';
$h{'v'} = 'V';
$h{'w'} = 'W';
$h{'x'} = 'X';
$h{'y'} = 'Y';
$h{'z'} = 'Z';

$h{'goner3'} = 'snork';

delete $h{'goner1'};
$X->DELETE('goner3');

my @keys = keys(%h);
my @values = values(%h);

ok(23, $#keys == 29 && $#values == 29) ;

$i = 0 ;
while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

ok(24, $i == 30) ;

@keys = ('blurfl', keys(%h), 'dyick');
ok(25, $#keys == 31) ;

$h{'foo'} = '';
ok(26, $h{'foo'} eq '' );

# Berkeley DB from version 2.4.10 to 3.0 does not allow null keys.
# This feature was reenabled in version 3.1 of Berkeley DB.
my $result = 0 ;
if ($null_keys_allowed) {
    $h{''} = 'bar';
    $result = ( $h{''} eq 'bar' );
}
else
  { $result = 1 }
ok(27, $result) ;

# check cache overflow and numeric keys and contents
my $ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
ok(28, $ok );

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
ok(29, $size > 0 );

@h{0..200} = 200..400;
my @foo = @h{0..200};
ok(30, join(':',200..400) eq join(':',@foo) );


# Now check all the non-tie specific stuff

# Check NOOVERWRITE will make put fail when attempting to overwrite
# an existing record.
 
my $status = $X->put( 'x', 'newvalue', R_NOOVERWRITE) ;
ok(31, $status == 1 );
 
# check that the value of the key 'x' has not been changed by the 
# previous test
ok(32, $h{'x'} eq 'X' );

# standard put
$status = $X->put('key', 'value') ;
ok(33, $status == 0 );

#check that previous put can be retrieved
$value = 0 ;
$status = $X->get('key', $value) ;
ok(34, $status == 0 );
ok(35, $value eq 'value' );

# Attempting to delete an existing key should work

$status = $X->del('q') ;
ok(36, $status == 0 );

# Make sure that the key deleted, cannot be retrieved
{
    no warnings 'uninitialized' ;
    ok(37, $h{'q'} eq undef );
}

# Attempting to delete a non-existant key should fail

$status = $X->del('joe') ;
ok(38, $status == 1 );

# Check the get interface

# First a non-existing key
$status = $X->get('aaaa', $value) ;
ok(39, $status == 1 );

# Next an existing key
$status = $X->get('a', $value) ;
ok(40, $status == 0 );
ok(41, $value eq 'A' );

# seq
# ###

# ditto, but use put to replace the key/value pair.

# use seq to walk backwards through a file - check that this reversed is

# check seq FIRST/LAST

# sync
# ####

$status = $X->sync ;
ok(42, $status == 0 );


# fd
# ##

$status = $X->fd ;
ok(43, $status != 0 );

undef $X ;
untie %h ;

unlink $Dfile;

# clear
# #####

ok(44, tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );
foreach (1 .. 10)
  { $h{$_} = $_ * 100 }

# check that there are 10 elements in the hash
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(45, $i == 10);

# now clear the hash
%h = () ;

# check it is empty
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(46, $i == 0);

untie %h ;
unlink $Dfile ;


# Now try an in memory file
ok(47, $X = tie(%h, 'DB_File',undef, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

# fd with an in memory file should return fail
$status = $X->fd ;
ok(48, $status == -1 );

undef $X ;
untie %h ;

{
    # check ability to override the default hashing
    my %x ;
    my $filename = "xyz" ;
    my $hi = new DB_File::HASHINFO ;
    $::count = 0 ;
    $hi->{hash} = sub { ++$::count ; length $_[0] } ;
    ok(49, tie %x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $hi ) ;
    $h{"abc"} = 123 ;
    ok(50, $h{"abc"} == 123) ;
    untie %x ;
    unlink $filename ;
    ok(51, $::count >0) ;
}

{
    # check that attempting to tie an array to a DB_HASH will fail

    my $filename = "xyz" ;
    my @x ;
    eval { tie @x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $DB_HASH ; } ;
    ok(52, $@ =~ /^DB_File can only tie an associative array to a DB_HASH database/) ;
    unlink $filename ;
}

{
   # sub-class test

   package Another ;

   use warnings ;
   use strict ;

   open(FILE, ">SubDB.pm") or die "Cannot open SubDB.pm: $!\n" ;
   print FILE <<'EOM' ;

   package SubDB ;

   use warnings ;
   use strict ;
   our (@ISA, @EXPORT);

   require Exporter ;
   use DB_File;
   @ISA=qw(DB_File);
   @EXPORT = @DB_File::EXPORT ;

   sub STORE { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::STORE($key, $value * 2) ;
   }

   sub FETCH { 
	my $self = shift ;
        my $key = shift ;
        $self->SUPER::FETCH($key) - 1 ;
   }

   sub put { 
	my $self = shift ;
        my $key = shift ;
        my $value = shift ;
        $self->SUPER::put($key, $value * 3) ;
   }

   sub get { 
	my $self = shift ;
        $self->SUPER::get($_[0], $_[1]) ;
	$_[1] -= 2 ;
   }

   sub A_new_method
   {
	my $self = shift ;
        my $key = shift ;
        my $value = $self->FETCH($key) ;
	return "[[$value]]" ;
   }

   1 ;
EOM

    close FILE ;

    BEGIN { push @INC, '.'; }             
    eval 'use SubDB ; ';
    main::ok(53, $@ eq "") ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB","dbhash.tmp", O_RDWR|O_CREAT, 0640, $DB_HASH );
	' ;

    main::ok(54, $@ eq "") ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok(55, $@ eq "") ;
    main::ok(56, $ret == 5) ;

    my $value = 0;
    $ret = eval '$X->put("joe", 4) ; $X->get("joe", $value) ; return $value' ;
    main::ok(57, $@ eq "") ;
    main::ok(58, $ret == 10) ;

    $ret = eval ' R_NEXT eq main::R_NEXT ' ;
    main::ok(59, $@ eq "" ) ;
    main::ok(60, $ret == 1) ;

    $ret = eval '$X->A_new_method("joe") ' ;
    main::ok(61, $@ eq "") ;
    main::ok(62, $ret eq "[[11]]") ;

    undef $X;
    untie(%h);
    unlink "SubDB.pm", "dbhash.tmp" ;

}

{
   # DBM Filter tests
   use warnings ;
   use strict ;
   my (%h, $db) ;
   my ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   unlink $Dfile;

   sub checkOutput
   {
       no warnings 'uninitialized';
       my($fk, $sk, $fv, $sv) = @_ ;

       print "# Fetch Key   : expected '$fk' got '$fetch_key'\n" 
           if $fetch_key ne $fk ;
       print "# Fetch Value : expected '$fv' got '$fetch_value'\n" 
           if $fetch_value ne $fv ;
       print "# Store Key   : expected '$sk' got '$store_key'\n" 
           if $store_key ne $sk ;
       print "# Store Value : expected '$sv' got '$store_value'\n" 
           if $store_value ne $sv ;
       print "# \$_          : expected 'original' got '$_'\n" 
           if $_ ne 'original' ;

       return
           $fetch_key   eq $fk && $store_key   eq $sk && 
	   $fetch_value eq $fv && $store_value eq $sv &&
	   $_ eq 'original' ;
   }
   
   ok(63, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

   $db->filter_fetch_key   (sub { $fetch_key = $_ }) ;
   $db->filter_store_key   (sub { $store_key = $_ }) ;
   $db->filter_fetch_value (sub { $fetch_value = $_}) ;
   $db->filter_store_value (sub { $store_value = $_ }) ;

   $_ = "original" ;

   $h{"fred"} = "joe" ;
   #                   fk   sk     fv   sv
   ok(64, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(65, $h{"fred"} eq "joe");
   #                   fk    sk     fv    sv
   ok(66, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   my ($k, $v) ;
   $k = 'fred';
   ok(67, ! $db->seq($k, $v, R_FIRST) ) ;
   ok(68, $k eq "fred") ;
   ok(69, $v eq "joe") ;
   #                    fk     sk  fv  sv
   ok(70, checkOutput( "fred", "fred", "joe", "")) ;

   # replace the filters, but remember the previous set
   my ($old_fk) = $db->filter_fetch_key   
   			(sub { $_ = uc $_ ; $fetch_key = $_ }) ;
   my ($old_sk) = $db->filter_store_key   
   			(sub { $_ = lc $_ ; $store_key = $_ }) ;
   my ($old_fv) = $db->filter_fetch_value 
   			(sub { $_ = "[$_]"; $fetch_value = $_ }) ;
   my ($old_sv) = $db->filter_store_value 
   			(sub { s/o/x/g; $store_value = $_ }) ;
   
   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"Fred"} = "Joe" ;
   #                   fk   sk     fv    sv
   ok(71, checkOutput( "", "fred", "", "Jxe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(72, $h{"Fred"} eq "[Jxe]");
   #                   fk   sk     fv    sv
   ok(73, checkOutput( "", "fred", "[Jxe]", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $k = 'Fred'; $v ='';
   ok(74, ! $db->seq($k, $v, R_FIRST) ) ;
   ok(75, $k eq "FRED") ;
   ok(76, $v eq "[Jxe]") ;
   #                   fk   sk     fv    sv
   ok(77, checkOutput( "FRED", "fred", "[Jxe]", "")) ;

   # put the original filters back
   $db->filter_fetch_key   ($old_fk);
   $db->filter_store_key   ($old_sk);
   $db->filter_fetch_value ($old_fv);
   $db->filter_store_value ($old_sv);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(78, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(79, $h{"fred"} eq "joe");
   ok(80, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   #ok(77, $db->FIRSTKEY() eq "fred") ;
   $k = 'fred';
   ok(81, ! $db->seq($k, $v, R_FIRST) ) ;
   ok(82, $k eq "fred") ;
   ok(83, $v eq "joe") ;
   #                   fk   sk     fv    sv
   ok(84, checkOutput( "fred", "fred", "joe", "")) ;

   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(85, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(86, $h{"fred"} eq "joe");
   ok(87, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $k = 'fred';
   ok(88, ! $db->seq($k, $v, R_FIRST) ) ;
   ok(89, $k eq "fred") ;
   ok(90, $v eq "joe") ;
   ok(91, checkOutput( "", "", "", "")) ;

   undef $db ;
   untie %h;
   unlink $Dfile;
}

{    
    # DBM Filter with a closure

    use warnings ;
    use strict ;
    my (%h, $db) ;

    unlink $Dfile;
    ok(92, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

    my %result = () ;

    sub Closure
    {
        my ($name) = @_ ;
	my $count = 0 ;
	my @kept = () ;

	return sub { ++$count ; 
		     push @kept, $_ ; 
		     $result{$name} = "$name - $count: [@kept]" ;
		   }
    }

    $db->filter_store_key(Closure("store key")) ;
    $db->filter_store_value(Closure("store value")) ;
    $db->filter_fetch_key(Closure("fetch key")) ;
    $db->filter_fetch_value(Closure("fetch value")) ;

    $_ = "original" ;

    $h{"fred"} = "joe" ;
    ok(93, $result{"store key"} eq "store key - 1: [fred]");
    ok(94, $result{"store value"} eq "store value - 1: [joe]");
    ok(95, ! defined $result{"fetch key"} );
    ok(96, ! defined $result{"fetch value"} );
    ok(97, $_ eq "original") ;

    ok(98, $db->FIRSTKEY() eq "fred") ;
    ok(99, $result{"store key"} eq "store key - 1: [fred]");
    ok(100, $result{"store value"} eq "store value - 1: [joe]");
    ok(101, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(102, ! defined $result{"fetch value"} );
    ok(103, $_ eq "original") ;

    $h{"jim"}  = "john" ;
    ok(104, $result{"store key"} eq "store key - 2: [fred jim]");
    ok(105, $result{"store value"} eq "store value - 2: [joe john]");
    ok(106, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(107, ! defined $result{"fetch value"} );
    ok(108, $_ eq "original") ;

    ok(109, $h{"fred"} eq "joe");
    ok(110, $result{"store key"} eq "store key - 3: [fred jim fred]");
    ok(111, $result{"store value"} eq "store value - 2: [joe john]");
    ok(112, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(113, $result{"fetch value"} eq "fetch value - 1: [joe]");
    ok(114, $_ eq "original") ;

    undef $db ;
    untie %h;
    unlink $Dfile;
}		

{
   # DBM Filter recursion detection
   use warnings ;
   use strict ;
   my (%h, $db) ;
   unlink $Dfile;

   ok(115, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

   $db->filter_store_key (sub { $_ = $h{$_} }) ;

   eval '$h{1} = 1234' ;
   ok(116, $@ =~ /^recursion detected in filter_store_key at/ );
   
   undef $db ;
   untie %h;
   unlink $Dfile;
}


{
   # Examples from the POD

  my $file = "xyzt" ;
  {
    my $redirect = new Redirect $file ;

    use warnings FATAL => qw(all);
    use strict ;
    use DB_File ;
    our (%h, $k, $v);

    unlink "fruit" ;
    tie %h, "DB_File", "fruit", O_RDWR|O_CREAT, 0640, $DB_HASH 
        or die "Cannot open file 'fruit': $!\n";

    # Add a few key/value pairs to the file
    $h{"apple"} = "red" ;
    $h{"orange"} = "orange" ;
    $h{"banana"} = "yellow" ;
    $h{"tomato"} = "red" ;

    # Check for existence of a key
    print "Banana Exists\n\n" if $h{"banana"} ;

    # Delete a key/value pair.
    delete $h{"apple"} ;

    # print the contents of the file
    while (($k, $v) = each %h)
      { print "$k -> $v\n" }

    untie %h ;

    unlink "fruit" ;
  }  

  ok(117, docat_del($file) eq <<'EOM') ;
Banana Exists

orange -> orange
tomato -> red
banana -> yellow
EOM
   
}

{
    # Bug ID 20001013.009
    #
    # test that $hash{KEY} = undef doesn't produce the warning
    #     Use of uninitialized value in null operation 
    use warnings ;
    use strict ;
    use DB_File ;

    unlink $Dfile;
    my %h ;
    my $a = "";
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    
    tie %h, 'DB_File', $Dfile or die "Can't open file: $!\n" ;
    $h{ABC} = undef;
    ok(118, $a eq "") ;
    untie %h ;
    unlink $Dfile;
}

{
    # test that %hash = () doesn't produce the warning
    #     Argument "" isn't numeric in entersub
    use warnings ;
    use strict ;
    use DB_File ;

    unlink $Dfile;
    my %h ;
    my $a = "";
    local $SIG{__WARN__} = sub {$a = $_[0]} ;
    
    tie %h, 'DB_File', $Dfile or die "Can't open file: $!\n" ;
    %h = (); ;
    ok(119, $a eq "") ;
    untie %h ;
    unlink $Dfile;
}

{
    # When iterating over a tied hash using "each", the key passed to FETCH
    # will be recycled and passed to NEXTKEY. If a Source Filter modifies the
    # key in FETCH via a filter_fetch_key method we need to check that the
    # modified key doesn't get passed to NEXTKEY.
    # Also Test "keys" & "values" while we are at it.

    use warnings ;
    use strict ;
    use DB_File ;

    unlink $Dfile;
    my $bad_key = 0 ;
    my %h = () ;
    my $db ;
    ok(120, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );
    $db->filter_fetch_key (sub { $_ =~ s/^Beta_/Alpha_/ if defined $_}) ;
    $db->filter_store_key (sub { $bad_key = 1 if /^Beta_/ ; $_ =~ s/^Alpha_/Beta_/}) ;

    $h{'Alpha_ABC'} = 2 ;
    $h{'Alpha_DEF'} = 5 ;

    ok(121, $h{'Alpha_ABC'} == 2);
    ok(122, $h{'Alpha_DEF'} == 5);

    my ($k, $v) = ("","");
    while (($k, $v) = each %h) {}
    ok(123, $bad_key == 0);

    $bad_key = 0 ;
    foreach $k (keys %h) {}
    ok(124, $bad_key == 0);

    $bad_key = 0 ;
    foreach $v (values %h) {}
    ok(125, $bad_key == 0);

    undef $db ;
    untie %h ;
    unlink $Dfile;
}

{
    # now an error to pass 'hash' a non-code reference
    my $dbh = new DB_File::HASHINFO ;

    eval { $dbh->{hash} = 2 };
    ok(126, $@ =~ /^Key 'hash' not associated with a code reference at/);

}

{
    # recursion detection in hash
    my %hash ;
    unlink $Dfile;
    my $dbh = new DB_File::HASHINFO ;
    $dbh->{hash} = sub { $hash{3} = 4 ; length $_[0] } ;
 
 
    my (%h);
    ok(127, tie(%hash, 'DB_File',$Dfile, O_RDWR|O_CREAT, 0640, $dbh ) );

    eval {	$hash{1} = 2;
    		$hash{4} = 5;
	 };

    ok(128, $@ =~ /^DB_File hash callback: recursion detected/);
    {
        no warnings;
        untie %hash;
    }
    unlink $Dfile;
}

{
    # Check that two hash's don't interact
    my %hash1 ;
    my %hash2 ;
    my $h1_count = 0;
    my $h2_count = 0;
    unlink $Dfile, $Dfile2;
    my $dbh1 = new DB_File::HASHINFO ;
    $dbh1->{hash} = sub { ++ $h1_count ; length $_[0] } ;

    my $dbh2 = new DB_File::HASHINFO ;
    $dbh2->{hash} = sub { ++ $h2_count ; length $_[0] } ;
 
 
 
    my (%h);
    ok(129, tie(%hash1, 'DB_File',$Dfile, O_RDWR|O_CREAT, 0640, $dbh1 ) );
    ok(130, tie(%hash2, 'DB_File',$Dfile2, O_RDWR|O_CREAT, 0640, $dbh2 ) );

    $hash1{DEFG} = 5;
    $hash1{XYZ} = 2;
    $hash1{ABCDE} = 5;

    $hash2{defg} = 5;
    $hash2{xyz} = 2;
    $hash2{abcde} = 5;

    ok(131, $h1_count > 0);
    ok(132, $h1_count == $h2_count);

    ok(133, safeUntie \%hash1);
    ok(134, safeUntie \%hash2);
    unlink $Dfile, $Dfile2;
}

{
    # Passing undef for flags and/or mode when calling tie could cause 
    #     Use of uninitialized value in subroutine entry
    

    my $warn_count = 0 ;
    #local $SIG{__WARN__} = sub { ++ $warn_count };
    my %hash1;
    unlink $Dfile;

    tie %hash1, 'DB_File',$Dfile, undef;
    ok(135, $warn_count == 0);
    $warn_count = 0;
    tie %hash1, 'DB_File',$Dfile, O_RDWR|O_CREAT, undef;
    ok(136, $warn_count == 0);
    tie %hash1, 'DB_File',$Dfile, undef, undef;
    ok(137, $warn_count == 0);
    $warn_count = 0;

    unlink $Dfile;
}

{
   # Check that DBM Filter can cope with read-only $_

   use warnings ;
   use strict ;
   my (%h, $db) ;
   unlink $Dfile;

   ok(138, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_HASH ) );

   $db->filter_fetch_key   (sub { }) ;
   $db->filter_store_key   (sub { }) ;
   $db->filter_fetch_value (sub { }) ;
   $db->filter_store_value (sub { }) ;

   $_ = "original" ;

   $h{"fred"} = "joe" ;
   ok(139, $h{"fred"} eq "joe");

   eval { grep { $h{$_} } (1, 2, 3) };
   ok (140, ! $@);


   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   $h{"fred"} = "joe" ;

   ok(141, $h{"fred"} eq "joe");

   ok(142, $db->FIRSTKEY() eq "fred") ;
   
   eval { grep { $h{$_} } (1, 2, 3) };
   ok (143, ! $@);

   undef $db ;
   untie %h;
   unlink $Dfile;
}

exit ;
