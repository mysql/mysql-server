#!/usr/bin/php
<?php
#set_include_path(get_include_path() . ':.:../..//include');
#require_once('shard-query.php');
class DRV_MYSQL extends StdClass {
	var $_handle = false;
	var $_opts = "";
	function _connect($DB=false) {
		global $options;
		if(!$this->_handle)	{
			$this->_handle = mysqli_connect($options->DB_HOST,$options->DB_USER,$options->DB_PASS);
		}
		mysqli_select_db($this->_handle,$DB ? $DB : $options->DB_SCHEMA);
	}

	function run_query($sql,$DB=false) {
		$this->_connect($DB);		
		$stmt = mysqli_query($this->_handle,$sql);
		if(!$stmt) {
			echo(mysqli_error($this->_handle));
			echo "\n$sql\n";
			exit;
			return -1;
		}
	}
}

class DRV_SQ extends StdClass {
	var $_handle = false;
	var $_opts = "";
	function _connect($DB=false) {
		global $options;
		if(!$DB)$DB = $options->DB_SCHEMA;
		if(!$this->_handle) {
			$this->_handle = new ShardQuery($DB);
		}
	}
	function run_query($sql,$DB=false) {
		$this->_connect($DB);		
		$stmt = $this->_handle->query($sql);
		if(!$stmt) {
			echo "\n$sql\n";
			print_r($this->_handle->errors);
			exit;
			return -1;
		}
	}
}


function def($const, $key, $default) {
	global $cmdline, $VERBOSE;
	global $options;

	$keys = array_keys($cmdline);
	if(!in_array($key, $keys)) {
		$val = $default;
	} else {
		$val = $cmdline[$key];
	}
	
	$options->$const = $val;
}

function template_replace($sql,$key, $val) {
  return str_replace(':' . $key,$val, $sql);
}

function write_result($qID, $resTime) {
	global $q, $options;
	$fp = fopen("result.log", "a") or die('could not open result.log for writing!');

	if (flock($fp, LOCK_EX)) { // do an exclusive lock
    fwrite($fp, $options->WORKLOAD . "/{$options->DB_SCHEMA}\t" . $options->RUN_INFO . "\t" .$qID . "\t" . "$resTime\n");
    flock($fp, LOCK_UN); 
	} else {
		die('could not lock result file');
	}
	fclose($fp);
}

$cmdline_input ="I:F:h:u:p:P:d:W:L:S:r:";

$cmdline= getopt($cmdline_input);

echo "\n";
$options = new StdClass;

def("RUN_INFO",'I', '"' . trim(`uname -a`) . '"');
def("DB_HOST",'h', '127.0.0.1');
def("DB_USER",'u', 'root');
def("DB_PASS",'p', false);
def("DB_PORT",'P', '3306');
def("DB_DRIVER",'d', 'mysql');
def("WORKLOAD",'W', 'ssb');
def("LOOP_COUNT",'L', 1);
def("SCALE", 'F', 1);
def("DB_SCHEMA",'S','test2');
def("RANDOMIZE", 'r', 'false');

require_once($options->WORKLOAD . '.php');

echo "Configuration after reading command line options:\n";
echo "------------------------------------------------\n";
foreach($options as $key => $val) {
	echo str_pad($key, 18, ' ', STR_PAD_RIGHT) . "|  $val\n";
}
foreach($argv as $v) {
	if($v == "--help") {
		echo "\nCommand line options:\n-d driver -Fscale_factor -Hhost -Pport -ppass -Uuser -Sschema -Ddbname -Lloopcount -Irun_information -Wworkload -rrandomize\n"; 
		exit; 
	}
}
echo "\n";

for($z=0;$z<$options->LOOP_COUNT;++$z) {
		#if($options->RANDOMIZE !== 'false') shuffle($q[$options->WORKLOAD]);
		$driver="DRV_" . $options->DB_DRIVER;	
		$db = new $driver;
		foreach($q as $qry) {
			echo "{$qry['qid']}: ";
			$time=microtime(true);
      echo "EXPLAIN\n" . $qry['template'];
			if($db->run_query($qry['template'],$options->DB_SCHEMA)) {
				$time=-1;
			} else {
				$time = microtime(true) - $time;
			}
			write_result($qry['qid'], $time);
			echo "$time\n";
		}
}

?>

