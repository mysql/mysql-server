<?php
$hostname     = $argv[1];
$port         = $argv[2];
$username     = $argv[3];
$password     = $argv[4];
$with_ssl     = $argv[5] == "0" ? false : true;
$with_sharing = $argv[6] == "0" ? false : true;

// throw exceptions if a mysql function fails.
mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

function result_as_array($res) {
  if (!is_object($res)) return $res;

  $rows = [];
  while($row = $res->fetch_array(MYSQLI_NUM)) {
    $rows[] = $row;
  }
  return $rows;
}

function markdown_code($block, $highlight = "") {
return <<<EOF
```$highlight
$block
```

EOF;
}

function md($res) {
  return "* " . markdown_code(json_encode(result_as_array($res)), "javascript");
}

function multi_md($m, $res) {
  $out = "";

  do {
    $out .= md($res);
    if (!$m->wrapped_->next_result()) break;
    $res->free_result();
    $res = $m->wrapped_->store_result();
  } while (true);

  return $out;
}

function dump($v) {
  return var_export($v, true);
}

/**
 * print the called function with its parameters.
 */
class CallTracer {
  public function __construct($wrapped) {
    $this->wrapped_ = $wrapped;
  }

  public function __call($name, $args) {
    print(markdown_code("$name(" . implode(", ", array_map("dump", $args)) . ")\n", "javascript"));
    return $this->wrapped_->$name(...$args);
  }

  public $wrapped_;
};

$mysqli = new CallTracer(mysqli_init());

# mysqli_real_connect() checks the type of the mysqli. Pass it the wrapped class.
mysqli_real_connect($mysqli->wrapped_,
  $hostname, $username, $password, "", $port, NULL,
  $with_ssl ? MYSQLI_CLIENT_SSL : 0);

?>

# ROUTER SET trace = 1

<?php if ($with_sharing) print(md($mysqli->query("ROUTER SET trace = 1"))); ?>

# autocommit

<?= md($mysqli->autocommit(1)) ?>
<?= md($mysqli->autocommit(0)) ?>

# transactions

<?= md($mysqli->begin_transaction(0, "savepointname")) ?>
<?= md($mysqli->savepoint("abc")) ?>
<?= md($mysqli->release_savepoint("savepointname")) ?>
<?= md($mysqli->release_savepoint("abc")) ?>
<?= md($mysqli->commit()) ?>
<?= md($mysqli->rollback()) ?>

# debug

<?= md($mysqli->dump_debug_info()) ?>

# ping
<?= md($mysqli->ping()) ?>

# stat
<?= md($mysqli->stat()) ?>

# refresh
<?= md($mysqli->refresh(0)) ?>


# use schema
<?= md($mysqli->query("DROP SCHEMA IF EXISTS phpt")) ?>
<?= md($mysqli->query("CREATE SCHEMA phpt")) ?>
<?= md($mysqli->select_db("phpt")) ?>

# multi-resultsets

<?= md($mysqli->query("CREATE PROCEDURE p1 () BEGIN SELECT 1; SELECT 2; END")) ?>
<?= multi_md($mysqli, $mysqli->query("CALL p1()")) ?>

<?php
$res = result_as_array($mysqli->query("SELECT SCHEMA()"));
print("* " . markdown_code(json_encode($res), "javascript"));

$res[0][0] == "phpt" or throw new Exception("expected SCHEMA to be set");
?>

# change_user

<?= md($mysqli->change_user($username, $password, "")) ?>
<?php

$res = result_as_array($mysqli->query("SELECT SCHEMA()"));
print("* " . markdown_code(json_encode($res), "javascript"));

$res[0][0] == null or throw new Exception("expected SCHEMA to be null");
?>

# change_user with schema

<?= md($mysqli->change_user($username, $password, "phpt")) ?>

<?php
$res = result_as_array($mysqli->query("SELECT SCHEMA()"));
print("* " . markdown_code(json_encode($res), "javascript"));

$res[0][0] == "phpt" or throw new Exception("expected SCHEMA to be set");
?>

# query

<?php if ($with_sharing) print(md($mysqli->query("ROUTER SET trace = 1"))); ?>
<?= md($mysqli->query("DO 1")) ?>

## `SHOW WARNINGS`

<?php
{
  $trace_res = result_as_array($mysqli->query("SHOW WARNINGS"));
  if ($with_sharing) {
    count($trace_res) > 0 or throw new Exception("expected row-count > 0, got " . count($trace_res));
  
    # last element.
    $trace_row = array_slice($trace_res, -1)[0];
    print(markdown_code($trace_row[2], "json"));
  
    $trace = json_decode($trace_row[2]);
    ($trace->{"attributes"}->{"mysql.sharing_blocked"} == false) or 
      throw new Exception("expected sharing to be allowed.");
  }
}
