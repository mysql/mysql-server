use strict;
use warnings;
use LWP::Simple;
use DBI;
use Cache::Memcached;

# NOTE: Env variable MEMCACHED_PORT is mandatory
# for this perl script to work.
my $memcached_port=$ENV{'MEMCACHED_PORT'} or die;

my $memd = new Cache::Memcached {
  'servers' => [ "127.0.0.1:$memcached_port" ],
  'connect_timeout' => 20,
  'select_timeout' => 20
};

# /** Retrieve SDI stored in a tablespace for single copy.
# @param[in]	type_full	SDI type in format "sdi_<number>"
# @param[in]	id		SDI id
# @param[in]	expected_data	expected string to compare with SDI retrieved
#				from tablespace. */
sub sdi_get_from_copy {
	if (scalar(@_) ne 3) {
		die "Wrong number of arguments passed."
		. " Expected args: (id_full, type, expected_data)\n";
	}
	my $type_full = $_[0];
	my $id = $_[1];
	my $expected_data = $_[2];

	my $val = $memd->get("$type_full:$id");
	my $cmp_result = $expected_data eq $val;

	if (!$cmp_result) {
		print "input and output mismatch for rec($type_full:$id)";
		print "input is $expected_data\n";
		print "output is $val\n";
	}
}

# /** Inserts SDI into both copies and retrieve SDI stored to compare with the
# given data.
# @param[in]	type		SDI type (just the number)
# @param[in]	id		SDI id
# @param[in]	data		data to be inserted */
sub sdi_set_get {
	if (scalar(@_) ne 3) {
		die "Wrong number of arguments passed."
		. " Expected args: (type, id, data)\n";
	}

	my $type = $_[0];
	my $type_full= "sdi_" . $type;
	my $id = $_[1];
	my $data = $_[2];

	if (!$memd->set("$type_full:$id", $data)) {
		print "Error: $type:$id|$data cannot be inserted.\n";
	}

	# Retrieve back and verify
	sdi_get_from_copy($type_full, $id, $data);
}

# /** Removes SDI from both copies and verifies the operation by expecting empty
# output on get.
# @param[in]	type	SDI type
# @param[in]	id	SDi id */
sub sdi_remove_get {

	if (scalar(@_) ne 2) {
		die "Wrong number of arguments passed."
		. " Expected args: (id, type)\n";
	}

	my $type = $_[0];
	my $type_full= "sdi_" . $type;
	my $id = $_[1];

	my $ret = $memd->delete("$type_full:$id");
	if (!$ret) {
		print "Error: rec($id:$type) cannot be deleted\n";
	}

	# Retrieve from copy
	my $val = $memd->get("$type_full:$id");
	if ($val) {
		print "Deleted but rec($type:$id) still exists with value:$val\n";
	}
}

# /** Retrieve SDI from copy.
# @param[in]	type	SDI type
# @param[in]	id 	SDI id */
sub sdi_get {

	if (scalar(@_) ne 2) {
		die "Wrong number of arguments passed."
	        . " Expected args: (type, id)\n";
	}

	my $type = $_[0];
	my $type_full= "sdi_" . $type;
	my $id = $_[1];

	my $val = $memd->get("$type_full:$id");
	# We don't print type & id because we use TABLE_ID as SDI KEY &
	# type.
	print "Get rec(type:id) is $val\n";
}

# /** Create SDI index in a tablespace. */
sub sdi_create_index {
	$memd->get("sdi_create_");
}

# /** Removes SDI indexes in a tablespace. */
sub sdi_drop_index {
	$memd->get("sdi_drop_");
}

# /** Select the table on which SDI operations should happen.
# @param[in]	table_name	table name */
sub sdi_switch_table {
	if (scalar(@_) ne 1) {
		die "Wrong number of arguments passed."
		. " Expected args: (table_name)\n";
	}
	my $table_name = $_[0];
	$memd->get("\@\@$table_name");
}

# /** Retrieves SDI keys only without data. (1:1|2:4|..) */
sub sdi_get_only_keys {
	return $memd->get("sdi_list_");
}

# /** Disconnect the current memcached connection. */
sub sdi_disconnect {
	$memd->disconnect_all;
}

# "require sdi_perl_func.pl" says the module should return true and this
# achieved by the below statement
1;
