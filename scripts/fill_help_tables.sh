#!@PERL@
#
# Copyright (C) 2003 MySQL AB
# For a more info consult the file COPYRIGHT distributed with this file.
#
# This script generates the SQL statements required by mysql_install_db to
# fill up the tables for the server-side online function help, which can be
# invoked with "help <function>" from the MySQL client.
#
# Usage:
#   fill_help_tables OPTIONS  < manual.texi > fill_help_tables.sql
#  
#  --help           display this helpscreen and exit
#  --verbose        print information about help completeness to STDERR
#  --lexems=path    path to file with lexems. it is used with verbose option.
#                       default value is ../sql/lex.h
# Examples:
#  ./fill_help_tables --help
#  ./fill_help_tables --verbose < manual.texi > fill_help_tables.sql
#  ./fill_help_tables < manual.texi > fill_help_tables.sql
# 
# Please note, that you first need to update Docs/manual.texi with the
# manual file from the separate "mysqldoc" BitKeeper-Tree! The manual.texi
# included in the source tree is just an empty stub file - the full manual
# is now maintained in a separate tree.
#
# extra tags in manual.texi:
#
#	@c help_category <category_name>[@<parent_category_name>]
#
#	@c description_for_help_topic <topic_name>  <keyword1> <keyword2>
#		....
#	@c end_description_for_help_topic
#
#	@c example_for_help_topic <topic_name>
#	@example
#		....
#	@end example
#
#
# Original version by Victor Vagin <vva@mysql.com>
#

use strict;
use Getopt::Long;

my $insert_portion_size= 15;
my $error_prefix= "---- help parsing errors :";

my $path_to_lex_file= "../sql/lex.h";
my $verbose_option= 0;
my $help_option= 0;

my $cur_line= 0;
my $count_errors= 0;

GetOptions(
  "help",\$help_option,
  "verbose",\$verbose_option,
  "lexems=s",\$path_to_lex_file
);

if ($help_option ne 0)
{
  print <<_HELP;

This script generates the SQL statements required by mysql_install_db to
fill up the tables for the server-side online function help, which can be
invoked with "help <function>" from the MySQL client.

Usage:
  fill_help_tables OPTIONS  < manual.texi > fill_help_tables.sql
  
  --help           display this helpscreen and exit
  --verbose        print information about help completeness to STDERR
  --lexems=path    path to file with lexems. it is used with verbose option.
                       default value is ../sql/lex.h

Examples:
  ./fill_help_tables --help
  ./fill_help_tables --verbose < manual.texi > fill_help_tables.sql
  ./fill_help_tables < manual.texi > fill_help_tables.sql
    
_HELP
  exit;
}

my $current_category= "";
my $current_parent_category= "";
my $next_example_for_topic= "";

my %topics; 
my %categories;
my %keywords;

$categories{Contents}->{__parent_category__}= "";

sub print_error
{
  my ($text)= @_;
  if ($count_errors==0)
  {
    print STDERR "$error_prefix\n";
  }
  print STDERR "line $cur_line : $text";
  $count_errors++;
}

sub add_topic_to_category
{
  my ($topic_name)= @_;

  $categories{$current_category}->{$topic_name}= $topics{$topic_name};
  my $category= $categories{$current_category};
  $category->{__name__}= $current_category;
    
  if (exists($category->{__parent_category__}))
  {
    my $old_parent= $category->{__parent_category__};
    if ($old_parent ne $current_parent_category)
    {
      print_error "wrong parent for $current_category\n";
    }
  }

  if ($current_parent_category ne "")
  {
    $category->{__parent_category__}= $current_parent_category;
  }
    
  if (exists($topics{$topic_name}->{category}))
  {
    my $old_category= $topics{$topic_name}->{category};
    if ($old_category ne $category)
    {
      print_error "wrong category for $topic_name (first one's \"$old_category->{__name__}\" second one's \"$current_category\")\n";
    }
  }
    
  $topics{$topic_name}->{category}= $category;    
}

sub add_example
{
  my ($topic_name,$example)= @_;
    
  $topic_name=~ tr/a-z/A-Z/;

  if (exists($topics{$topic_name}->{example}))
  {
    print_error "double example for $topic_name\n";
  }
    
  $topics{$topic_name}->{example}= $example;    
  add_topic_to_category($topic_name);
}

sub add_description
{
  my ($topic_name,$description)= @_;
    
  $topic_name=~ tr/a-z/A-Z/;
    
  if (exists($topics{$topic_name}->{description}))
  {
    print_error "double description for $topic_name\n";
  }
  $topics{$topic_name}->{description}= $description;
  add_topic_to_category($topic_name);
}

sub add_keyword
{
  my ($topic_name,$keyword)= @_;
    
  $topic_name=~ tr/a-z/A-Z/;
  $keyword=~ tr/a-z/A-Z/; 
    
  push(@{$topics{$topic_name}->{keywords}},$keyword);
  if (exists($keywords{$keyword}->{$topic_name}))
  {
    print_error "double keyword $keyword for $topic_name\n";
  }
  $keywords{$keyword}->{$topic_name}= $topics{$topic_name};
}

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
  $a =~ s/\@minus\{\}/-/g;
  $a =~ s/\@dots\{\}/.../g;
  $a =~ s/\@var\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@command\{((.|\n)+?)\}/$1/go;
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
  $a =~ s/\"/\\\"/g;

  $a =~ s/((\w|\s)+)\(([\+-=><\/%*!<>\s]+)\)/$3/gxs;
  $a =~ s/([\+-=><\/%*!<>\s]+)\(((\w|\s)+)\)/$1/gxs;
  $a =~ s/((\w|\s)+)\((.+)\)/$1/gxs;
  
  $a =~ s/((\s)+)$//g;
												    
  return $a;
}

sub prepare_description
{
  my ($a)= @_;

  $a =~ s/(\@itemize \@bullet\n)//g;
  $a =~ s/(\@c help_keyword (.*?)\n)//g;
  $a =~ s/(\@end itemize\n)//g;
  $a =~ s/(\@end example\n)//g;
  $a =~ s/(\@example\n)//g;
  $a =~ s/(\@{)/{/g;
  $a =~ s/(\@})/}/g;
  $a =~ s/(\@end multitable)/  /g;
  $a =~ s/(\@end table)/  /g;
  $a =~ s/(\@cindex(.*?)\n)//g;
  $a =~ s/(\@findex(.*?)\n)//g;
  $a =~ s/(\@table(.*?)\n)//g;
  $a =~ s/(\@multitable \@columnfractions(.*?)\n)/  /g;
  $a =~ s/(\@node(.*?)\n)/  /g;
  $a =~ s/(\@tab)/\t/g;
  $a =~ s/\@itemx/  /g;
  $a =~ s/(\@item\n(\s*?))(\S)/ --- $3/g;
  $a =~ s/(\@item)/  /g;
  $a =~ s/(\@tindex\s(.*?)\n)//g;
  $a =~ s/(\@c\s(.*?)\n)//g;
  $a =~ s/\@minus\{\}/-/g;
  $a =~ s/\@dots\{\}/.../g;
  $a =~ s/\@var\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@command\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@code\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@strong\{(.+?)\}/$1/go;
  $a =~ s/\@samp\{(.+?)\}/$1/go;
  $a =~ s/\@emph\{((.|\n)+?)\}/\/$1\//go;
  $a =~ s/\@xref\{((.|\n)+?)\}/See also : [$1]/go;
  $a =~ s/\@ref\{((.|\n)+?)\}/[$1]/go;
  $a =~ s/\@w\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@strong\{((.|\n)+?)\}/\n!!!!\n$1\n!!!!\n/go;
  $a =~ s/\@file\{((.|\n)+?)\}/\*$1/go;
  $a =~ s/\\/\\\\/g;
  $a =~ s/\n\n$/\n/g;
  $a =~ s/\n\n$/\n/g;
  $a =~ s/\n\n$/\n/g;
  $a =~ s/\n\n$/\n/g;
  $a =~ s/\n\n$/\n/g;
  $a =~ s/\n/\\n/g;
  $a =~ s/\"/\\\"/g;

  $a =~ s/\@table \@code/  /g;

  return $a;
}

sub prepare_example
{
  my ($a)= @_;

  $a =~ s/(^\@c for_help_topic(.*?)\n)//g;

  $a =~ s/\@var\{((.|\n)+?)\}/$1/go;
  $a =~ s/\@dots\{\}/.../g;
  $a =~ s/\\/\\\\/g;
  $a =~ s/(\@{)/{/g;
  $a =~ s/(\@})/}/g;
  $a =~ s/(\@\@)/\@/g;
  $a =~ s/(\n*?)$//g;
  $a =~ s/\n/\\n/g;
  $a =~ s/\"/\\\"/g;
    
  return $a;
}

sub parse_example
{
  return if (!($_=~/\@example/));
  return if ($next_example_for_topic eq "");
    
  my $topic_name= $next_example_for_topic;
  $next_example_for_topic= "";
  my $text= "";
    
  while (<>)
  {
    $cur_line++;
    last if ($_=~/\@end example/);
    $text .= $_;
  }
    
  $text= prepare_example($text);
  $topic_name= prepare_name($topic_name);
  add_example($topic_name,$text) if ($topic_name ne "");
}

sub parse_example_for_topic
{
  my ($for_topic)= m|\@c example_for_help_topic (.+?)$|;
  return if ($for_topic eq "");
    
  $next_example_for_topic= $for_topic;    
}

sub parse_description
{
  my ($topic_description)= m|\@c description_for_help_topic (.+?)$|;
  return if ($topic_description eq "");
    
  my ($topic_name,$topic_keywords)= split(/  /,$topic_description);
    
  if ($topic_name eq "" || $topic_keywords eq "")
  {
    $topic_name= $topic_description;
  }
  else
  {
    my $keyword;
    foreach $keyword (split(/ /,$topic_keywords))
    {
      add_keyword($topic_name,$keyword) if ($keyword ne "");
    }
  }
    
  my $text= "";
    
  while (<>)
  {
    $cur_line++;
    last if ($_=~/\@c end_description_for_help_topic/);
    $text .= $_;
  }
    
  $text= prepare_description($text);
  $topic_name= prepare_name($topic_name);
  add_description($topic_name,$text);
}

sub parse_category
{
  my ($c_name,$pc_name)= m|\@c help_category (.+?)\@(.+?)$|;

  if ($pc_name ne "")
  {
    $current_category= prepare_name($c_name);
    $current_parent_category= prepare_name($pc_name);
  }
  else
  {
    my ($c_name)=m|\@c help_category (.+?)$|;
    return if ($c_name eq "");

    $current_category= prepare_name($c_name);
    $current_parent_category= "Contents"
  }
}

# parse manual:

while (<>)
{
  parse_example_for_topic ();
  parse_example           ();
  parse_description       ();	
  parse_category          ();
  $cur_line++;
}

# test results of parsing:

sub print_bad_names
{
  my($names,$prompt)= @_;
  if (scalar(@{$names}))
  {
    print STDERR "\n-------------- $prompt : \n\n";
    my $name;
    foreach $name (@{$names})
    {
      print STDERR "$name\n";
    }
    print STDERR "\n";
  }
}

sub print_verbose_errors
{
  my($name_of_log_file)= @_;

  my @without_help;
  my @description_with_at;
  my @example_with_at;
  my @without_description;
  my @without_example;
    
  print STDERR "\n-------------- parameters of help completeness : \n\n";
    
  my $count_lex= 0;
  if (!open (TLEX,"<$path_to_lex_file"))
  {
    print STDERR "Error opening lex file \"$path_to_lex_file\" $!\n";
  }
  else
  {	
    for (<TLEX>)
    {
      my ($a,$lex,$b)=m|(.+?)\"(.+?)\"(.+?)$|;
      next if ($lex eq "");
      $count_lex++;
      next if (exists($topics{$lex}) || exists($keywords{$lex}));
      push(@without_help,$lex);
    }
    close(TLEX);
    print STDERR "number of lexems in \"$path_to_lex_file\" - $count_lex\n";
  }
    
  my $name;
  my @topic_names= keys(%topics);
  foreach $name (@topic_names)
  {
    my $topic= $topics{$name};
    push(@description_with_at,$name) if ($topic->{description}=~/\@/);
    push(@example_with_at,$name) if ($topic->{example}=~/\@/);
    push(@without_description,$name) if (!exists($topic->{description}));
    push(@without_example,$name) if (!exists($topic->{example}));
  }
    
  my $count_categories= scalar(keys(%categories));
  print STDERR "number of help categories          - ",$count_categories,"\n";
  my $count_topics= scalar(@topic_names);
  print STDERR "number of help topics              - ",$count_topics,"\n";
  my $count_keywords= scalar(keys(%keywords));
  print STDERR "number of help keywords            - ",$count_keywords,"\n";
    
  my $count_without_help= scalar(@without_help);
  print_bad_names(\@without_help,"lexems without help (".
                            $count_without_help." ~ ".
                            (int (($count_without_help/$count_lex)*100)).
                            "%)");
  print_bad_names(\@description_with_at,
          " topics below have symbol \'@\' in their descriptions.\n".
          "it's probably the litter from 'texi' tags (script needs fixing)");
  print_bad_names(\@example_with_at,
          " topics below have symbol \'@\' in their examples.\n".
          "it's probably the litter from 'texi' tags (script needs fixing)");
  print_bad_names(\@without_description,"topics without description");
    
  my $count_without_example= scalar(@without_example);
  print_bad_names(\@without_example,"topics without example (".
                            $count_without_example." ~ ".
                            (int (($count_without_example/$count_topics)*100)).
                            "%)");
}

print_verbose_errors if ($verbose_option ne 0);

# output result 

sub print_insert_header
{
  my($count,$header)= @_;
    
  if ($count % $insert_portion_size ne 0) {
    print ",";
  } else {
    print ";\n" if ($count ne 0);
    print "$header";
  }
}

print "delete from help_topic;\n";
print "delete from help_category;\n";
print "delete from help_keyword;\n";
print "delete from help_relation;\n\n";

my @category_names= keys(%categories);
if (scalar(@category_names))
{
  my $cat_name;
  my $count= 0;
  foreach $cat_name (@category_names)
  {
    $categories{$cat_name}->{__id__}= $count;
    $count++;
  }

  my $header= "insert into help_category ".
            "(help_category_id,name,parent_category_id) values ";
  $count= 0;
  foreach $cat_name (@category_names)
  {
    print_insert_header($count,$header);
    my $parent_cat_name= $categories{$cat_name}->{__parent_category__};
    my $parent_cat_id= $parent_cat_name eq "" 
        ? "-1" : $categories{$parent_cat_name}->{__id__};
    print "($count,\"$cat_name\",$parent_cat_id)";
    $count++;
  }
  printf ";\n\n";
}

my @topic_names= keys(%topics);
if (scalar(@topic_names))
{
  my $header= "insert into help_topic ".
      "(help_topic_id,help_category_id,name,description,example) values ";
  my $topic_name;
  my $count= 0;
  foreach $topic_name (@topic_names)
  {
    print_insert_header($count,$header);
    my $topic= $topics{$topic_name};
    print "($count,";
    print "$topic->{category}->{__id__},";
    print "\"$topic_name\",";
    print "\"$topic->{description}\",";
    print "\"$topic->{example}\")";
    $topics{$topic_name}->{__id__}= $count;
    $count++;
  }
  printf ";\n\n";
}

my @keywords_names= keys(%keywords);
if (scalar(@keywords_names))
{
  my $header= "insert into help_keyword (help_keyword_id,name) values ";
  my $keyword_name;
  my $count= 0;
  foreach $keyword_name (@keywords_names)
  {
    print_insert_header($count,$header);
    print "($count,\"$keyword_name\")";
    $count++;
  }
  printf ";\n\n";
    
  $header= "insert into help_relation ".
	"(help_topic_id,help_keyword_id) values ";
  $count= 0;
  my $count_keyword= 0;
  foreach $keyword_name (@keywords_names)
  {
    my $topic_name;
    foreach $topic_name (keys(%{$keywords{$keyword_name}}))
    {
      print_insert_header($count,$header);
      print "($topics{$topic_name}->{__id__},$count_keyword)";
      $count++;
    }
    $count_keyword++;
  }
  printf ";\n\n";
}

if ($count_errors)
{
  print STDERR "$count_errors errors !!!\n";
  exit 1;
}
