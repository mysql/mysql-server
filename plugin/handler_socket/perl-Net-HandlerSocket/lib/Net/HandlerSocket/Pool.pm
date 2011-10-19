#!/usr/bin/perl

package Net::HandlerSocket::HSPool;

use strict;
use warnings;
use Net::HandlerSocket;
use Socket;

sub new {
	my $self = {
		config => $_[1],
		reopen_interval => 60,
		hostmap => { },
	};
	return bless $self, $_[0];
}

sub clear_pool {
	my ($self) = @_;
	$self->{hostmap} = { };
}

sub on_error {
	my ($self, $obj) = @_;
	my $error_func = $self->{config}->{error};
	if (defined($error_func)) {
		return &{$error_func}($obj);
	}
	die $obj;
}

sub on_warning {
	my ($self, $obj) = @_;
	my $warning_func = $self->{config}->{warning};
	if (defined($warning_func)) {
		return &{$warning_func}($obj);
	}
}

sub get_conf {
	my ($self, $dbtbl) = @_;
	my $hcent = $self->{config}->{hostmap}->{$dbtbl};
	if (!defined($hcent)) {
		$self->on_error("get_conf: $dbtbl not found");
		return undef;
	}
	my %cpy = %$hcent;
	$cpy{port} ||= 9998;
	$cpy{timeout} ||= 2;
	return \%cpy;
}

sub resolve_hostname {
	my ($self, $hcent, $host_ip_list) = @_;
	if (defined($host_ip_list)) {
		if (scalar(@$host_ip_list) > 0) {
			$hcent->{host} = shift(@$host_ip_list);
			return $host_ip_list;
		}
		return undef; # no more ip
	}
	my $host = $hcent->{host}; # unresolved name
	$hcent->{hostname} = $host;
	my $resolve_list_func = $self->{config}->{resolve_list};
	if (defined($resolve_list_func)) {
		$host_ip_list = &{$resolve_list_func}($host);
		if (scalar(@$host_ip_list) > 0) {
			$hcent->{host} = shift(@$host_ip_list);
			return $host_ip_list;
		}
		return undef; # no more ip
	}
	my $resolve_func = $self->{config}->{resolve};
	if (defined($resolve_func)) {
		$hcent->{host} = &{$resolve_func}($host);
		return [];
	}
	my $packed = gethostbyname($host);
	if (!defined($packed)) {
		return undef;
	}
	$hcent->{host} = inet_ntoa($packed);
	return [];
}

sub get_handle_exec {
	my ($self, $db, $tbl, $idx, $cols, $exec_multi, $exec_args) = @_;
	my $now = time();
	my $dbtbl = join('.', $db, $tbl);
	my $hcent = $self->get_conf($dbtbl); # copy
	if (!defined($hcent)) {
		return undef;
	}
	my $hmkey = join(':', $hcent->{host}, $hcent->{port});
	my $hment = $self->{hostmap}->{$hmkey};
		# [ open_time, handle, index_map, host, next_index_id ]
	my $host_ip_list;
	TRY_OTHER_IP:
	if (!defined($hment) ||
		$hment->[0] + $self->{reopen_interval} < $now ||
		!$hment->[1]->stable_point()) {
		$host_ip_list = $self->resolve_hostname($hcent, $host_ip_list);
		if (!defined($host_ip_list)) {
			my $hostport = $hmkey . '(' . $hcent->{host} . ')';
			$self->on_error("HSPool::get_handle" .
				"($db, $tbl, $idx, $cols): host=$hmkey: " .
				"no more active ip");
			return undef;
		}
		my $hnd = new Net::HandlerSocket($hcent);
		my %m = ();
		$hment = [ $now, $hnd, \%m, $hcent->{host}, 1 ];
		$self->{hostmap}->{$hmkey} = $hment;
	}
	my $hnd = $hment->[1];
	my $idxmap = $hment->[2];
	my $imkey = join(':', $idx, $cols);
	my $idx_id = $idxmap->{$imkey};
	if (!defined($idx_id)) {
		$idx_id = $hment->[4];
		my $e = $hnd->open_index($idx_id, $db, $tbl, $idx, $cols);
		if ($e != 0) {
			my $estr = $hnd->get_error();
			my $hostport = $hmkey . '(' . $hcent->{host} . ')';
			my $errmess = "HSPool::get_handle open_index" .
				"($db, $tbl, $idx, $cols): host=$hostport " .
				"err=$e($estr)";
			$self->on_warning($errmess);
			$hnd->close();
			$hment = undef;
			goto TRY_OTHER_IP;
		}
		$hment->[4]++;
		$idxmap->{$imkey} = $idx_id;
	}
	if ($exec_multi) {
		my $resarr;
		for my $cmdent (@$exec_args) {
			$cmdent->[0] = $idx_id;
		}
		if (scalar(@$exec_args) == 0) {
			$resarr = [];
		} else {
			$resarr = $hnd->execute_multi($exec_args);
		}
		my $i = 0;
		for my $res (@$resarr) {
			if ($res->[0] != 0) {
				my $cmdent = $exec_args->[$i];
				my $ec = $res->[0];
				my $estr = $res->[1];
				my $op = $cmdent->[1];
				my $kfvs = $cmdent->[2];
				my $kvstr = defined($kfvs)
					? join(',', @$kfvs) : '';
				my $limit = $cmdent->[3] || 0;
				my $skip = $cmdent->[4] || 0;
				my $hostport = $hmkey . '(' . $hcent->{host}
					. ')';
				my $errmess = "HSPool::get_handle execm" .
					"($db, $tbl, $idx, [$cols], " .
					"($idx_id), $op, [$kvstr] " .
					"$limit, $skip): " . 
					"host=$hostport err=$ec($estr)";
				if ($res->[0] < 0 || $res->[0] == 2) {
					$self->on_warning($errmess);
					$hnd->close();
					$hment = undef;
					goto TRY_OTHER_IP;
				} else {
					$self->on_error($errmess);
				}
			}
			shift(@$res);
			++$i;
		}
		return $resarr;
	} else {
		my $res = $hnd->execute_find($idx_id, @$exec_args);
		if ($res->[0] != 0) {
			my ($op, $kfvals, $limit, $skip) = @$exec_args;
			my $ec = $res->[0];
			my $estr = $res->[1];
			my $kvstr = join(',', @$kfvals);
			my $hostport = $hmkey . '(' . $hcent->{host} . ')';
			my $errmess = "HSPool::get_handle exec" .
				"($db, $tbl, $idx, [$cols], ($idx_id), " .
				"$op, [$kvstr], $limit, $skip): " .
				"host=$hostport err=$ec($estr)";
			if ($res->[0] < 0 || $res->[0] == 2) {
				$self->on_warning($errmess);
				$hnd->close();
				$hment = undef;
				goto TRY_OTHER_IP;
			} else {
				$self->on_error($errmess);
			}
		}
		shift(@$res);
		return $res;
	}
}

sub index_find {
	my ($self, $db, $tbl, $idx, $cols, $op, $kfvals, $limit, $skip) = @_;
	# cols: comma separated list
	# kfvals: arrayref
	$limit ||= 0;
	$skip ||= 0;
	my $res = $self->get_handle_exec($db, $tbl, $idx, $cols,
		0, [ $op, $kfvals, $limit, $skip ]);
	return $res;
}

sub index_find_multi {
	my ($self, $db, $tbl, $idx, $cols, $cmdlist) = @_;
	# cols : comma separated list
	# cmdlist : [ dummy, op, kfvals, limit, skip ]
	# kfvals : arrayref
	my $resarr = $self->get_handle_exec($db, $tbl, $idx, $cols,
		1, $cmdlist);
	return $resarr;
}

sub result_single_to_arrarr {
	my ($numcols, $hsres, $ret) = @_;
	my $hsreslen = scalar(@$hsres);
	my $rlen = int($hsreslen / $numcols);
	$ret = [ ] if !defined($ret);
	my @r = ();
	my $p = 0;
	for (my $i = 0; $i < $rlen; ++$i) {
		my @a = splice(@$hsres, $p, $numcols);
		$p += $numcols;
		push(@$ret, \@a);
	}
	return $ret; # arrayref of arrayrefs
}

sub result_multi_to_arrarr {
	my ($numcols, $mhsres, $ret) = @_;
	$ret = [ ] if !defined($ret);
	for my $hsres (@$mhsres) {
		my $hsreslen = scalar(@$hsres);
		my $rlen = int($hsreslen / $numcols);
		my $p = 0;
		for (my $i = 0; $i < $rlen; ++$i) {
			my @a = splice(@$hsres, $p, $numcols);
			$p += $numcols;
			push(@$ret, \@a);
		}
	}
	return $ret; # arrayref of arrayrefs
}

sub result_single_to_hasharr {
	my ($names, $hsres, $ret) = @_;
	my $nameslen = scalar(@$names);
	my $hsreslen = scalar(@$hsres);
	my $rlen = int($hsreslen / $nameslen);
	$ret = [ ] if !defined($ret);
	my $p = 0;
	for (my $i = 0; $i < $rlen; ++$i) {
		my %h = ();
		for (my $j = 0; $j < $nameslen; ++$j, ++$p) {
			$h{$names->[$j]} = $hsres->[$p];
		}
		push(@$ret, \%h);
	}
	return $ret; # arrayref of hashrefs
}

sub result_multi_to_hasharr {
	my ($names, $mhsres, $ret) = @_;
	my $nameslen = scalar(@$names);
	$ret = [ ] if !defined($ret);
	for my $hsres (@$mhsres) {
		my $hsreslen = scalar(@$hsres);
		my $rlen = int($hsreslen / $nameslen);
		my $p = 0;
		for (my $i = 0; $i < $rlen; ++$i) {
			my %h = ();
			for (my $j = 0; $j < $nameslen; ++$j, ++$p) {
				$h{$names->[$j]} = $hsres->[$p];
			}
			push(@$ret, \%h);
		}
	}
	return $ret; # arrayref of hashrefs
}

sub result_single_to_hashhash {
	my ($names, $key, $hsres, $ret) = @_;
	my $nameslen = scalar(@$names);
	my $hsreslen = scalar(@$hsres);
	my $rlen = int($hsreslen / $nameslen);
	$ret = { } if !defined($ret);
	my $p = 0;
	for (my $i = 0; $i < $rlen; ++$i) {
		my %h = ();
		for (my $j = 0; $j < $nameslen; ++$j, ++$p) {
			$h{$names->[$j]} = $hsres->[$p];
		}
		my $k = $h{$key};
		$ret->{$k} = \%h if defined($k);
	}
	return $ret; # hashref of hashrefs
}

sub result_multi_to_hashhash {
	my ($names, $key, $mhsres, $ret) = @_;
	my $nameslen = scalar(@$names);
	$ret = { } if !defined($ret);
	for my $hsres (@$mhsres) {
		my $hsreslen = scalar(@$hsres);
		my $rlen = int($hsreslen / $nameslen);
		my $p = 0;
		for (my $i = 0; $i < $rlen; ++$i) {
			my %h = ();
			for (my $j = 0; $j < $nameslen; ++$j, ++$p) {
				$h{$names->[$j]} = $hsres->[$p];
			}
			my $k = $h{$key};
			$ret->{$k} = \%h if defined($k);
		}
	}
	return $ret; # hashref of hashrefs
}

sub select_cols_where_eq_aa {
	# SELECT $cols FROM $db.$tbl WHERE $idx_key = $kv LIMIT 1
	my ($self, $db, $tbl, $idx, $cols_aref, $kv_aref) = @_;
	my $cols_str = join(',', @$cols_aref);
	my $res = $self->index_find($db, $tbl, $idx, $cols_str, '=', $kv_aref);
	return result_single_to_arrarr(scalar(@$cols_aref), $res);
}

sub select_cols_where_eq_hh {
	# SELECT $cols FROM $db.$tbl WHERE $idx_key = $kv LIMIT 1
	my ($self, $db, $tbl, $idx, $cols_aref, $kv_aref, $retkey) = @_;
	my $cols_str = join(',', @$cols_aref);
	my $res = $self->index_find($db, $tbl, $idx, $cols_str, '=', $kv_aref);
	my $r = result_single_to_hashhash($cols_aref, $retkey, $res);
	return $r;
}

sub select_cols_where_in_hh {
	# SELECT $cols FROM $db.$tbl WHERE $idx_key in ($vals)
	my ($self, $db, $tbl, $idx, $cols_aref, $vals_aref, $retkey) = @_;
	my $cols_str = join(',', @$cols_aref);
	my @cmdlist = ();
	for my $v (@$vals_aref) {
		push(@cmdlist, [ -1, '=', [ $v ] ]);
	}
	my $res = $self->index_find_multi($db, $tbl, $idx, $cols_str,
		\@cmdlist);
	return result_multi_to_hashhash($cols_aref, $retkey, $res);
}

1;

