#!@PERL@
#
# Copyright (C) 2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file.
#
# fill_func_tables - parse ../Docs/manual.texi 
#
# Original version by Victor Vagin <vva@mysql.com>
#

my $cat_name= "";
my $func_name= "";
my $text= "";
my $example= "";

local $mode= "";

sub prepare_name
{
  my ($a)= @_;

  $a =~ s/(\@itemize \@bullet)/  /g;
  $a =~ s/(\@end itemize)/  /g;
  $a =~ s/(\@end multitable)/  /g;
  $a =~ s/(\@end table)/  /g;
  $a =~ s/(\@cindex(.*?)\n)/  /g;
  $a =~ s/(\@multitable \@columnfractions(.*?)\n)/  /g;
  $a =~ s/(\@node(.*?)\n)/  /g;
  $a =~ s/(\@tab)/\t/g;
  $a =~ s/\@item/  /g;
  $a =~ s/\@code\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@strong\{(.+?)\}/$1/go;
  $a =~ s/\@samp\{(.+?)\}/$1/go;
  $a =~ s/\@emph\{((.|\n)+?)\}/\/$1\//go;
  $a =~ s/\@xref\{((.|\n)+?)\}/See also : [$1]/go;
  $a =~ s/\@ref\{((.|\n)+?)\}/[$1]/go;
  $a =~ s/\'/\'\'/g;
  $a =~ s/\\/\\\\/g;
  $a =~ s/\`/\`\`/g;

  $a =~ s/\@table \@code/  /g;

  $a =~ s/\(\)//g;

  $a =~ s/((\w|\s)+)\(([\+-=><\/%*!<>\s]+)\)/$3/gxs; #$a =~ s/((\w|\s)+)\(([\+-=><\/%*!<>\s]+)\)/$3 $1/gxs;
  $a =~ s/([\+-=><\/%*!<>\s]+)\(((\w|\s)+)\)/$1/gxs;#$a =~ s/([\+-=><\/%*!<>\s]+)\(((\w|\s)+)\)/$1 $2/gxs;
  $a =~ s/((\w|\s)+)\((.+)\)/$1/gxs;

  return $a;
}

sub prepare_text
{
  my ($a)= @_;

  $a =~ s/(\@itemize \@bullet)/  /g;
  $a =~ s/(\@end itemize)/  /g;
  $a =~ s/(\@end multitable)/  /g;
  $a =~ s/(\@end table)/  /g;
  $a =~ s/(\@cindex(.*?)\n)/  /g;
  $a =~ s/(\@multitable \@columnfractions(.*?)\n)/  /g;
  $a =~ s/(\@node(.*?)\n)/  /g;
  $a =~ s/(\@tab)/\t/g;
  $a =~ s/\@itemx/  /g;
  $a =~ s/\@item/  /g;
  $a =~ s/\@code\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@strong\{(.+?)\}/$1/go;
  $a =~ s/\@samp\{(.+?)\}/$1/go;
  $a =~ s/\@emph\{((.|\n)+?)\}/\/$1\//go;
  $a =~ s/\@xref\{((.|\n)+?)\}/See also : [$1]/go;
  $a =~ s/\@ref\{((.|\n)+?)\}/[$1]/go;
  $a =~ s/\'/\'\'/g;
  $a =~ s/\\/\\\\/g;
  $a =~ s/\`/\`\`/g;
  $a =~ s/(\n*?)$//g;
  $a =~ s/\n/\\n/g;

  $a =~ s/\@table \@code/  /g;

  return $a;
}

sub prepare_example
{
  my ($a)= @_;

  $a =~ s/\'/\'\'/g;
  $a =~ s/\\/\\\\/g;
  $a =~ s/\`/\`\`/g;
  $a =~ s/(\n*?)$//g;
  $a =~ s/\n/\\n/g;

  return $a;
}

sub flush_all
{
  my ($mode) = @_;

  if ($mode eq ""){return;}

  $func_name= prepare_name($func_name);
  $text= prepare_text($text);
  $example= prepare_example($example);

  if ($func_name ne "" && $text ne "" && !($func_name =~ /[abcdefghikjlmnopqrstuvwxyz]/)){
    print "INSERT INTO function (name,description,example) VALUES (";
    print "'$func_name',";
    print "'$text',";
    print "'$example'";
    print ");\n";
    print "INSERT INTO function_category (cat_id,func_id) VALUES (\@cur_category,LAST_INSERT_ID());\n";
  }

  $func_name= "";
  $text= "";
  $example= "";
  $mode= "";
}

sub new_category
{
  my ($category)= @_;

  $category= prepare_text($category);

  print "INSERT INTO function_category_name (name) VALUES (\'$category\');\n";
  print "SELECT \@cur_category:=LAST_INSERT_ID();\n";
}

print "INSERT INTO db (Host,DB,User,Select_priv) VALUES ('%','mysql_help','','Y');\n";
print "CREATE DATABASE mysql_help;\n";

print "USE mysql_help;\n";

print "DROP TABLE IF EXISTS function;\n";
print "CREATE TABLE function (";
print "  func_id       int unsigned not null auto_increment,";
print "  name          char(64) not null,";
print "  url           char(128) not null,";
print "  description   text not null,";
print "  example       text not null,";
print "  min_args      tinyint not null,";
print "  max_args      tinyint,";
print "  date_created  datetime not null,";
print "  last_modified timestamp not null,";
print "  primary key   (func_id)";
print ") type=myisam;\n\n";

print "DROP TABLE IF EXISTS function_category_name;\n";
print "CREATE TABLE function_category_name (";
print "  cat_id        smallint unsigned not null auto_increment,";
print "  name          char(64) not null,";
print "  url           char(128) not null,";
print "  date_created  datetime not null,";
print "  last_modified timestamp not null,";
print "  primary key   (cat_id)";
print ") type=myisam;\n\n";

print "DROP TABLE IF EXISTS function_category;\n";
print "CREATE TABLE function_category (";
print "  cat_id        smallint unsigned not null references function_category_name,";
print "  func_id       int unsigned not null references function,";
print "  primary key   (cat_id, func_id)";
print ") type=myisam;\n\n";

print "DELETE FROM function_category_name;\n";
print "DELETE FROM function_category;\n";
print "DELETE FROM function;\n";
print "SELECT \@cur_category:=null;\n\n";

my $in_section_6_3= 0;

for(<>)
{
  if ($_=~/\@section Functions for Use in \@code{SELECT} and \@code{WHERE} Clauses/ &&
      !$in_section_6_3){
    $in_section_6_3= 1;
    next;
  }

  if ($_=~/\@section/ && $in_section_6_3){
    $in_section_6_3= 0;
    next;
  }

  if (!$in_section_6_3) { next; }

  my $c_name= "";

  ($c_name)=m|\@c for_mysql_help,(.+?)$|;
  if (!($c_name eq "") && ! ($c_name =~ m/$cat_name/i)){
    ($cat_name)= $c_name;
    new_category($cat_name);
    next;
  }

  ($c_name)=m|\@subsubsection (.+?)$|;
  if (!($c_name eq "") && ! ($c_name =~ m/$cat_name/i)){
    ($cat_name)= $c_name;
    new_category($cat_name);
    next;
  }

  ($c_name)=m|\@subsection (.+?)$|;
  if (!($c_name eq "") && ! ($c_name =~ m/$cat_name/i)){
    ($cat_name)= $c_name;
    new_category($cat_name);
    next;
  }

  ($f_name)=m|\@findex (.+?)$|;
  if (!($f_name eq "")){
    flush_all($mode);
    ($func_name)= ($f_name);
    $mode= "text";
    next;
  }

  if ($_=~/\@example/ && ($mode eq "text")){
    $mode= "example";
    next;
  }

  if ($_=~/\@end example/ && ($mode eq "example")){
    flush_all($mode);
    next;
  }

  if ($mode eq "text")    { $text    .= $_; }
  if ($mode eq "example") { $example .= $_; }
}


print "DELETE function_category_name ";
print "FROM function_category_name ";
print "LEFT JOIN function_category ON function_category.cat_id=function_category_name.cat_id ";
print "WHERE function_category.cat_id is null;"

