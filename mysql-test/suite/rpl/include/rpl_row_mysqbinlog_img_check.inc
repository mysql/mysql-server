-- perl END_OF_FILE

sub match_imgs
{
  my ($contents, $ai_ref, $bi_ref, $stmt_type) = @_;

  my $match= 0;
  my $error= 0;

  %ai= %{$ai_ref};
  %bi= %{$bi_ref};

  my $composed_bi= "";
  for my $k (sort (keys(%bi)))
  {
    $composed_bi = $composed_bi . "###   \@$k=" . $bi{$k} . "\n";
  }

  my $composed_ai= "";
  for my $k (sort (keys(%ai)))
  {
    $composed_ai = $composed_ai . "###   \@$k=" . $ai{$k} . "\n";
  }

  if($stmt_type=~/INSERT/)
  {
    $match= ($contents =~ /### INSERT.*\n### SET\n$composed_ai# at/m) ? 1 : 0;
  }
  elsif ($stmt_type=~/UPDATE/)
  {
    $match= ($contents =~ /### UPDATE.*\n### WHERE\n$composed_bi### SET\n$composed_ai# at/m) ? 1 : 0;
  }
  elsif ($stmt_type=~/DELETE/)
  {
    $match= ($contents =~ /### DELETE.*\n### WHERE\n$composed_bi# at/m) ? 1 : 0;
  }
  else
  {
    $error= 1;
  }

  if ($error)
  {
    print "Unexpected statement type: $stmt_type.\n"
  }

  if (!$match)
  {
    print "Unexpected columns in $stmt_type.\n";
    print "EXPECTED BEFORE IMAGE:\n$composed_bi\nEXPECTED AFTER IMAGE:\n$composed_ai\nBINLOG CONTENTS:" . $contents;
  }

  return $match;
}


my $ai;
my $bi;

if (not defined $ENV{'IMG_AI_EXPECTED'})
{
  $ai= {};
}
else
{
  $ai= eval ($ENV{'IMG_AI_EXPECTED'}) or die("Invalid AI parameter: ".  $ENV{'IMG_AI_EXPECTED'});
}

if (not defined $ENV{'IMG_BI_EXPECTED'})
{
  $bi= {};
}
else
{
  $bi= eval ($ENV{'IMG_BI_EXPECTED'}) or die("Invalid BI parameter: ".  $ENV{'IMG_BI_EXPECTED'});
}

my $binlog= $ENV{'IMG_BINLOG_FILE'};
my $stmt_type= $ENV{'IMG_STMT_TYPE'};

open(FILE, "$binlog") or die("Unable to open $binlog: $!\n");
my $contents = do { local $/; <FILE> };
close(FILE) or die("Unable to close file.");

match_imgs($contents, $ai, $bi, $stmt_type);

END_OF_FILE
