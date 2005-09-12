#!/usr/bin/perl -w

use strict;

use DBI;
use POSIX;
use HTML::Template;

# MySQL Cluster size estimator
# ----------------------------
#
# (C)2005 MySQL AB
#
#
# The purpose of this tool is to work out storage requirements
# from an existing MySQL database.
#
# This involves connecting to a mysql server and throwing a bunch
# of queries at it.
#
# We currently estimate sizes for: 4.1, 5.0 and 5.1 to various amounts
# of accurracy.
#
# There is no warranty.
#
# BUGS
# ----
# - enum/set is 0 byte storage! Woah - efficient!
# - some float stores come out weird (when there's a comma e.g. 'float(4,1)')
# - no disk data values
# - computes the storage requirements of views (and probably MERGE)
# - ignores character sets.

my $template = HTML::Template->new(filename => 'ndb_size.tmpl',
				   die_on_bad_params => 0);

my $dbh;

{
    my $database= $ARGV[0];
    my $hostname= $ARGV[1];
    my $port= $ARGV[2];
    my $user= $ARGV[3];
    my $password= $ARGV[4];
    my $dsn = "DBI:mysql:database=$database;host=$hostname;port=$port";
    $dbh= DBI->connect($dsn, $user, $password);
    $template->param(db => $database);
    $template->param(dsn => $dsn);
}

my @releases = ({rel=>'4.1'},{rel=>'5.0'},{rel=>'5.1'});
$template->param(releases => \@releases);

my $tables  = $dbh->selectall_arrayref("show tables");

my @table_size;

sub align {
    my($to,@unaligned) = @_;
    my @aligned;
    foreach my $x (@unaligned) {
	push @aligned, $to * POSIX::floor(($x+$to-1)/$to);
    }
    return @aligned;
}

foreach(@{$tables})
{
    my $table= @{$_}[0];
    my @columns;
    my $info= $dbh->selectall_hashref("describe $table","Field");
    my @count  = $dbh->selectrow_array("select count(*) from $table");
    my %columnsize; # used for index calculations

    # We now work out the DataMemory usage
    
    # sizes for   4.1, 5.0, 5.1
    my @totalsize= (0,0,0);

    foreach(keys %$info)
    {
	my @realsize = (0,0,0);
	my $type;
	my $size;
	my $name= $_;

	if($$info{$_}{Type} =~ /^(.*?)\((\d+)\)/)
	{
	    $type= $1;
	    $size= $2;
	}
	else
	{
	    $type= $$info{$_}{Type};
	}

	if($type =~ /tinyint/)
	{@realsize=(1,1,1)}
	elsif($type =~ /smallint/)
	{@realsize=(2,2,2)}
	elsif($type =~ /mediumint/)
	{@realsize=(3,3,3)}
	elsif($type =~ /bigint/)
	{@realsize=(8,8,8)}
	elsif($type =~ /int/)
	{@realsize=(4,4,4)}
	elsif($type =~ /float/)
	{
	    if($size<=24)
	    {@realsize=(4,4,4)}
	    else
	    {@realsize=(8,8,8)}
	}
	elsif($type =~ /double/ || $type =~ /real/)
	{@realsize=(8,8,8)}
	elsif($type =~ /bit/)
	{
	    my $a=($size+7)/8;
	    @realsize = ($a,$a,$a);
	}
	elsif($type =~ /datetime/)
	{@realsize=(8,8,8)}
	elsif($type =~ /timestamp/)
	{@realsize=(4,4,4)}
	elsif($type =~ /date/ || $type =~ /time/)
	{@realsize=(3,3,3)}
	elsif($type =~ /year/)
	{@realsize=(1,1,1)}
	elsif($type =~ /varchar/ || $type =~ /varbinary/)
	{
	    my $fixed= 1+$size;
	    my @dynamic=$dbh->selectrow_array("select avg(length($name)) from $table");
	    $dynamic[0]=0 if !$dynamic[0];
	    @realsize= ($fixed,$fixed,ceil($dynamic[0]));
	}
	elsif($type =~ /binary/ || $type =~ /char/)
	{@realsize=($size,$size,$size)}
	elsif($type =~ /text/ || $type =~ /blob/)
	{@realsize=(256,256,1)} # FIXME check if 5.1 is correct

	@realsize= align(4,@realsize);

	$totalsize[$_]+=$realsize[$_] foreach 0..$#totalsize;

	my @realout;
	push @realout,{val=>$_} foreach @realsize;

	push @columns, {
	    name=>$name,
	    type=>$type,
	    size=>$size,
	    key=>$$info{$_}{Key},
	    datamemory=>\@realout,
	};

	$columnsize{$name}= \@realsize; # used for index calculations
    }
    
    # And now... the IndexMemory usage.
    #
    # Firstly, we assemble some information about the indexes.
    # We use SHOW INDEX instead of using INFORMATION_SCHEMA so
    # we can still connect to pre-5.0 mysqlds.
    my %indexes;
    {
	my $sth= $dbh->prepare("show index from $table");
	$sth->execute;
	while(my $i = $sth->fetchrow_hashref)
	{    
	    $indexes{${%$i}{Key_name}}= {
		type=>${%$i}{Index_type},
		unique=>!${%$i}{Non_unique},
		comment=>${%$i}{Comment},
	    } if !defined($indexes{${%$i}{Key_name}});

	    $indexes{${%$i}{Key_name}}{columns}[${%$i}{Seq_in_index}-1]= 
		${%$i}{Column_name};
	}
    }

    if(!defined($indexes{PRIMARY})) {
	$indexes{PRIMARY}= {
	    type=>'BTREE',
	    unique=>1,
	    comment=>'Hidden pkey created by NDB',
	    columns=>['HIDDEN_NDB_PKEY'],
	};
	push @columns, {
	    name=>'HIDDEN_NDB_PKEY',
	    type=>'bigint',
	    size=>8,
	    key=>'PRI',
	    datamemory=>[{val=>8},{val=>8},{val=>8}],
	};
	$columnsize{'HIDDEN_NDB_PKEY'}= [8,8,8];
    }

    my @IndexDataMemory= ({val=>0},{val=>0},{val=>0});
    my @RowIndexMemory= ({val=>0},{val=>0},{val=>0});

    my @indexes;
    foreach my $index (keys %indexes) {
	my $im41= 25;
	$im41+=$columnsize{$_}[0] foreach @{$indexes{$index}{columns}};
	my @im = ({val=>$im41},{val=>25},{val=>25});
	my @dm = ({val=>10},{val=>10},{val=>10});
	push @indexes, {
	    name=>$index,
	    type=>$indexes{$index}{type},
	    columns=>join(',',@{$indexes{$index}{columns}}),
	    indexmemory=>\@im,
	    datamemory=>\@dm,
	};
	$IndexDataMemory[$_]{val}+=$dm[$_]{val} foreach 0..2;
	$RowIndexMemory[$_]{val}+=$im[$_]{val} foreach 0..2;
    }

    # total size + 16 bytes overhead
    my @TotalDataMemory;
    $TotalDataMemory[$_]{val}=$IndexDataMemory[$_]{val}+$totalsize[$_]+16 foreach 0..2;

    my @RowDataMemory;
    push @RowDataMemory,{val=>$_} foreach @totalsize;
    
    my @RowPerPage;
    push @RowPerPage,{val=>(floor((32768-128)/$TotalDataMemory[$_]{val}))} foreach 0..$#TotalDataMemory;

    my @RowPerIndexPage;
    push @RowPerIndexPage,{val=>(floor(8192/$RowIndexMemory[$_]{val}))} foreach 0..$#TotalDataMemory;

    my @DataMemory;
    push @DataMemory,{val=>ceil(($count[0]/($RowPerPage[$_]{val})))*32} foreach 0..$#RowPerPage;

    my @IndexMemory;
    push @IndexMemory,{val=>ceil(($count[0]/($RowPerIndexPage[$_]{val})))*8} foreach 0..$#RowPerPage;

    my $count= $count[0];
    my @counts;
    $counts[$_]{val}= $count foreach 0..$#releases;

    push @table_size, {
	table=>$table,
	indexes=>\@indexes,
	columns=>\@columns,
	count=>\@counts,
	RowDataMemory=>\@RowDataMemory,
	releases=>\@releases,
	IndexDataMemory=>\@IndexDataMemory,
	TotalDataMemory=>\@TotalDataMemory,
	RowPerPage=>\@RowPerPage,
	DataMemory=>\@DataMemory,
	RowIndexMemory=>\@RowIndexMemory,
	RowPerIndexPage=>\@RowPerIndexPage,
	IndexMemory=>\@IndexMemory,
	
    };
}

$template->param(tables => \@table_size);
print $template->output;
