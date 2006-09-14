#!/usr/bin/perl
#
# This script generates defines for all functions
# in yassl/include/openssl/ so they are renamed to
# ya<old_function_name>. Hopefully that is unique enough.
#
# The script is to be run manually when we import
# a new version of yaSSL
#



# Find all functions in "input" and add macros
# to prefix/rename them into "output
sub generate_prefix($$)
{
  my $input= shift;
  my $output= shift;
  open(IN, $input)
      or die("Can't open input file $input: $!");
  open(OUT, ">", $output)
    or mtr_error("Can't open output file $output: $!");

  while (<IN>)
  {
    chomp;

    if ( /typedef/ )
    {
      next;
    }

    if ( /^\s*[a-zA-Z0-9*_ ]+\s+\*?([_a-zA-Z0-9]+)\s*\(/ )
    {
      print OUT "#define $1 ya$1\n";
    }
  }

  close OUT;
  close IN;
}

generate_prefix("ssl.h", "prefix_ssl.h");
generate_prefix("crypto.h", "prefix_crypto.h");

