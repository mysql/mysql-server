#!./perl -w

use warnings;
use strict;

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
}
 
use Config;
 
BEGIN {
    if(-d "lib" && -f "TEST") {
        if ($Config{'extensions'} !~ /\bDB_File\b/ ) {
            print "1..157\n";
            exit 0;
        }
    }
}

use DB_File; 
use Fcntl;

print "1..157\n";

sub ok
{
    my $no = shift ;
    my $result = shift ;
 
    print "not " unless $result ;
    print "ok $no\n" ;
}

sub lexical
{
    my(@a) = unpack ("C*", $a) ;
    my(@b) = unpack ("C*", $b) ;

    my $len = (@a > @b ? @b : @a) ;
    my $i = 0 ;

    foreach $i ( 0 .. $len -1) {
        return $a[$i] - $b[$i] if $a[$i] != $b[$i] ;
    }

    return @a - @b ;
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

sub docat
{ 
    my $file = shift;
    #local $/ = undef unless wantarray ;
    open(CAT,$file) || die "Cannot open $file: $!";
    my @result = <CAT>;
    close(CAT);
    wantarray ? @result : join("", @result) ;
}   

sub docat_del
{ 
    my $file = shift;
    #local $/ = undef unless wantarray ;
    open(CAT,$file) || die "Cannot open $file: $!";
    my @result = <CAT>;
    close(CAT);
    unlink $file ;
    wantarray ? @result : join("", @result) ;
}   


my $db185mode =  ($DB_File::db_version == 1 && ! $DB_File::db_185_compat) ;
my $null_keys_allowed = ($DB_File::db_ver < 2.004010 
				|| $DB_File::db_ver >= 3.1 );

my $Dfile = "dbbtree.tmp";
unlink $Dfile;

umask(0);

# Check the interface to BTREEINFO

my $dbh = new DB_File::BTREEINFO ;
ok(1, ! defined $dbh->{flags}) ;
ok(2, ! defined $dbh->{cachesize}) ;
ok(3, ! defined $dbh->{psize}) ;
ok(4, ! defined $dbh->{lorder}) ;
ok(5, ! defined $dbh->{minkeypage}) ;
ok(6, ! defined $dbh->{maxkeypage}) ;
ok(7, ! defined $dbh->{compare}) ;
ok(8, ! defined $dbh->{prefix}) ;

$dbh->{flags} = 3000 ;
ok(9, $dbh->{flags} == 3000) ;

$dbh->{cachesize} = 9000 ;
ok(10, $dbh->{cachesize} == 9000);

$dbh->{psize} = 400 ;
ok(11, $dbh->{psize} == 400) ;

$dbh->{lorder} = 65 ;
ok(12, $dbh->{lorder} == 65) ;

$dbh->{minkeypage} = 123 ;
ok(13, $dbh->{minkeypage} == 123) ;

$dbh->{maxkeypage} = 1234 ;
ok(14, $dbh->{maxkeypage} == 1234 );

$dbh->{compare} = 1234 ;
ok(15, $dbh->{compare} == 1234) ;

$dbh->{prefix} = 1234 ;
ok(16, $dbh->{prefix} == 1234 );

# Check that an invalid entry is caught both for store & fetch
eval '$dbh->{fred} = 1234' ;
ok(17, $@ =~ /^DB_File::BTREEINFO::STORE - Unknown element 'fred' at/ ) ;
eval 'my $q = $dbh->{fred}' ;
ok(18, $@ =~ /^DB_File::BTREEINFO::FETCH - Unknown element 'fred' at/ ) ;

# Now check the interface to BTREE

my ($X, %h) ;
ok(19, $X = tie(%h, 'DB_File',$Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE )) ;

my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
ok(20, ($mode & 0777) == ($^O eq 'os2' ? 0666 : 0640) || $^O eq 'amigaos' || $^O eq 'MSWin32');

my ($key, $value, $i);
while (($key,$value) = each(%h)) {
    $i++;
}
ok(21, !$i ) ;

$h{'goner1'} = 'snork';

$h{'abc'} = 'ABC';
ok(22, $h{'abc'} eq 'ABC' );
ok(23, ! defined $h{'jimmy'} ) ;
ok(24, ! exists $h{'jimmy'} ) ;
ok(25,  defined $h{'abc'} ) ;

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

# tie to the same file again
ok(26, $X = tie(%h,'DB_File',$Dfile, O_RDWR, 0640, $DB_BTREE)) ;

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

ok(27, $#keys == 29 && $#values == 29) ;

$i = 0 ;
while (($key,$value) = each(%h)) {
    if ($key eq $keys[$i] && $value eq $values[$i] && $key eq lc($value)) {
	$key =~ y/a-z/A-Z/;
	$i++ if $key eq $value;
    }
}

ok(28, $i == 30) ;

@keys = ('blurfl', keys(%h), 'dyick');
ok(29, $#keys == 31) ;

#Check that the keys can be retrieved in order
my @b = keys %h ;
my @c = sort lexical @b ;
ok(30, ArrayCompare(\@b, \@c)) ;

$h{'foo'} = '';
ok(31, $h{'foo'} eq '' ) ;

# Berkeley DB from version 2.4.10 to 3.0 does not allow null keys.
# This feature was reenabled in version 3.1 of Berkeley DB.
my $result = 0 ;
if ($null_keys_allowed) {
    $h{''} = 'bar';
    $result = ( $h{''} eq 'bar' );
}
else
  { $result = 1 }
ok(32, $result) ;

# check cache overflow and numeric keys and contents
my $ok = 1;
for ($i = 1; $i < 200; $i++) { $h{$i + 0} = $i + 0; }
for ($i = 1; $i < 200; $i++) { $ok = 0 unless $h{$i} == $i; }
ok(33, $ok);

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
   $blksize,$blocks) = stat($Dfile);
ok(34, $size > 0 );

@h{0..200} = 200..400;
my @foo = @h{0..200};
ok(35, join(':',200..400) eq join(':',@foo) );

# Now check all the non-tie specific stuff


# Check R_NOOVERWRITE flag will make put fail when attempting to overwrite
# an existing record.
 
my $status = $X->put( 'x', 'newvalue', R_NOOVERWRITE) ;
ok(36, $status == 1 );
 
# check that the value of the key 'x' has not been changed by the 
# previous test
ok(37, $h{'x'} eq 'X' );

# standard put
$status = $X->put('key', 'value') ;
ok(38, $status == 0 );

#check that previous put can be retrieved
$value = 0 ;
$status = $X->get('key', $value) ;
ok(39, $status == 0 );
ok(40, $value eq 'value' );

# Attempting to delete an existing key should work

$status = $X->del('q') ;
ok(41, $status == 0 );
if ($null_keys_allowed) {
    $status = $X->del('') ;
} else {
    $status = 0 ;
}
ok(42, $status == 0 );

# Make sure that the key deleted, cannot be retrieved
ok(43, ! defined $h{'q'}) ;
ok(44, ! defined $h{''}) ;

undef $X ;
untie %h ;

ok(45, $X = tie(%h, 'DB_File',$Dfile, O_RDWR, 0640, $DB_BTREE ));

# Attempting to delete a non-existant key should fail

$status = $X->del('joe') ;
ok(46, $status == 1 );

# Check the get interface

# First a non-existing key
$status = $X->get('aaaa', $value) ;
ok(47, $status == 1 );

# Next an existing key
$status = $X->get('a', $value) ;
ok(48, $status == 0 );
ok(49, $value eq 'A' );

# seq
# ###

# use seq to find an approximate match
$key = 'ke' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
ok(50, $status == 0 );
ok(51, $key eq 'key' );
ok(52, $value eq 'value' );

# seq when the key does not match
$key = 'zzz' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
ok(53, $status == 1 );


# use seq to set the cursor, then delete the record @ the cursor.

$key = 'x' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
ok(54, $status == 0 );
ok(55, $key eq 'x' );
ok(56, $value eq 'X' );
$status = $X->del(0, R_CURSOR) ;
ok(57, $status == 0 );
$status = $X->get('x', $value) ;
ok(58, $status == 1 );

# ditto, but use put to replace the key/value pair.
$key = 'y' ;
$value = '' ;
$status = $X->seq($key, $value, R_CURSOR) ;
ok(59, $status == 0 );
ok(60, $key eq 'y' );
ok(61, $value eq 'Y' );

$key = "replace key" ;
$value = "replace value" ;
$status = $X->put($key, $value, R_CURSOR) ;
ok(62, $status == 0 );
ok(63, $key eq 'replace key' );
ok(64, $value eq 'replace value' );
$status = $X->get('y', $value) ;
ok(65, 1) ; # hard-wire to always pass. the previous test ($status == 1)
	    # only worked because of a bug in 1.85/6

# use seq to walk forwards through a file 

$status = $X->seq($key, $value, R_FIRST) ;
ok(66, $status == 0 );
my $previous = $key ;

$ok = 1 ;
while (($status = $X->seq($key, $value, R_NEXT)) == 0)
{
    ($ok = 0), last if ($previous cmp $key) == 1 ;
}

ok(67, $status == 1 );
ok(68, $ok == 1 );

# use seq to walk backwards through a file 
$status = $X->seq($key, $value, R_LAST) ;
ok(69, $status == 0 );
$previous = $key ;

$ok = 1 ;
while (($status = $X->seq($key, $value, R_PREV)) == 0)
{
    ($ok = 0), last if ($previous cmp $key) == -1 ;
    #print "key = [$key] value = [$value]\n" ;
}

ok(70, $status == 1 );
ok(71, $ok == 1 );


# check seq FIRST/LAST

# sync
# ####

$status = $X->sync ;
ok(72, $status == 0 );


# fd
# ##

$status = $X->fd ;
ok(73, $status != 0 );


undef $X ;
untie %h ;

unlink $Dfile;

# Now try an in memory file
my $Y;
ok(74, $Y = tie(%h, 'DB_File',undef, O_RDWR|O_CREAT, 0640, $DB_BTREE ));

# fd with an in memory file should return failure
$status = $Y->fd ;
ok(75, $status == -1 );


undef $Y ;
untie %h ;

# Duplicate keys
my $bt = new DB_File::BTREEINFO ;
$bt->{flags} = R_DUP ;
my ($YY, %hh);
ok(76, $YY = tie(%hh, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $bt )) ;

$hh{'Wall'} = 'Larry' ;
$hh{'Wall'} = 'Stone' ; # Note the duplicate key
$hh{'Wall'} = 'Brick' ; # Note the duplicate key
$hh{'Wall'} = 'Brick' ; # Note the duplicate key and value
$hh{'Smith'} = 'John' ;
$hh{'mouse'} = 'mickey' ;

# first work in scalar context
ok(77, scalar $YY->get_dup('Unknown') == 0 );
ok(78, scalar $YY->get_dup('Smith') == 1 );
ok(79, scalar $YY->get_dup('Wall') == 4 );

# now in list context
my @unknown = $YY->get_dup('Unknown') ;
ok(80, "@unknown" eq "" );

my @smith = $YY->get_dup('Smith') ;
ok(81, "@smith" eq "John" );

{
my @wall = $YY->get_dup('Wall') ;
my %wall ;
@wall{@wall} = @wall ;
ok(82, (@wall == 4 && $wall{'Larry'} && $wall{'Stone'} && $wall{'Brick'}) );
}

# hash
my %unknown = $YY->get_dup('Unknown', 1) ;
ok(83, keys %unknown == 0 );

my %smith = $YY->get_dup('Smith', 1) ;
ok(84, keys %smith == 1 && $smith{'John'}) ;

my %wall = $YY->get_dup('Wall', 1) ;
ok(85, keys %wall == 3 && $wall{'Larry'} == 1 && $wall{'Stone'} == 1 
		&& $wall{'Brick'} == 2);

undef $YY ;
untie %hh ;
unlink $Dfile;


# test multiple callbacks
my $Dfile1 = "btree1" ;
my $Dfile2 = "btree2" ;
my $Dfile3 = "btree3" ;
 
my $dbh1 = new DB_File::BTREEINFO ;
$dbh1->{compare} = sub { 
	no warnings 'numeric' ;
	$_[0] <=> $_[1] } ; 
 
my $dbh2 = new DB_File::BTREEINFO ;
$dbh2->{compare} = sub { $_[0] cmp $_[1] } ;
 
my $dbh3 = new DB_File::BTREEINFO ;
$dbh3->{compare} = sub { length $_[0] <=> length $_[1] } ;
 
 
my (%g, %k);
tie(%h, 'DB_File',$Dfile1, O_RDWR|O_CREAT, 0640, $dbh1 ) ; 
tie(%g, 'DB_File',$Dfile2, O_RDWR|O_CREAT, 0640, $dbh2 ) ;
tie(%k, 'DB_File',$Dfile3, O_RDWR|O_CREAT, 0640, $dbh3 ) ;
 
my @Keys = qw( 0123 12 -1234 9 987654321 def  ) ;
my (@srt_1, @srt_2, @srt_3);
{ 
  no warnings 'numeric' ;
  @srt_1 = sort { $a <=> $b } @Keys ; 
}
@srt_2 = sort { $a cmp $b } @Keys ;
@srt_3 = sort { length $a <=> length $b } @Keys ;
 
foreach (@Keys) {
    $h{$_} = 1 ;
    $g{$_} = 1 ;
    $k{$_} = 1 ;
}
 
sub ArrayCompare
{
    my($a, $b) = @_ ;
 
    return 0 if @$a != @$b ;
 
    foreach (1 .. length @$a)
    {
        return 0 unless $$a[$_] eq $$b[$_] ;
    }
 
    1 ;
}
 
ok(86, ArrayCompare (\@srt_1, [keys %h]) );
ok(87, ArrayCompare (\@srt_2, [keys %g]) );
ok(88, ArrayCompare (\@srt_3, [keys %k]) );

untie %h ;
untie %g ;
untie %k ;
unlink $Dfile1, $Dfile2, $Dfile3 ;

# clear
# #####

ok(89, tie(%h, 'DB_File', $Dfile1, O_RDWR|O_CREAT, 0640, $DB_BTREE ) );
foreach (1 .. 10)
  { $h{$_} = $_ * 100 }

# check that there are 10 elements in the hash
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(90, $i == 10);

# now clear the hash
%h = () ;

# check it is empty
$i = 0 ;
while (($key,$value) = each(%h)) {
    $i++;
}
ok(91, $i == 0);

untie %h ;
unlink $Dfile1 ;

{
    # check that attempting to tie an array to a DB_BTREE will fail

    my $filename = "xyz" ;
    my @x ;
    eval { tie @x, 'DB_File', $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE ; } ;
    ok(92, $@ =~ /^DB_File can only tie an associative array to a DB_BTREE database/) ;
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
   use vars qw( @ISA @EXPORT) ;

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
    main::ok(93, $@ eq "") ;
    my %h ;
    my $X ;
    eval '
	$X = tie(%h, "SubDB","dbbtree.tmp", O_RDWR|O_CREAT, 0640, $DB_BTREE );
	' ;

    main::ok(94, $@ eq "") ;

    my $ret = eval '$h{"fred"} = 3 ; return $h{"fred"} ' ;
    main::ok(95, $@ eq "") ;
    main::ok(96, $ret == 5) ;

    my $value = 0;
    $ret = eval '$X->put("joe", 4) ; $X->get("joe", $value) ; return $value' ;
    main::ok(97, $@ eq "") ;
    main::ok(98, $ret == 10) ;

    $ret = eval ' R_NEXT eq main::R_NEXT ' ;
    main::ok(99, $@ eq "" ) ;
    main::ok(100, $ret == 1) ;

    $ret = eval '$X->A_new_method("joe") ' ;
    main::ok(101, $@ eq "") ;
    main::ok(102, $ret eq "[[11]]") ;

    undef $X;
    untie(%h);
    unlink "SubDB.pm", "dbbtree.tmp" ;

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
       my($fk, $sk, $fv, $sv) = @_ ;
       return
           $fetch_key eq $fk && $store_key eq $sk && 
	   $fetch_value eq $fv && $store_value eq $sv &&
	   $_ eq 'original' ;
   }
   
   ok(103, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE ) );

   $db->filter_fetch_key   (sub { $fetch_key = $_ }) ;
   $db->filter_store_key   (sub { $store_key = $_ }) ;
   $db->filter_fetch_value (sub { $fetch_value = $_}) ;
   $db->filter_store_value (sub { $store_value = $_ }) ;

   $_ = "original" ;

   $h{"fred"} = "joe" ;
   #                   fk   sk     fv   sv
   ok(104, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(105, $h{"fred"} eq "joe");
   #                   fk    sk     fv    sv
   ok(106, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(107, $db->FIRSTKEY() eq "fred") ;
   #                    fk     sk  fv  sv
   ok(108, checkOutput( "fred", "", "", "")) ;

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
   ok(109, checkOutput( "", "fred", "", "Jxe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(110, $h{"Fred"} eq "[Jxe]");
   #                   fk   sk     fv    sv
   ok(111, checkOutput( "", "fred", "[Jxe]", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(112, $db->FIRSTKEY() eq "FRED") ;
   #                   fk   sk     fv    sv
   ok(113, checkOutput( "FRED", "", "", "")) ;

   # put the original filters back
   $db->filter_fetch_key   ($old_fk);
   $db->filter_store_key   ($old_sk);
   $db->filter_fetch_value ($old_fv);
   $db->filter_store_value ($old_sv);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(114, checkOutput( "", "fred", "", "joe")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(115, $h{"fred"} eq "joe");
   ok(116, checkOutput( "", "fred", "joe", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(117, $db->FIRSTKEY() eq "fred") ;
   ok(118, checkOutput( "fred", "", "", "")) ;

   # delete the filters
   $db->filter_fetch_key   (undef);
   $db->filter_store_key   (undef);
   $db->filter_fetch_value (undef);
   $db->filter_store_value (undef);

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   $h{"fred"} = "joe" ;
   ok(119, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(120, $h{"fred"} eq "joe");
   ok(121, checkOutput( "", "", "", "")) ;

   ($fetch_key, $store_key, $fetch_value, $store_value) = ("") x 4 ;
   ok(122, $db->FIRSTKEY() eq "fred") ;
   ok(123, checkOutput( "", "", "", "")) ;

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
    ok(124, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE ) );

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
    ok(125, $result{"store key"} eq "store key - 1: [fred]");
    ok(126, $result{"store value"} eq "store value - 1: [joe]");
    ok(127, ! defined $result{"fetch key"} );
    ok(128, ! defined $result{"fetch value"} );
    ok(129, $_ eq "original") ;

    ok(130, $db->FIRSTKEY() eq "fred") ;
    ok(131, $result{"store key"} eq "store key - 1: [fred]");
    ok(132, $result{"store value"} eq "store value - 1: [joe]");
    ok(133, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(134, ! defined $result{"fetch value"} );
    ok(135, $_ eq "original") ;

    $h{"jim"}  = "john" ;
    ok(136, $result{"store key"} eq "store key - 2: [fred jim]");
    ok(137, $result{"store value"} eq "store value - 2: [joe john]");
    ok(138, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(139, ! defined $result{"fetch value"} );
    ok(140, $_ eq "original") ;

    ok(141, $h{"fred"} eq "joe");
    ok(142, $result{"store key"} eq "store key - 3: [fred jim fred]");
    ok(143, $result{"store value"} eq "store value - 2: [joe john]");
    ok(144, $result{"fetch key"} eq "fetch key - 1: [fred]");
    ok(145, $result{"fetch value"} eq "fetch value - 1: [joe]");
    ok(146, $_ eq "original") ;

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

   ok(147, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE ) );

   $db->filter_store_key (sub { $_ = $h{$_} }) ;

   eval '$h{1} = 1234' ;
   ok(148, $@ =~ /^recursion detected in filter_store_key at/ );
   
   undef $db ;
   untie %h;
   unlink $Dfile;
}


{
   # Examples from the POD


  my $file = "xyzt" ;
  {
    my $redirect = new Redirect $file ;

    # BTREE example 1
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;

    my %h ;

    sub Compare
    {
        my ($key1, $key2) = @_ ;
        "\L$key1" cmp "\L$key2" ;
    }

    # specify the Perl sub that will do the comparison
    $DB_BTREE->{'compare'} = \&Compare ;

    unlink "tree" ;
    tie %h, "DB_File", "tree", O_RDWR|O_CREAT, 0640, $DB_BTREE 
        or die "Cannot open file 'tree': $!\n" ;

    # Add a key/value pair to the file
    $h{'Wall'} = 'Larry' ;
    $h{'Smith'} = 'John' ;
    $h{'mouse'} = 'mickey' ;
    $h{'duck'}  = 'donald' ;

    # Delete
    delete $h{"duck"} ;

    # Cycle through the keys printing them in order.
    # Note it is not necessary to sort the keys as
    # the btree will have kept them in order automatically.
    foreach (keys %h)
      { print "$_\n" }

    untie %h ;

    unlink "tree" ;
  }  

  delete $DB_BTREE->{'compare'} ;

  ok(149, docat_del($file) eq <<'EOM') ;
mouse
Smith
Wall
EOM
   
  {
    my $redirect = new Redirect $file ;

    # BTREE example 2
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;

    use vars qw($filename %h ) ;

    $filename = "tree" ;
    unlink $filename ;
 
    # Enable duplicate records
    $DB_BTREE->{'flags'} = R_DUP ;
 
    tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE 
	or die "Cannot open $filename: $!\n";
 
    # Add some key/value pairs to the file
    $h{'Wall'} = 'Larry' ;
    $h{'Wall'} = 'Brick' ; # Note the duplicate key
    $h{'Wall'} = 'Brick' ; # Note the duplicate key and value
    $h{'Smith'} = 'John' ;
    $h{'mouse'} = 'mickey' ;

    # iterate through the associative array
    # and print each key/value pair.
    foreach (keys %h)
      { print "$_	-> $h{$_}\n" }

    untie %h ;

    unlink $filename ;
  }  

  ok(150, docat_del($file) eq ($db185mode ? <<'EOM' : <<'EOM') ) ;
Smith	-> John
Wall	-> Brick
Wall	-> Brick
Wall	-> Brick
mouse	-> mickey
EOM
Smith	-> John
Wall	-> Larry
Wall	-> Larry
Wall	-> Larry
mouse	-> mickey
EOM

  {
    my $redirect = new Redirect $file ;

    # BTREE example 3
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;
 
    use vars qw($filename $x %h $status $key $value) ;

    $filename = "tree" ;
    unlink $filename ;
 
    # Enable duplicate records
    $DB_BTREE->{'flags'} = R_DUP ;
 
    $x = tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE 
	or die "Cannot open $filename: $!\n";
 
    # Add some key/value pairs to the file
    $h{'Wall'} = 'Larry' ;
    $h{'Wall'} = 'Brick' ; # Note the duplicate key
    $h{'Wall'} = 'Brick' ; # Note the duplicate key and value
    $h{'Smith'} = 'John' ;
    $h{'mouse'} = 'mickey' ;
 
    # iterate through the btree using seq
    # and print each key/value pair.
    $key = $value = 0 ;
    for ($status = $x->seq($key, $value, R_FIRST) ;
         $status == 0 ;
         $status = $x->seq($key, $value, R_NEXT) )
      {  print "$key	-> $value\n" }
 
 
    undef $x ;
    untie %h ;
  }

  ok(151, docat_del($file) eq ($db185mode == 1 ? <<'EOM' : <<'EOM') ) ;
Smith	-> John
Wall	-> Brick
Wall	-> Brick
Wall	-> Larry
mouse	-> mickey
EOM
Smith	-> John
Wall	-> Larry
Wall	-> Brick
Wall	-> Brick
mouse	-> mickey
EOM


  {
    my $redirect = new Redirect $file ;

    # BTREE example 4
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;
 
    use vars qw($filename $x %h ) ;

    $filename = "tree" ;
 
    # Enable duplicate records
    $DB_BTREE->{'flags'} = R_DUP ;
 
    $x = tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE 
	or die "Cannot open $filename: $!\n";
 
    my $cnt  = $x->get_dup("Wall") ;
    print "Wall occurred $cnt times\n" ;

    my %hash = $x->get_dup("Wall", 1) ;
    print "Larry is there\n" if $hash{'Larry'} ;
    print "There are $hash{'Brick'} Brick Walls\n" ;

    my @list = sort $x->get_dup("Wall") ;
    print "Wall =>	[@list]\n" ;

    @list = $x->get_dup("Smith") ;
    print "Smith =>	[@list]\n" ;
 
    @list = $x->get_dup("Dog") ;
    print "Dog =>	[@list]\n" ; 
 
    undef $x ;
    untie %h ;
  }

  ok(152, docat_del($file) eq <<'EOM') ;
Wall occurred 3 times
Larry is there
There are 2 Brick Walls
Wall =>	[Brick Brick Larry]
Smith =>	[John]
Dog =>	[]
EOM

  {
    my $redirect = new Redirect $file ;

    # BTREE example 5
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;
 
    use vars qw($filename $x %h $found) ;

    my $filename = "tree" ;
 
    # Enable duplicate records
    $DB_BTREE->{'flags'} = R_DUP ;
 
    $x = tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE 
	or die "Cannot open $filename: $!\n";

    $found = ( $x->find_dup("Wall", "Larry") == 0 ? "" : "not") ; 
    print "Larry Wall is $found there\n" ;
    
    $found = ( $x->find_dup("Wall", "Harry") == 0 ? "" : "not") ; 
    print "Harry Wall is $found there\n" ;
    
    undef $x ;
    untie %h ;
  }

  ok(153, docat_del($file) eq <<'EOM') ;
Larry Wall is  there
Harry Wall is not there
EOM

  {
    my $redirect = new Redirect $file ;

    # BTREE example 6
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;
 
    use vars qw($filename $x %h $found) ;

    my $filename = "tree" ;
 
    # Enable duplicate records
    $DB_BTREE->{'flags'} = R_DUP ;
 
    $x = tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE 
	or die "Cannot open $filename: $!\n";

    $x->del_dup("Wall", "Larry") ;

    $found = ( $x->find_dup("Wall", "Larry") == 0 ? "" : "not") ; 
    print "Larry Wall is $found there\n" ;
    
    undef $x ;
    untie %h ;

    unlink $filename ;
  }

  ok(154, docat_del($file) eq <<'EOM') ;
Larry Wall is not there
EOM

  {
    my $redirect = new Redirect $file ;

    # BTREE example 7
    ###

    use warnings FATAL => qw(all) ;
    use strict ;
    use DB_File ;
    use Fcntl ;

    use vars qw($filename $x %h $st $key $value) ;

    sub match
    {
        my $key = shift ;
        my $value = 0;
        my $orig_key = $key ;
        $x->seq($key, $value, R_CURSOR) ;
        print "$orig_key\t-> $key\t-> $value\n" ;
    }

    $filename = "tree" ;
    unlink $filename ;

    $x = tie %h, "DB_File", $filename, O_RDWR|O_CREAT, 0640, $DB_BTREE
        or die "Cannot open $filename: $!\n";
 
    # Add some key/value pairs to the file
    $h{'mouse'} = 'mickey' ;
    $h{'Wall'} = 'Larry' ;
    $h{'Walls'} = 'Brick' ; 
    $h{'Smith'} = 'John' ;
 

    $key = $value = 0 ;
    print "IN ORDER\n" ;
    for ($st = $x->seq($key, $value, R_FIRST) ;
	 $st == 0 ;
         $st = $x->seq($key, $value, R_NEXT) )
	
      {  print "$key	-> $value\n" }
 
    print "\nPARTIAL MATCH\n" ;

    match "Wa" ;
    match "A" ;
    match "a" ;

    undef $x ;
    untie %h ;

    unlink $filename ;

  }

  ok(155, docat_del($file) eq <<'EOM') ;
IN ORDER
Smith	-> John
Wall	-> Larry
Walls	-> Brick
mouse	-> mickey

PARTIAL MATCH
Wa	-> Wall	-> Larry
A	-> Smith	-> John
a	-> mouse	-> mickey
EOM

}

#{
#   # R_SETCURSOR
#   use strict ;
#   my (%h, $db) ;
#   unlink $Dfile;
#
#   ok(156, $db = tie(%h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0640, $DB_BTREE ) );
#
#   $h{abc} = 33 ;
#   my $k = "newest" ;
#   my $v = 44 ;
#   my $status = $db->put($k, $v, R_SETCURSOR) ;
#   print "status = [$status]\n" ;
#   ok(157, $status == 0) ;
#   $status = $db->del($k, R_CURSOR) ;
#   print "status = [$status]\n" ;
#   ok(158, $status == 0) ;
#   $k = "newest" ;
#   ok(159, $db->get($k, $v, R_CURSOR)) ;
#
#   ok(160, keys %h == 1) ;
#   
#   undef $db ;
#   untie %h;
#   unlink $Dfile;
#}

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
    
    tie %h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0664, $DB_BTREE
	or die "Can't open file: $!\n" ;
    $h{ABC} = undef;
    ok(156, $a eq "") ;
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
    
    tie %h, 'DB_File', $Dfile, O_RDWR|O_CREAT, 0664, $DB_BTREE
	or die "Can't open file: $!\n" ;
    %h = (); ;
    ok(157, $a eq "") ;
    untie %h ;
    unlink $Dfile;
}

exit ;
