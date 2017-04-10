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
# @param[in]	id_full		SDI id in format "sdi_<number>"
# @param[in]	type		SDI type
# @param[in] 	copy_num 	SDI copy number in a tablespace (0 or 1)
# @param[in] 	expected_data 	expected string to compare with SDI retrieved
#				from tablespace. */
sub sdi_get_from_copy {
	if (scalar(@_) ne 4) {
		die "Wrong number of arguments passed."
		. " Expected args: (id_full, type, copy_num, expected_data)\n";
	}
	my $id_full = $_[0];
	my $type = $_[1];
	my $copy_num = $_[2];
	my $expected_data = $_[3];

	my $val = $memd->get("$id_full:$type:$copy_num");
	my $cmp_result = $expected_data eq $val;

	if (!$cmp_result) {
		print "input and output mismatch for rec($id_full:$type)"
		. " from copy $copy_num\n";
		print "input is $expected_data\n";
		print "output is $val\n";
	}
}

# /** Inserts SDI into both copies and retrieve SDI stored to compare with the
# given data.
# @param[in]	id		SDI id (just the number)
# @param[in]	type		SDI type
# @param[in] 	data 		data to be inserted */
sub sdi_set_get {
	if (scalar(@_) ne 3) {
		die "Wrong number of arguments passed."
		. " Expected args: (id, type, data)\n";
	}

	my $id = $_[0];
	my $id_full= "sdi_" . $id;
	my $type = $_[1];
	my $data = $_[2];

	if (!$memd->set("$id_full:$type", $data)) {
		print "Error: $id:$type|$data cannot be inserted.\n";
	}

	# Retrieve from Copy 0
	sdi_get_from_copy($id_full, $type, 0, $data);
	# Retrieve from Copy 1
	sdi_get_from_copy($id_full, $type, 1, $data);
}

# /** Removes SDI from both copies and verifies the operation by expecting empty
# output on get.
# @param[in]	id	SDI id
# @param[in]	type	SDi type */
sub sdi_remove_get {

	if (scalar(@_) ne 2) {
		die "Wrong number of arguments passed."
		. " Expected args: (id, type)\n";
	}

	my $id = $_[0];
	my $id_full= "sdi_" . $id;
	my $type = $_[1];

	my $ret = $memd->delete("$id_full:$type");
	if (!$ret) {
		print "Error: rec($id:$type) cannot be deleted\n";
	}

	# Retrieve from copy 0
	my $val = $memd->get("$id_full:$type:0");
	if ($val) {
		print "Deleted but rec($id:$type:0) still exists with value:$val\n";
	}

	# Retrieve from copy 1
	$val = $memd->get("$id_full:$type:1");
	if ($val) {
		print "Deleted but rec($id:$type:1) still exists with value:$val\n";
	}
}

# /** Retrieve SDI from copy.
# @param[in]	id 	SDI id
# @param[in]	type	SDI type */
sub sdi_get {

	if (scalar(@_) ne 2) {
		die "Wrong number of arguments passed."
	        . " Expected args: (id, type)\n";
	}

	my $id = $_[0];
	my $id_full= "sdi_" . $id;
	my $type = $_[1];

	my $val = $memd->get("$id_full:$type");
	# We don't print id & type because we use TABLE_ID as SDI KEY &
	# type. 
	print "Get rec(id:type) is $val\n";
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
