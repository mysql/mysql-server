#!/my/gnu/bin/perl -w -*- perl -*-

# Generate a mirror listing

line: while (<>) { last line if /START_OF_MIRROR_LISTING/;};

print "MySQL mirror listing\n";

line: while (<>)
{
  last line if /END_OF_MIRROR_LISTING/; 
  if (/^\@strong\{([A-Za-z ]+):\}$/)
  {
    print "\n*** $1\n";
  }
  elsif (m|^\@image\{Img/[a-z-]+\} ([A-Za-z]+) \[(.*)\]|)
  {
    print "\n$1 [$2]\n";
  }
  # A hacky URL regexp
  # (m!^\@uref\{((http\|ftp)://[^,]*), (FTP\|WWW)\}!)
  elsif (m!^\@uref\{((http|ftp)://[^,]*), (FTP|WWW)\}!)
  {
    $addr = $1;
    print "	$addr\n";
  }
}
