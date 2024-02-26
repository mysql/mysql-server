#
# Extract the contents of mysql.dd_propeties and format it in a
# human readable way. This contains various DD related information,
# such as version numbers (removed in the test output to be
# version agnostic), DD table definitions (to be used for opening
# DD tables during upgrade), and SE private data for the DD tables
# (to be used when DD tables are opened on server restart).
#
# Expects the variable $file to be set to indicate the output
# destination. Optionally accepts a filter to be applied.
#

--source include/have_debug.inc

if (!$file)
{
  die;
}

# Get hold of the unformatted dd properties.
set debug='+d,skip_dd_table_access_check';
--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR
eval SELECT * FROM mysql.dd_properties INTO OUTFILE '$file';
set debug='-d,skip_dd_table_access_check';

# Provide variables that we can use in the perl snippets.
--let ENV_FILTER = $filter
--let ENV_FILE = $file

# Break up the contents in pieces that are easier to read. Must
# be done in a couple of passes since we insert newlines. Also
# note that we must use -i.bak to allow inplace editing on win.
exec perl -p -i.bak -e
   "s/;/\n/g; s/\\\\*//g;
    s/SYSTEM_TABLES\=/SYSTEM_TABLES\=\n/g;
    s/\=col0/\=\n  col0/g;
    s/\=fields/\=\n    fields/g;
    s/\=elem0/\=\n      elem0/g"
  $file;

--remove_file $file.bak

exec perl -p -i.bak -e
   "s/^elem/      elem/g;
    s/^col/  col/g; s/^data/  data/g; s/^version/  version/g;
    s/^def/  def/g; s/^indexes/    indexes/g;
    s/^foreign_keys/    foreign_keys/g; s/^name/    name/g;
    s/^options/    options/g; s/^id/  id/g;
    s/^lbl/            lbl/g; s/^pos/            pos/g;
    s/^idx/  idx/g; s/^space_id/       space_id/g;
    s/^root/       root/g; s/^table_id/       table_id/g;
    s/^trx_id/       trx_id/g"
  $file;

--remove_file $file.bak

# Remove empty lines, and apply an optional filter to remove lines
# from the contents.
perl;
  use strict;
  use warnings;

  my $file = $ENV{'ENV_FILE'} or die;
  my $filter = $ENV{'ENV_FILTER'};

  open my $fh, '<', $file or die "unable to open file '$file' : $!";
  my @all_lines = grep(/./, <$fh>);
  close $fh;

  if (length $filter) {
    @all_lines = grep(!/$filter/, @all_lines);
  }

  open $fh, '>', $file or die "unable to open file '$file' : $!";
  print $fh @all_lines;
  close $fh;
EOF
