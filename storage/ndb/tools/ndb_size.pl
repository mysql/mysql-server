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
# - DECIMAL is 0 byte storage. A bit too efficient.
# - some float stores come out weird (when there's a comma e.g. 'float(4,1)')
# - no disk data values
# - computes the storage requirements of views (and probably MERGE)
# - ignores character sets.

my $template = HTML::Template->new(filename => 'ndb_size.tmpl',
				   die_on_bad_params => 0)
    or die "Could not open ndb_size.tmpl.";

my $dbh;

if(@ARGV < 3 || $ARGV[0] eq '--usage' || $ARGV[0] eq '--help')
{
    print STDERR "Usage:\n";
    print STDERR "\tndb_size.pl database hostname user password\n\n";
    print STDERR "If you need to specify a port number, use host:port\n\n";
    exit(1);
}

{
    my $database= $ARGV[0];
    my $hostname= $ARGV[1];
    my $user= $ARGV[2];
    my $password= $ARGV[3];
    my $dsn = "DBI:mysql:database=$database;host=$hostname";
    $dbh= DBI->connect($dsn, $user, $password) or exit(1);
    $template->param(db => $database);
    $template->param(dsn => $dsn);
}

my @releases = ({rel=>'4.1'},{rel=>'5.0'},{rel=>'5.1'}); #,{rel=>'5.1-dd'});
$template->param(releases => \@releases);

my $tables  = $dbh->selectall_arrayref("show tables");

my @table_size;

my @dbDataMemory;
my @dbIndexMemory;
my @NoOfAttributes;
my @NoOfIndexes;
my @NoOfTables;
$NoOfTables[$_]{val} = @{$tables} foreach 0..$#releases;


sub align {
    my($to,@unaligned) = @_;
    my @aligned;
    foreach my $x (@unaligned) {
	push @aligned, $to * POSIX::floor(($x+$to-1)/$to);
    }
    return @aligned;
}

sub do_table {
    my $table= shift;
    my $info= shift;
    my %indexes= %{$_[0]};
    my @count= @{$_[1]};

    my @columns;
    my %columnsize; # used for index calculations
    # We now work out the DataMemory usage
    
    # sizes for   4.1, 5.0, 5.1 and 5.1-dd
    my @totalsize= (0,0,0,0);
    @totalsize= @totalsize[0..$#releases]; # limit to releases we're outputting
    my $nrvarsize= 0;

    foreach(keys %$info)
    {
	my @realsize = (0,0,0,0);
	my @varsize  = (0,0,0,0);
	my $type;
	my $size;
	my $name= $_;
	my $is_varsize= 0;

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
	{@realsize=(1,1,1,1)}
	elsif($type =~ /smallint/)
	{@realsize=(2,2,2,2)}
	elsif($type =~ /mediumint/)
	{@realsize=(3,3,3,3)}
	elsif($type =~ /bigint/)
	{@realsize=(8,8,8,8)}
	elsif($type =~ /int/)
	{@realsize=(4,4,4,4)}
	elsif($type =~ /float/)
	{
	    if($size<=24)
	    {@realsize=(4,4,4,4)}
	    else
	    {@realsize=(8,8,8,8)}
	}
	elsif($type =~ /double/ || $type =~ /real/)
	{@realsize=(8,8,8,8)}
	elsif($type =~ /bit/)
	{
	    my $a=($size+7)/8;
	    @realsize = ($a,$a,$a,$a);
	}
	elsif($type =~ /datetime/)
	{@realsize=(8,8,8,8)}
	elsif($type =~ /timestamp/)
	{@realsize=(4,4,4,4)}
	elsif($type =~ /date/ || $type =~ /time/)
	{@realsize=(3,3,3,3)}
	elsif($type =~ /year/)
	{@realsize=(1,1,1,1)}
	elsif($type =~ /varchar/ || $type =~ /varbinary/)
	{
	    my $fixed=$size+ceil($size/256);
	    my @dynamic=$dbh->selectrow_array("select avg(length(`"
					      .$name
					      ."`)) from `".$table.'`');
	    $dynamic[0]=0 if !$dynamic[0];
	    $dynamic[0]+=ceil($dynamic[0]/256); # size bit
	    $nrvarsize++;
	    $is_varsize= 1;
	    $varsize[3]= ceil($dynamic[0]);
	    @realsize= ($fixed,$fixed,ceil($dynamic[0]),$fixed);
	}
	elsif($type =~ /binary/ || $type =~ /char/)
	{@realsize=($size,$size,$size,$size)}
	elsif($type =~ /text/ || $type =~ /blob/)
	{
	    @realsize=(8+256,8+256,8+256,8+256);

	    my $blobhunk= 2000;
	    $blobhunk= 8000 if $type=~ /longblob/;
	    $blobhunk= 4000 if $type=~ /mediumblob/;

	    my @blobsize=$dbh->selectrow_array("select SUM(CEILING(".
					       "length(`$name`)/$blobhunk))".
					       "from `".$table."`");
	    $blobsize[0]=0 if !defined($blobsize[0]);
	    #$NoOfTables[$_]{val} += 1 foreach 0..$#releases; # blob uses table
	    do_table($table."\$BLOB_$name",
		     {'PK'=>{Type=>'int'},
		      'DIST'=>{Type=>'int'},
		      'PART'=>{Type=>'int'},
		      'DATA'=>{Type=>"binary($blobhunk)"}
		  },
		     {'PRIMARY' => {
                         'unique' => 1,
                         'comment' => '',
                         'columns' => [
                                        'PK',
				        'DIST',
				        'PART',
                                      ],
			 'type' => 'HASH'
				    }
		  },
		     \@blobsize);
	}

	@realsize= @realsize[0..$#releases];
	@realsize= align(4,@realsize);

	$totalsize[$_]+=$realsize[$_] foreach 0..$#totalsize;

	my @realout;
	push @realout,{val=>$_} foreach @realsize;

	push @columns, {
	    name=>$name,
	    type=>$type,
	    is_varsize=>$is_varsize,
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

    if(!defined($indexes{PRIMARY})) {
	my @usage= ({val=>8},{val=>8},{val=>8},{val=>8});
	@usage= @usage[0..$#releases];
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
	    datamemory=>\@usage,
	};
	$columnsize{'HIDDEN_NDB_PKEY'}= [8,8,8];
    }

    my @IndexDataMemory= ({val=>0},{val=>0},{val=>0},{val=>0});
    my @RowIndexMemory= ({val=>0},{val=>0},{val=>0},{val=>0});
    @IndexDataMemory= @IndexDataMemory[0..$#releases];
    @RowIndexMemory= @RowIndexMemory[0..$#releases];

    my @indexes;
    foreach my $index (keys %indexes) {
	my $im41= 25;
	$im41+=$columnsize{$_}[0] foreach @{$indexes{$index}{columns}};
	my @im = ({val=>$im41},{val=>25},{val=>25}); #,{val=>25});
	my @dm = ({val=>10},{val=>10},{val=>10}); #,{val=>10});
	push @indexes, {
	    name=>$index,
	    type=>$indexes{$index}{type},
	    columns=>join(',',@{$indexes{$index}{columns}}),
	    indexmemory=>\@im,
	    datamemory=>\@dm,
	};
	$IndexDataMemory[$_]{val}+=$dm[$_]{val} foreach 0..$#releases;
	$RowIndexMemory[$_]{val}+=$im[$_]{val} foreach 0..$#releases;
    }

    # total size + 16 bytes overhead
    my @TotalDataMemory;
    my @RowOverhead = ({val=>16},{val=>16},{val=>16}); #,{val=>24});
    # 5.1 has ptr to varsize page, and per-varsize overhead
    my @nrvarsize_mem= ({val=>0},{val=>0},
			{val=>8}); #,{val=>0});
    {
	my @a= align(4,$nrvarsize*2);
	$nrvarsize_mem[2]{val}+=$a[0]+$nrvarsize*4;
    }

    $TotalDataMemory[$_]{val}=$IndexDataMemory[$_]{val}+$totalsize[$_]+$RowOverhead[$_]{val}+$nrvarsize_mem[$_]{val} foreach 0..$#releases;

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

    my @nrvarsize_rel= ({val=>0},{val=>0},
			{val=>$nrvarsize}); #,{val=>0});

    push @table_size, {
	table=>$table,
	indexes=>\@indexes,
	columns=>\@columns,
	count=>\@counts,
	RowOverhead=>\@RowOverhead,
	RowDataMemory=>\@RowDataMemory,
	nrvarsize=>\@nrvarsize_rel,
	nrvarsize_mem=>\@nrvarsize_mem,
	releases=>\@releases,
	IndexDataMemory=>\@IndexDataMemory,
	TotalDataMemory=>\@TotalDataMemory,
	RowPerPage=>\@RowPerPage,
	DataMemory=>\@DataMemory,
	RowIndexMemory=>\@RowIndexMemory,
	RowPerIndexPage=>\@RowPerIndexPage,
	IndexMemory=>\@IndexMemory,
	
    };

    $dbDataMemory[$_]{val} += $DataMemory[$_]{val} foreach 0..$#releases;
    $dbIndexMemory[$_]{val} += $IndexMemory[$_]{val} foreach 0..$#releases;
    $NoOfAttributes[$_]{val} += @columns foreach 0..$#releases;
    $NoOfIndexes[$_]{val} += @indexes foreach 0..$#releases;
}

foreach(@{$tables})
{
    my $table= @{$_}[0];
    my $info= $dbh->selectall_hashref('describe `'.$table.'`',"Field");
    my @count  = $dbh->selectrow_array('select count(*) from `'.$table.'`');

    my %indexes;
    {
	my $sth= $dbh->prepare("show index from `".$table.'`');
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
    do_table($table, $info, \%indexes, \@count);
}

my @NoOfTriggers;
# for unique hash indexes
$NoOfTriggers[$_]{val} += $NoOfIndexes[$_]{val}*3 foreach 0..$#releases;
# for ordered index
$NoOfTriggers[$_]{val} += $NoOfIndexes[$_]{val} foreach 0..$#releases;

my @ParamMemory;
foreach (0..$#releases) {
    $ParamMemory[0]{releases}[$_]{val}= POSIX::ceil(200*$NoOfAttributes[$_]{val}/1024);
    $ParamMemory[0]{name}= 'Attributes';

    $ParamMemory[1]{releases}[$_]{val}= 20*$NoOfTables[$_]{val};
    $ParamMemory[1]{name}= 'Tables';

    $ParamMemory[2]{releases}[$_]{val}= 10*$NoOfIndexes[$_]{val};
    $ParamMemory[2]{name}= 'OrderedIndexes';

    $ParamMemory[3]{releases}[$_]{val}= 15*$NoOfIndexes[$_]{val};
    $ParamMemory[3]{name}= 'UniqueHashIndexes';    
}

$template->param(tables => \@table_size);
$template->param(Parameters => [{name=>'DataMemory (kb)',
				 releases=>\@dbDataMemory},
				{name=>'IndexMemory (kb)',
				 releases=>\@dbIndexMemory},
				{name=>'MaxNoOfTables',
				 releases=>\@NoOfTables},
				{name=>'MaxNoOfAttributes',
				 releases=>\@NoOfAttributes},
				{name=>'MaxNoOfOrderedIndexes',
				 releases=>\@NoOfIndexes},
				{name=>'MaxNoOfUniqueHashIndexes',
				 releases=>\@NoOfIndexes},
				{name=>'MaxNoOfTriggers',
				 releases=>\@NoOfTriggers}
				]
		 );
$template->param(ParamMemory => \@ParamMemory);

print $template->output;
