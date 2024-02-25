<?php
$hostname     = $argv[1];
$port         = $argv[2];
$username     = $argv[3];
$password     = $argv[4];
$with_ssl     = $argv[5] == "0" ? false : true;
$with_sharing = $argv[6] == "0" ? false : true;

if ($password != "" && !$with_ssl) {
  $with_sharing = false;
}


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


function markdown_json_block($json) {
return <<<EOF
```json
$json
```

EOF;
}

foreach ([
  "SELECT 1",
] as $qry) {
  print("# `" . $qry . "`\n\n");

  print(markdown_json_block(json_encode(query($mysqli, $qry), JSON_PRETTY_PRINT)));
}

print("## `SHOW WARNINGS`\n\n");
$warn_res = query($mysqli, "SHOW WARNINGS");
count($warn_res) == 0 or throw new Exception("expected row-count == 0, got " . count($warn_res));
