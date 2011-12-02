# open p_lex.h file to write. 
open OUTFILE,">", "p_lex.h";

print OUTFILE "/*\n This is an auto generated file. Please do not modify it directly.\n*/";
print OUTFILE "\n\n\n";

# open sql_yacc.h file to read. 
open INFILE, "../../sql/sql_yacc.h" or die $!;
my @lines = <INFILE>;

# reading Tokens' definition from sql_yacc.h and writing them to p_lex.h.
$found= 0;
foreach $line(@lines)
{
  if($line =~ /define ABORT_SYM 258/)
  {
    print OUTFILE "/* Tokens' Definitions start.*/\n";
    print OUTFILE "$line";
    $found= 1;
    next;
  }
  if($found && $line eq "\n")
  {
    print $line;
    print OUTFILE "/* Tokens' Definitions end.*/\n\n";
    $found= 0;
    last;
  }
  if($found)
  {
    print OUTFILE "$line";
    next;
  }
}
print OUTFILE "\n\nvoid initialize_lex_symbol();\n";
close(INFILE);
close(OUTFILE);

# open p_lex.h file to write. 
open OUTFILE,">", "p_lex.cc";

print OUTFILE "/*\n This is an auto generated file. Please do not modify it directly.\n*/";
print OUTFILE "\n\n\n";
print OUTFILE "#include \"pfs_digest.h\"\n";

# open file sql_lex.h to read. 
open INFILE, "../../sql/lex.h" or die $!;
my @lines = <INFILE>;

print OUTFILE "\nextern const char* symbol[];\n\n";
print OUTFILE "void initialize_lex_symbol()\n{\n";

$found= 0;
foreach $line(@lines)
{
  if($found)
  {
    @arr1= split('"',$line); 
    @arr2= split('\(',$line);
    @arr3= split('\)',$arr2[1]);
    if($arr3[0] ne "") 
    {
      print OUTFILE "  symbol[$arr3[0]-START_TOKEN_NUMBER]= (char*)\"$arr1[1]\";\n";
    }
    next;
  }
  if($line =~ /static SYMBOL symbols\[\]/)
  {
    #print $line;
    $found= 1;
  }
}
print OUTFILE "}\n";

close(INFILE);
close OUTFILE;
