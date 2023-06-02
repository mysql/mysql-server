<?php
$hostname     = $argv[1];
$port         = $argv[2];
$username     = $argv[3];
$password     = $argv[4];
$with_ssl     = $argv[5] == "0" ? false : true;
$with_sharing = $argv[6] == "0" ? false : true;

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
$mysqli = mysqli_init();
mysqli_real_connect($mysqli,
  $hostname, $username, $password, "", $port, NULL,
  $with_ssl ? MYSQLI_CLIENT_SSL : 0);

function result_as_array($res) {
  if (is_bool($res)) return $res;

  $rows = [];
  while($row = $res->fetch_array(MYSQLI_NUM)) {
    $rows[] = $row;
  }
  return $rows;
}

function query($m, $qry) {
  return result_as_array($m->query($qry));
}

if ($with_sharing) {
  foreach ([
    "ROUTER SET trace = 1",
  ] as $qry) {
    print("# " . $qry . "\n\n");
    print(json_encode(query($mysqli, $qry), JSON_PRETTY_PRINT) . "\n\n");
  }
}

foreach ([
  "DO 1",
  "SELECT 1",
  "SELECT 1, 1",
  "SELECT 1, ?",
] as $qry) {
  print("# prepare: " . $qry . "\n\n");

  $stmt = $mysqli->prepare($qry);

   {
    print("## SHOW WARNINGS\n\n");
    $trace_res = query($mysqli, "SHOW WARNINGS");
    if ($with_sharing) {
      count($trace_res) > 0 or throw new Exception("expected row-count > 0, got " . count($trace_res));
      print($trace_res[0][2] . "\n\n");
      $trace = json_decode($trace_res[0][2]);
      ($trace->{"attributes"}->{"mysql.sharing_blocked"} == true) or throw new Exception("expected sharing to be blocked.");
    }
  }

  print("## -> execute (1)\n\n");
  $foo = "abc";
  if ($stmt->param_count > 0) {
    $stmt->bind_param("s", $foo);
  }
  $stmt->execute();
  $res = result_as_array($stmt->result_metadata() ? $stmt->get_result() : true);
  print(json_encode($res, JSON_PRETTY_PRINT) . "\n\n");

  {
    print("## SHOW WARNINGS\n\n");
    $trace_res = query($mysqli, "SHOW WARNINGS");
    if ($with_sharing) {
      count($trace_res) > 0 or throw new Exception("expected row-count > 0, got " . count($trace_res));
      print($trace_res[0][2] . "\n\n");
      $trace = json_decode($trace_res[0][2]);
      ($trace->{"attributes"}->{"mysql.sharing_blocked"} == true) or throw new Exception("expected sharing to be blocked.");
    }
  }

  print("## -> execute (2)\n\n");
  $stmt->execute();
  $res = result_as_array($stmt->result_metadata() ? $stmt->get_result() : true);
  print(json_encode($res, JSON_PRETTY_PRINT) . "\n\n");

  {
    print("## SHOW WARNINGS\n\n");
    $trace_res = query($mysqli, "SHOW WARNINGS");
    if ($with_sharing) {
      count($trace_res) > 0 or throw new Exception("expected row-count > 0, got " . count($trace_res));
      print($trace_res[0][2] . "\n\n");
      $trace = json_decode($trace_res[0][2]);
      ($trace->{"attributes"}->{"mysql.sharing_blocked"} == true) or throw new Exception("expected sharing to be blocked.");
    }
  }

  $stmt->close();
}
