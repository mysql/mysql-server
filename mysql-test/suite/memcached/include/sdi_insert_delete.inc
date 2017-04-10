perl;
use strict;
use warnings;
use Cwd;

my $srcdir = getcwd;
require "$srcdir/include/sdi_perl_func.pl";

print "1. Insert Small data\n";
sdi_set_get(0, 0, "Hello How are you?");

print "2. Insert Large data. Will be stored in external pages\n";
my $input = "abcdefghijklmnopqrstuvwxyz" x 800;
sdi_set_get(1, 1, $input);

print "3. Move off-page to in-page\n";
sdi_set_get(1, 1, "Reducing from off page to in-page");

print "4. Move in-page to off-page\n";
sdi_set_get(1, 1, $input);

print "5. Insert 1000 rows and delete\n";
$input = "abcdef";
for (my $i=2; $i <= 1001; $i++) {
	sdi_set_get($i, $i, $input x $i);
}

# The list of SDI keys
my $input_list;
for (my $i=0; $i <=1001; $i++) {
	$input_list .= "$i:$i|";
}

# Retrieve only SDI Keys
my $out = sdi_get_only_keys();

my $list_cmp_result = $input_list eq $out;

if (!$list_cmp_result) {
	print "List Keys mismatch\n";
	print "input is $input_list\n";
	print "output is $out\n";
}

for (my $i=0; $i <= 1001; $i++) {
	sdi_remove_get($i, $i);
}

sdi_disconnect();
EOF
