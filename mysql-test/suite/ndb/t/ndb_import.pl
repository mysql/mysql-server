#!/usr/bin/perl

use strict;
use Symbol;

my $vardir = $ENV{MYSQLTEST_VARDIR}
  or die "need MYSQLTEST_VARDIR";

# fixed parameters

my $par_nullpct = 10;   # pct nulls if nullable
my $par_sppct = 5;      # a few pct special values like min,max
my $par_quotepct  = 20; # pct quoted if fields_enclosed_by
my $par_randbias = 3;   # prefer smaller random values

# type info

sub gen_int;
sub gen_bigint;
sub gen_float;
sub gen_char;
sub gen_decimal;
sub gen_bit;
sub gen_year;
sub gen_date;
sub gen_time;
sub gen_datetime;
sub gen_timestamp;
sub gen_blob;

my %typeinfo = (
  tinyint => {
    gen => \&gen_int,
    min_signed => -2**7,
    max_signed => 2**7-1,
    max_unsigned => 2**8-1,
  },
  smallint => {
    gen => \&gen_int,
    min_signed => -2**15,
    max_signed => 2**15-1,
    max_unsigned => 2**16-1,
  },
  mediumint => {
    gen => \&gen_int,
    min_signed => -2**23,
    max_signed => 2**23-1,
    max_unsigned => 2**24-1,
  },
  int => {
    gen => \&gen_int,
    min_signed => -2**31,
    max_signed => 2**31-1,
    max_unsigned => 2**32-1,
  },
  bigint => {
    gen => \&gen_bigint,
    min_signed => -2**31,
    max_signed => 2**31-1,
    max_unsigned => 2**32-1,
  },
  double => {
    gen => \&gen_float,
    max_exp => 308,
  },
  float => {
    gen => \&gen_float,
    max_exp => 38
  },
  char => {
    gen => \&gen_char,
  },
  varchar => {
    gen => \&gen_char,
  },
  binary => {
    binary => 1,
    gen => \&gen_char,
  },
  varbinary => {
    binary => 1,
    gen => \&gen_char,
  },
  decimal => {
    gen => \&gen_decimal,
  },
  bit => {
    gen => \&gen_bit,
  },
  year => {
    gen => \&gen_year,
  },
  date => {
    gen => \&gen_date,
  },
  time => {
    gen => \&gen_time,
  },
  datetime => {
    gen => \&gen_datetime,
  },
  timestamp => {
    gen => \&gen_timestamp,
  },
  text => {
    gen => \&gen_blob,
  },
  blob => {
    gen => \&gen_blob,
  },
);

# init test

# CSV control strings
# note --exec uses sh so these are passed in single quotes

sub get_csvfmt1 {
  my $csvfmt = {
    fields_terminated_by => '\t',
    fields_enclosed_by => undef,
    fields_escaped_by => '\\',
    lines_terminated_by => '\n',
  };
  return $csvfmt;
}

sub get_csvfmt2 {
  my $csvfmt = {
    fields_terminated_by => ',',
    fields_enclosed_by => '"',
    fields_escaped_by => '\\',
    lines_terminated_by => '\n',
  };
  return $csvfmt;
}

# translate used CSV control string to binary
# in perl '\\' and '\'' are 1 byte and other '\x' are 2 bytes
# basic double quoted chars work as expected

sub translate_csv_control {
  my ($s) = @_;
  return "\t" if $s eq '\t';
  return "\n" if $s eq '\n';
  return $s if $s eq ',';
  return $s if $s eq '"';
  return "\\" if $s eq '\\';
  return "\\" if $s eq '\\\\';
  die "translate_csv_control: unknown $s";
}

sub init_test {
  my ($test) = @_;
  my $f = $test->{csvfmt};
  my $t = $test->{csvesc} = {};
  my @name = qw(
    fields_terminated_by
    fields_enclosed_by
    fields_escaped_by
    lines_terminated_by
  );
  for my $name (@name) {
    if (defined($f->{$name})) {
      $t->{$name} = translate_csv_control($f->{$name});
    } else {
      $t->{$name} = undef;
    }
  }
  # defaults
  $test->{statedir} = "$vardir/tmp";
  $test->{csvdir} = "$vardir/tmp";
  $test->{database} = "test";
  # does load data for verification need own csv file
  $test->{csvver} = $test->{verify} && $test->{rejects};
}

sub get_tablename {
  my ($test, $table, $opts) = @_;
  my $tag = $test->{tag};
  my $ver = $opts->{ver} ? "ver" : "";
  return "$table->{name}$tag$ver";
}

sub get_csvfile {
  my ($test, $table, $opts) = @_;
  my $tag = $test->{tag};
  my $ver = $test->{csvver} && $opts->{ver} ? "ver" : "";
  return "$test->{csvdir}/$table->{name}$tag$ver.csv";
}

# create and drop

sub create_attr {
  my ($attr) = @_;
  $typeinfo{$attr->{type}}
    or die "$attr->{name}: bad type: $attr->{type}";
  my @txt = ();
  push(@txt, $attr->{name});
  push(@txt, $attr->{type});
  push(@txt, "($attr->{len})")
    if defined($attr->{len});
  push(@txt, "($attr->{prec}, $attr->{scale})")
    if defined($attr->{prec}) && defined($attr->{scale});
  push(@txt, "($attr->{prec})")
    if defined($attr->{prec}) && !defined($attr->{scale});
  push(@txt, "unsigned")
    if $attr->{unsigned};
  push(@txt, "not null")
    if $attr->{notnull};
  return "@txt";
}

sub create_attrs {
  my ($attrs) = @_;
  my @txt = ();
  my $attrno = 0;
  for my $attr (@$attrs) {
    $attr->{attrno} = $attrno++;
    $attr->{notnull} = 1 if $attr->{pk};
  }
  for my $attr (@$attrs) {
    my $s = $attr->{attrno} == 0 ? "\n" : ",\n";
    push(@txt, $s . create_attr($attr));
    $attrno++;
  }
  return "@txt";
}

sub create_table {
  my ($test, $table, $opts) = @_;
  my $tname = get_tablename($test, $table, $opts);
  my @txt = ();
  push(@txt, "create table $tname (");
  my $attrs = $table->{attrs};
  push(@txt, create_attrs($attrs));
  my @pklist = ();
  for my $attr (@$attrs) {
    push(@pklist, $attr->{name})
      if $attr->{pk};
  }
  if (@pklist) {
    my $pklist = join(", ", @pklist);
    push(@txt, ",\nprimary key ($pklist)");
  }
  push(@txt, "\n) engine $opts->{engine};\n");
  return "@txt";
}

sub create_tables {
  my ($test, $opts) = @_;
  my @txt = ();
  my $tables = $test->{tables};
  for my $table (@$tables) {
    push(@txt, create_table($test, $table, $opts));
  }
  return "@txt";
};

sub drop_table {
  my ($test, $table, $opts) = @_;
  my $tname = get_tablename($test, $table, $opts);
  my @txt = ();
  push(@txt, "drop table");
  push(@txt, "if exists")
    if $opts->{if_exists};
  push(@txt, "$tname;\n");
  return "@txt";
}

sub drop_tables {
  my ($test, $opts) = @_;
  my @txt = ();
  my $tables = $test->{tables};
  for my $table (@$tables) {
    push(@txt, drop_table($test, $table, $opts));
  }
  return "@txt";
};

# run test

sub run_import {
  my ($test, $opts) = @_;
  my $fter = $test->{csvfmt}{fields_terminated_by};
  my $fenc = $test->{csvfmt}{fields_enclosed_by};
  my @cmd = ();
  push(@cmd, "\$NDB_IMPORT");
  push(@cmd, "--state-dir='$test->{statedir}'");
  push(@cmd, "--input-type=csv");
  push(@cmd, "--input-workers=2");
  push(@cmd, "--output-type=ndb");
  push(@cmd, "--output-workers=2");
  push(@cmd, "--db-workers=2");
  push(@cmd, "--temperrors=100");
  push(@cmd, "--fields-terminated-by='$fter'");
  if (defined($fenc)) {
    push(@cmd, "--fields-optionally-enclosed-by='$fenc'");
  }
  # fail on first rejected row if resume flag is set
  if ($test->{rejects} && !$test->{resume}) {
    push(@cmd, "--rejects=$test->{rejects}");
  }
  # $opts tells is this is a resume
  if ($opts->{resume}) {
    push(@cmd, "--resume");
  }
  push(@cmd, "--verbose=1");
  push(@cmd, $test->{database});
  my $tables = $test->{tables};
  for my $table (@$tables) {
    my $csvfile = get_csvfile($test, $table, $opts);
    push(@cmd, $csvfile);
  }
  # runs mainly on unix/linux so stderr goes to stdout
  my $log = '>>$NDB_TOOLS_OUTPUT 2>&1';
  my @cmd1 = ("--exec", "echo", @cmd, $log);
  my @cmd2 = ("--exec", @cmd, $log);
  if (defined($opts->{error})) {
    unshift(@cmd2, "--error $opts->{error}\n");
  }
  return "@cmd1\n@cmd2\n";
}

sub select_count {
  my ($test, $table, $opts) = @_;
  my $tname = get_tablename($test, $table, $opts);
  my @txt = ();
  push(@txt, "select count(*) from $tname;\n");
  return "@txt";
}

sub select_counts {
  my ($test, $opts) = @_;
  my @txt = ();
  my $tables = $test->{tables};
  for my $table (@$tables) {
    push(@txt, select_count($test, $table, $opts));
  }
  return "@txt";
}

sub load_table {
  my ($test, $table, $opts) = @_;
  my $csvfile = get_csvfile($test, $table, $opts);
  my $tname = get_tablename($test, $table, $opts);
  my $fter = $test->{csvfmt}{fields_terminated_by};
  my $fenc = $test->{csvfmt}{fields_enclosed_by};
  my @txt = ();
  push(@txt, "load data infile '$csvfile'\n");
  push(@txt, "into table $tname\n");
  push(@txt, "fields");
  push(@txt, "terminated by '$fter'");
  if (defined($fenc)) {
    push(@txt, "optionally enclosed by '$fenc'");
  }
  push(@txt, ";\n");
  return "@txt";
}

sub load_tables {
  my ($test, $opts) = @_;
  my @txt = ();
  push @txt, "--disable_query_log\n";
  my $tables = $test->{tables};
  for my $table (@$tables) {
    push(@txt, load_table($test, $table, $opts));
  }
  push @txt, "--enable_query_log\n";
  return "@txt";
}

sub verify_table {
  my ($test, $table, $opts) = @_;
  my $tname1 = get_tablename($test, $table, { ver => 0 });
  my $tname2 = get_tablename($test, $table, { ver => 1 });
  my @txt = ();
  push(@txt, "select count(*) from $tname1 x, $tname2 y\n");
  push(@txt, "where");
  my @cls = ();
  my $attrs = $table->{attrs};
  for my $attr (@$attrs) {
    my $a1 = "x." . $attr->{name};
    my $a2 = "y." . $attr->{name};
    my $c;
    if ($attr->{notnull}) {
      $c = "$a1 = $a2";
    } else {
      $c = "($a1 = $a2 or ($a1 is null and $a2 is null))";
    }
    push(@cls, $c);
  }
  push(@txt, join(" and\n", @cls));
  push(@txt, ";\n");
  return "@txt";
}

sub verify_tables {
  my ($test, $opts) = @_;
  my @txt = ();
  my $tables = $test->{tables};
  for my $table (@$tables) {
    push(@txt, verify_table($test, $table, $opts));
  }
  return "@txt";
}

# write test

sub make_test {
  my ($test) = @_;
  init_test($test);
  my @txt = ();
  push @txt, "--echo # test $test->{tag} - $test->{desc}\n";
  push @txt, create_tables($test, { engine => "ndb" });
  if (!$test->{resume}) {
    push @txt, run_import($test, {});
  } else {
    push @txt, run_import($test, { error => "1" });
    for (my $i = 1; $i <= $test->{rejects}; $i++) {
      push @txt, run_import($test, { resume => $i, error => "0,1" });
    }
  }
  push @txt, select_counts($test, {});
  if ($test->{verify}) {
    push @txt, create_tables($test, { engine => "ndb", ver => 1 });
    push @txt, load_tables($test, { ver => 1 });
    push @txt, select_counts($test, { ver => 1 });
    push @txt, verify_tables($test, {});
    push @txt, drop_tables($test, { ver => 1 });
  }
  push @txt, drop_tables($test, {});
  return "@txt";
}

sub write_tests {
  my ($tests) = @_;
  my $tag = $tests->{tag};
  my $file = "$vardir/tmp/ndb_import$tag.inc";
  my $fh = gensym();
  open($fh, ">$file")
    or die "$file: open for write failed: $!";
  my $testlist = $tests->{testlist};
  for my $test (@$testlist) {
    print $fh make_test($test);
  }
  close($fh)
    or die "$file: close after write failed: $!";
  for my $test (@$testlist) {
    # values in $opts propagate down *and* up
    my $opts = {};
    write_csvfiles($test, $opts);
  }
}

# generate values

sub myrand {
  my ($m) = @_;
  return int(rand($m));
}

sub myrand2 {
  my ($m, $k) = @_;
  my $n = myrand($m);
  while ($k > 0) {
    $n = myrand($n + 1);
    $k--;
  }
  return $n;
}

sub gen_int {
  my ($test, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  my $lo;
  my $hi;
  if ($attr->{unsigned}) {
    $lo = 0;
    $hi = $typeinfo->{max_unsigned};
  } else {
    $lo = $typeinfo->{min_signed};
    $hi = $typeinfo->{max_signed};
  }
  my $val;
  if ($attr->{pk}) {
    $val = $opts->{rowid} % ($hi + 1);
  } else {
    my $r = int(rand(100));
    if ($r < $par_sppct) {
      $val = 0 if $r % 3 == 0;
      $val = $lo if $r % 3 == 1;
      $val = $hi if $r % 3 == 2;
    } else {
      $val = $lo + int(rand($hi - $lo + 1));
    }
  }
  return "$val";
}

sub gen_bigint {
  my ($test, $attr, $opts) = @_;
  my $val = gen_int($test, $attr, $opts);
  return $val;
};

sub gen_float {
  my ($test, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  my $hiexp = $typeinfo->{max_exp};
  my $p = myrand2($hiexp + 1, $par_randbias);
  if (myrand(2) == 0) {
    $p = (-1) * $p;
  }
  my $v = rand() * (10 ** $p);
  if (myrand(2) == 0) {
    $v = (-1) * $v;
  }
  my $val;
  if (myrand(2) == 0) {
    $val = sprintf("%f", $v);
  } else {
    $val = sprintf("%g", $v);
  }
  return $val;
}

my $g_escape = {
  0 => '0',
  0x08 => 'b',
  0x0a => 'n',
  0x0d => 'r',
  0x09 => 't',
  0x1a => 'Z',
};

sub make_byte {
  my ($test, $opts) = @_;
  my $fter = $test->{csvesc}{fields_terminated_by};
  my $fenc = $test->{csvesc}{fields_enclosed_by};
  my $fesc = $test->{csvesc}{fields_escaped_by};
  my $lter = $test->{csvesc}{lines_terminated_by};
  my $binary = $opts->{binary};
  my $mask = $opts->{mask};
  my $val;
  while (1) {
    my $x;
    # non-binary used only to make readable files, no charsets yet
    if (!$binary) {
      $x = 32 + myrand(127 - 32);
    } else {
      $x = myrand(256);
    }
    if (defined($mask)) {
      $x &= $mask;
    }
    if ($x == 0) {
      $val = $fesc.'0';
      last;
    }
    if ($x == ord($fter)) {
      if ($opts->{quote}) {
        $val = $fter;
      } else {
        $val = $fesc.$fter;
      }
      last;
    }
    if (defined($fenc) && $x == ord($fenc)) {
      if ($opts->{quote} && myrand(2) == 0) {
        $val = $fenc.$fenc;
      } else {
        $val = $fesc.$fenc;
      }
      last;
    }
    if ($x == ord($fesc)) {
      $val = $fesc.$fesc;
      last;
    }
    if ($x == ord($lter)) {
      if ($opts->{quote}) {
        if (myrand(2) == 0) {
          $val = $lter;
          last;
        }
      }
      if ($lter eq "\n") {
        $val = "\\n";
        last;
      }
      die "make_byte: cannot handle lter='$lter'";
    }
    if (myrand(5) == 0) {
      my $v = $g_escape->{$x};
      if (defined($v)) {
        $val = $fesc.$v;
        last;
      }
    }
    $val = pack('C', $x);
    last;
  }
  return $val;
}

sub make_string {
  my ($test, $opts) = @_;
  my $hilen = $opts->{hilen};
  my $len = myrand(10) != 0 ? myrand2($hilen, $par_randbias) : $hilen;
  my $val = "";
  for (my $i = 0; $i < $len; $i++) {
    $val .= make_byte($test, $opts);
  }
  return $val;
}

sub gen_char {
  my ($test, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  my $val;
  if ($attr->{pk}) {
    $val = sprintf("%d", $opts->{rowid});
  } else {
    $opts->{binary} = $typeinfo->{binary};
    $opts->{hilen} = $attr->{len};
    $val = make_string($test, $opts);
  }
  return $val;
}

sub gen_bit {
  my ($test, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  my $fter = $test->{csvfmt}{fields_terminated_by};
  my $fesc = $test->{csvfmt}{fields_escaped_by};
  my $l = $attr->{len};
  my $n = int(($l + 7) / 8);    # bytes rounded up
  my $m = 8 - (8 * $n - $l);    # bits in last byte
  my $val = "";
  for (my $i = 0; $i < $n; $i++) {
    my $mask = 255;
    if ($i + 1 == $n && $m != 0) {
      $mask = (1 << $m) - 1;
    }
    $opts->{mask} = $mask;
    my $v = make_byte($test, $opts);
    $val .= $v;
  }
  return $val;
}

sub make_decimalpart {
  my ($test, $opts) = @_;
  my $n = $opts->{partlen};
  my $m = myrand($n + 1);
  my $val = "";
  for (my $k = 0; $k < $m; $k++) {
    my $d = myrand(10);
    $val .= sprintf("%d", $d);
  }
  return $val;
}

sub gen_decimal {
  my ($test, $attr, $opts) = @_;
  my $prec = $attr->{prec};
  my $scale = $attr->{scale};
  $prec >= $scale or die "invalid prec=$prec scale=$scale";
  my $val;
  while (1) {
    $opts->{partlen} = $prec - $scale;
    $val = make_decimalpart($test, $opts);
    if (myrand(2) == 0) {
      $val .= ".";
      if (myrand(2) == 0) {
        $opts->{partlen} = $scale;
        $val .= make_decimalpart($test, $opts);
      }
    }
    last if $val =~ /\d/;
  }
  if ($attr->{unsigned}) {
    if (myrand(3) == 0) {
      $val = "+$val";
    }
  } else {
    if (myrand(3) == 0) {
      $val = "-$val";
    }
  }
  return $val;
}

sub gen_year {
  my ($test, $attr, $opts) = @_;
  my $yy = 1901 + myrand(255);
  my $val = sprintf("%04d", $yy);
  return $val;
}

# Date::Manip could be used to generate better valid dates
sub make_date {
  my ($test, $opts) = @_;
  my $yystart = $opts->{yystart};
  my $yyrange = $opts->{yyrange};
  my $yy = $yystart + myrand($yyrange);
  my $mm = 1 + myrand(12);
  my $dd = 1 + myrand(28);
  my $val = sprintf("%04d-%02d-%02d", $yy, $mm, $dd);
  return $val;
}

sub gen_date {
  my ($test, $attr, $opts) = @_;
  $opts->{yystart} = 1950;
  $opts->{yyrange} = 100;
  my $val = make_date($test, $opts);
  return $val;
}

sub make_frac {
  my ($test, $opts) = @_;
  my $prec = $opts->{prec};
  my $n = myrand($prec + 1);
  my $val = "";
  for (my $i = 0; $i < $n; $i++) {
    $val .= myrand(10);
  }
  $val = ".$val" if $n != 0 || myrand(5) == 0;
  return $val;
}

sub make_time {
  my ($test, $opts) = @_;
  my $sep = $opts->{sep};
  my $ts = $opts->{ts};
  my $val;
  my $hh = myrand(24);
  if ($ts && ($hh == 2 || $hh == 3 || $hh == 4)) {
    $hh = 1;
  }
  my $mm = myrand(60);
  my $ss = myrand(60);
  if ($sep) {
    $hh = "0$hh" if $hh < 10 && myrand(2) == 0;
    $mm = "0$mm" if $mm < 10 && myrand(2) == 0;
    $ss = "0$ss" if $ss < 10 && myrand(2) == 0;
    $val = "$hh:$mm:$ss";
  } else {
    $hh = "0$hh" if $hh < 10;
    $mm = "0$mm" if $mm < 10;
    $ss = "0$ss" if $ss < 10;
    $val = "$hh$mm$ss";
  }
  $val .= make_frac($test, $opts);
  return $val;
}

sub gen_time {
  my ($test, $attr, $opts) = @_;
  $opts->{sep} = myrand(2);
  $opts->{prec} = $attr->{prec};
  $opts->{ts} = 0;
  my $val = make_time($test, $opts);
  return $val;
}

sub gen_datetime {
  my ($test, $attr, $opts) = @_;
  $opts->{yystart} = 1950;
  $opts->{yyrange} = 100;
  my $d = make_date($test, $opts);
  $opts->{sep} = 1;
  $opts->{prec} = $attr->{prec};
  $opts->{ts} = 0;
  my $t = make_time($test, $opts);
  my $p = myrand(3) ? "/" : " ";
  return "$d$p$t"
}

sub gen_timestamp {
  my ($test, $attr, $opts) = @_;
  my $prec = $attr->{prec};
  $opts->{yystart} = 1971;
  $opts->{yyrange} = 64;
  my $d = make_date($test, $opts);
  $opts->{sep} = 1;
  $opts->{prec} = $attr->{prec};
  $opts->{ts} = 1;
  my $t = make_time($test, $opts);
  my $p = myrand(3) ? "/" : " ";
  return "$d$p$t"
}

sub gen_blob {
  my ($test, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  $opts->{binary} = $typeinfo->{binary};
  $opts->{hilen} = $attr->{len};
  my $val = make_string($test, $opts);
  return $val;
}

sub gen_csvfield {
  my ($test, $table, $attr, $opts) = @_;
  my $typeinfo = $typeinfo{$attr->{type}};
  my $fenc = $test->{csvesc}{fields_enclosed_by};
  my $fesc = $test->{csvesc}{fields_escaped_by};
  my $val;
  # if value will be enclosed by
  $opts->{quote} = defined($fenc) &&
                   rand(100) < $par_quotepct;
  if (!$attr->{notnull} &&
      int(rand(100) < $par_nullpct)) {
    $val = $fesc.'N';
  } else {
    $val = $typeinfo->{gen}($test, $attr, $opts);
    if ($opts->{rejectsflag} &&
        myrand(100) < $par_nullpct) {
      $val = $fesc.'N';
      $opts->{rejectserrs}++;
    }
  }
  if ($opts->{quote}) {
    $val = "$fenc$val$fenc";
  }
  return $val;
}

sub gen_csvline {
  my ($test, $table, $opts) = @_;
  my $attrs = $table->{attrs};
  my $fter = $test->{csvesc}{fields_terminated_by};
  my $fesc = $test->{csvesc}{fields_escaped_by};
  my $lter = $test->{csvesc}{lines_terminated_by};
  my @line = ();
  if ($opts->{rejectsflag}) {
    $opts->{rejectserrs} = 0;
  }
  for my $attr (@$attrs) {
    my $val = gen_csvfield($test, $table, $attr, $opts);
    push(@line, $val);
  }
  if ($opts->{rejectsflag}) {
    if ($opts->{rejectserrs} == 0) {
      if (myrand(2) == 0) {
        my $val = $fesc.'N';
        push(@line, $val);
      } else {
        pop(@line);
      }
      $opts->{rejectserrs}++;
    }
  }
  my $line = join($fter, @line).$lter;
  return $line;
}

# write csv files

sub write_csvfile {
  my ($test, $table, $opts) = @_;
  my $file = get_csvfile($test, $table, { ver => 0 });
  my $filever = get_csvfile($test, $table, { ver => 1 });
  my $fh = gensym();
  my $fhver = gensym();
  open($fh, ">$file")
    or die "$file: open for write failed: $!";
  if ($test->{csvver}) {
    open($fhver, ">$filever")
      or die "$filever: open for write failed: $!";
  }
  my $tablename = get_tablename($test, $table, { ver => 0 });
  my $rows = $table->{rows};
  $table->{rejects} = 0;
  for (my $n = 0; $n < $rows; $n++) {
    $opts->{rowid} = $n;
    $opts->{rejectsflag} = 0;
    if ($test->{rejects}) {
      my $rowsleft = $rows - $n;
      my $rejectsleft = $test->{rejects} - $table->{rejects};
      if ($rejectsleft != 0) {
        if ($rejectsleft == $rowsleft ||
            myrand(1 + $rowsleft/$rejectsleft) == 0) {
          $opts->{rejectsflag} = 1;
          $opts->{rejectserrs} = 0;
        }
      }
    }
    my $line = gen_csvline($test, $table, $opts);
    if ($opts->{rejectsflag}) {
      $opts->{rejectserrs} or die "no rejectserrs";
      $table->{rejects}++;
    }
    print $fh "$line"
      or die "$file: rowid $n: write failed: $!";
    if ($test->{csvver}) {
      if (!$opts->{rejectsflag}) {
        print $fhver "$line"
          or die "$filever: rowid $n: write failed: $!";
      }
    }
  }
  $test->{rejects} == $table->{rejects}
    or die "$tablename: $test->{rejects} != $table->{rejects}";
  close($fh)
    or die "$file: close after write failed: $!";
  if ($test->{csvver}) {
    close($fhver)
      or die "$filever: close after write failed: $!";
  }
}

sub write_csvfiles {
  my ($test, $opts) = @_;
  my $tables = $test->{tables};
  for my $table (@$tables) {
    write_csvfile($test, $table, $opts);
  }
}

1;
