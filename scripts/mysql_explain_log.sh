#!@PERL@ -w
use strict;
use DBI;

use Getopt::Long;
$Getopt::Long::ignorecase=0;

print "explain_log	provided by http://www.mobile.de\n";
print "===========	================================\n";

my $Param={};

$Param->{host}='';
$Param->{user}='';
$Param->{password}='';
$Param->{PrintError}=0;

if (!GetOptions ('date|d:i' => \$Param->{ViewDate},
		 'host|h:s' => \$Param->{host},
		 'user|u:s' => \$Param->{user},
		 'password|p:s' => \$Param->{password},
		 'printerror|e:s' => \$Param->{PrintError},
		)) {
  ShowOptions();
}
else {
  $Param->{UpdateCount} = 0;
  $Param->{SelectCount} = 0;
  $Param->{IdxUseCount} = 0;
  $Param->{LineCount} = 0;

  $Param->{Init} = 0;
  $Param->{Field} = 0;
  $Param->{Refresh} = 0;
  $Param->{QueryCount} = 0;
  $Param->{Statistics} =0;

  $Param->{Query} = undef;
  $Param->{ALL} = undef ;
  $Param->{Comment} = undef ;

  @{$Param->{Rows}} = (qw|possible_keys key type|);

  if ($Param->{ViewDate}) {
    $Param->{View} = 0;
  }
  else {
    $Param->{View} = 1;
  }

  #print "Date=$Param->{ViewDate}, host=$Param->{host}, user=$Param->{user}, password=$Param->{password}\n";

  $Param->{dbh}=DBI->connect("DBI:mysql:host=$Param->{host}",$Param->{user},$Param->{password},{PrintError=>0});
  if (DBI::err()) {
    print "Error: " . DBI::errstr() . "\n";
  }
  else {
    $Param->{Start} = time;
    while(<STDIN>) {
      $Param->{LineCount} ++ ;

      if ($Param->{ViewDate} ) {
	if (m/^(\d{6})\s+\d{1,2}:\d\d:\d\d\s.*$/) { # get date
	  #print "# $1 #\n";
	  if ($1 == $Param->{ViewDate}) {
	    $Param->{View} = 1;
	  }
	  else {
	    $Param->{View} = 0;
	  }
	}
      }
      if ($Param->{View} ) {
	#print "->>>$_";

	if (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Connect.+\s+on\s+(.*)$/i) { # get connection ID($2) and database($3)
	  #print "C-$1--$2--$3------\n";
	  RunQuery($Param);
	  if (defined $3) {
	    $Param->{CID}->{$2} = $3 ;
	    #print "DB:$Param->{CID}->{$2} .. $2 .. $3 \n";
	  }
	}

	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Connect.+$/i) { # get connection ID($2) and database($3)
	  #print "\n <<<<<<<<<<<<<<<<<<----------------------------<<<<<<<<<<<<<<<< \n";
	  #print "Connect \n";
	  RunQuery($Param);
	}
	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Change user .*\s+on\s+(.*)$/i) { # get connection ID($2) and database($3)
	  #print "C-$1--$2--$3------\n";
	  RunQuery($Param);
	  if (defined $3) {
	    $Param->{CID}->{$2} = $3 ;
	    #print "DB:$Param->{CID}->{$2} .. $2 .. $3 \n";
	  }
	}

	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Quit\s+$/i) { # remove connection ID($2) and querystring
	  #print "Q-$1--$2--------\n";
	  RunQuery($Param);
	  delete $Param->{CID}->{$2} ;
	}

	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Query\s+(select.+)$/i) { # get connection ID($2) and querystring
	  #print "S1-$1--$2--$3------\n";
	  RunQuery($Param);
	  unless ($Param->{CID}->{$2}) {
	    #print "Error: No Database for Handle: $2 found\n";
	  }
	  else {
	    $Param->{DB}=$Param->{CID}->{$2};

	    my $s = "$3";
	    $s =~ s/from\s/from $Param->{DB}./i;
	    $Param->{Query}="EXPLAIN $s";

	    #$s =~ m/from\s+(\w+[.]\w+)/i;
	    #$Param->{tab} =$1;
	    #print "-- $Param->{tab} -- $s --\n";
	  }
	}

	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Query\s+(update.+)$/i) { # get connection ID($2) and querystring
	  #print "S2--$1--$2--$3------\n";
	  RunQuery($Param);
	  unless ($Param->{CID}->{$2}) {
	    #print "Error: No Database for Handle: $2 found\n";
	  }
	  else {
	    $Param->{DB}=$Param->{CID}->{$2};

	    my $ud = $3;
	    $ud =~ m/^update\s+(\w+).+(where.+)$/i;
	    $Param->{Query} ="EXPLAIN SELECT * FROM $1 $2";
	    $Param->{Query} =~ s/from\s/from $Param->{DB}./i;

	    #$Param->{Query} =~ m/from\s+(\w+[.]\w+)/i;
	    #$Param->{tab} =$1;
	  }
	}

	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Statistics\s+(.*)$/i) { # get connection ID($2) and info?
	  $Param->{Statistics} ++;
	  #print "Statistics--$1--$2--$3------\n";
	  RunQuery($Param);
	}
	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Query\s+(.+)$/i) { # get connection ID($2)
	  $Param->{QueryCount} ++;
	  #print "Query-NULL $3\n";
	  RunQuery($Param);
	}
	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Refresh\s+(.+)$/i) { # get connection ID($2)
	  $Param->{Refresh} ++;
	  #print "Refresh\n";
	  RunQuery($Param);
	}
	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Init\s+(.+)$/i) { # get connection ID($2)
	  $Param->{Init} ++;
	  #print "Init $3\n";
	  RunQuery($Param);
	}
	elsif (m/^(\d{6}\s+\d{1,2}:\d\d:\d\d\s+|\s+)(\d+)\s+Field\s+(.+)$/i) { # get connection ID($2)
	  $Param->{Field} ++;
	  #print "Field $3\n";
	  RunQuery($Param);
	}

	elsif (m/^\s+(.+)$/ ) { # command could be some lines ...
	  #print "multi-lined ($1)\n";
	  my ($A)=$1;
 	  chomp $A;
	  $Param->{Query} .= " $1";
	  #print "multi-lined ($1)<<$Param->{Query}>>\n";
	}


      }

    }

    $Param->{dbh}->disconnect();

    if (1 == 0) {
      print "\nunclosed handles----------------------------------------\n";
      my $count=0;
      foreach (sort keys %{$Param->{CID}}) {
	print "$count | $_ : $Param->{CID}->{$_} \n";
	$count ++;
      }
    }

    print "\nIndex usage ------------------------------------\n";
    foreach my $t (sort keys %{$Param->{Data}}) {
      print "\nTable\t$t: ---\n";
      foreach my $k (sort keys %{$Param->{Data}->{$t}}) {
	print " count\t$k:\n";
	my %h = %{$Param->{Data}->{$t}->{$k}};
	  foreach (sort {$h{$a} <=> $h{$b}} keys %h) {
	  print "  $Param->{Data}->{$t}->{$k}->{$_}\t$_\n";
	}
      }
    }

    $Param->{AllCount}=0;
    print "\nQueries causing table scans -------------------\n\n";
    foreach (@{$Param->{ALL}}) {
      $Param->{AllCount} ++;
      print "$_\n";
    }
    print "Sum: $Param->{AllCount} table scans\n";

    print "\nSummary ---------------------------------------\n\n";
    print "Select: \t$Param->{SelectCount} queries\n";
    print "Update: \t$Param->{UpdateCount} queries\n";
    print "\n";

    print "Init:   \t$Param->{Init} times\n";
    print "Field:  \t$Param->{Field} times\n";
    print "Refresh: \t$Param->{Refresh} times\n";
    print "Query:  \t$Param->{QueryCount} times\n";
    print "Statistics:\t$Param->{Statistics} times\n";
    print "\n";

    print "Logfile: \t$Param->{LineCount} lines\n";
    print "Started:  \t".localtime($Param->{Start})."\n";
    print "Finished:   \t".localtime(time)."\n";

  }
}


###########################################################################
#
#
#
sub RunQuery {
  my $Param = shift ;

  if (defined $Param->{Query}) {
    if (defined $Param->{DB} ) {

      $Param->{Query} =~ m/from\s+(\w+[.]\w+|\w+)/i;
      $Param->{tab} =$1;
      #print "||$Param->{tab} -- $Param->{Query}\n";

      my $sth=$Param->{dbh}->prepare("USE $Param->{DB}");
      if (DBI::err()) {
	if ($Param->{PrintError}) {print "Error: ".DBI::errstr()."\n";}
      }
      else {
	$sth->execute();
	if (DBI::err()) {
	  if ($Param->{PrintError}) {print "Error: ".DBI::errstr()."\n";}
	}
	else {
	  $sth->finish();

	  $sth=$Param->{dbh}->prepare($Param->{Query});
	  if (DBI::err()) {
	    if ($Param->{PrintError}) {print "Error: ".DBI::errstr()."\n";}
	  }
	  else {
	    #print "$Param->{Query}\n";
	    $sth->execute();
	    if (DBI::err()) {
	      if ($Param->{PrintError}) {print "[$Param->{LineCount}]<<$Param->{Query}>>\n";}
	      if ($Param->{PrintError}) {print "Error: ".DBI::errstr()."\n";}
	    }
	    else {
	      my $row = undef;
	      while ($row = $sth->fetchrow_hashref()) {
		$Param->{SelectCount} ++;

		if (defined $row->{Comment}) {
		  push (@{$Param->{Comment}}, "$row->{Comment}; $_; $Param->{DB}; $Param->{Query}");
		}
		foreach (@{$Param->{Rows}}) {
		  if (defined $row->{$_}) {
		    #if (($_ eq 'type' ) and ($row->{$_} eq 'ALL')) {
		    if ($row->{type} eq 'ALL') {
		      push (@{$Param->{ALL}}, "$row->{$_} $_ $Param->{DB} $Param->{Query}");
		      #print ">> $row->{$_} $_ $Param->{DB} $Param->{Query}\n";
		    }
		    $Param->{IdxUseCount} ++;
		    $Param->{Data}->{$Param->{tab}}->{$_}->{$row->{$_}} ++;
		  }
		}
	      }
	    }
	  }
	}
      }
      $sth->finish();
    }
    $Param->{Query} = undef ;
  }
}

###########################################################################
#
#
#
sub ShowOptions {
  print <<EOF;
Usage: $0 [OPTIONS] < LOGFILE

--date=YYMMDD       select only entrys of date
-d=YYMMDD
--host=HOSTNAME     db-host to ask
-h=HOSTNAME
--user=USERNAME     db-user
-u=USERNAME
--password=PASSWORD password of db-user
-p=PASSWORD

Read logfile from STDIN an try to EXPLAIN all SELECT statements. All UPDATE statements are rewritten to an EXPLAIN SELECT statement. The results of the EXPLAIN statement are collected and counted. All results with type=ALL are collected in an separete list. Results are printed to STDOUT.

EOF
}

1;

__END__

=pod

=head1 NAME

explain_log.pl

Feed a mysqld general logfile (created with mysqld --log) back into mysql
and collect statistics about index usage with EXPLAIN.

=head1 DISCUSSION

To optimize your indices, you have to know which ones are actually
used and what kind of queries are causing table scans. Especially
if you are generating your queries dynamically and you have a huge
amount of queries going on, this isn't easy.

Use this tool to take a look at the effects of your real life queries.
Then add indices to avoid table scans and remove those which aren't used.

=head1 USAGE

explain_log.pl [--date=YYMMDD] --host=dbhost] [--user=dbuser] [--password=dbpw] < logfile

--date=YYMMDD       select only entrys of date

-d=YYMMDD

--host=HOSTNAME     db-host to ask

-h=HOSTNAME

--user=USERNAME     db-user

-u=USERNAME

--password=PASSWORD password of db-user

-p=PASSWORD

=head1 EXAMPLE

explain_log.pl --host=localhost --user=foo --password=bar < /var/lib/mysql/mobile.log

=head1 AUTHOR

  Stefan Nitz
  Jan Willamowius <jan@mobile.de>, http://www.mobile.de

=head1 RECRUITING

If you are looking for a MySQL or Perl job, take a look at http://www.mobile.de
and send me an email with your resume (you must be speaking German!).

=head1 SEE ALSO

mysql documentation

=cut
