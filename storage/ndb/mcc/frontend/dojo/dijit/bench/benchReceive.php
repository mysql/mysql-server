<?php
/*

	benchReceive.php - example way to handle incoming benchmark data,
	or how to use JSON php class to mangle data.  No benchmark data
	is stored currently.

--
-- Table structure for table `benchmarks`
--

CREATE TABLE `benchmarks` (
  `id` int(11) NOT NULL auto_increment,
  `useragent` varchar(242) NOT NULL default '',
  `dojover` varchar(96) NOT NULL default '',
  `testNum` int(11) NOT NULL default '0',
  `dijit` varchar(64) NOT NULL default '',
  `testCount` int(11) NOT NULL default '0',
  `testAverage` float NOT NULL default '0',
  `testMethod` varchar(10) NOT NULL default '',
  `testTime` bigint(20) NOT NULL default '0',
  `dataSet` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`id`),
  KEY `dijit` (`dijit`,`testAverage`),
  KEY `dataSet` (`dataSet`)
) TYPE=MyISAM;

--
-- [end table struct] --

*/

if(is_array($_POST)){

	$username = '';
	$password = '';
	$dataBase = '';
	$table    = '';

	mysql_connect("localhost",$username,$password);
	mysql_select_db($dataBase);

	require("../../dojo/tests/resources/JSON.php");
	$json = new Services_JSON();

	// see "escape()" call in benchTest.html
	$string = $json->decode(urldecode($_POST['key']));
	// $string = $json->decode($_POST['key']);

	print "<h1>Thank YOU!</h1>";
	print "
		<p>Your results have been added to our database. No
		personal information outside of what you see here
		has been stored.
		</p>

		<p>You can <a href= \"javascript:history.back()\">go back</a>
		and run more tests, or even better, load up another browser
		and the submit your tests again!
		</p>

		<p>again ... thanks for your time.</p>

		";

	print "<h3>Results Submitted:</h3>";
	print "<pre style=\"font:6pt Terminal,sans-serif; border:1px solid #cecece; background-color:#ededed; padding:20px; \">";

		$ua = $string->clientNavigator;
		$dojov = $string->dojoVersion;

		print "Client: ".$ua."\n";
		print "Dojo v".$dojov."\n";

		if(is_array($string->dataSet)){
			print "\nTest Results:";
			// should client serialize a key, or is this safer?
			$dataSet = md5(serialize($string));
			foreach ($string->dataSet as $test){
				$data = array(
					'dataSet' => $dataSet,
					'useragent' => $ua,
					'dojover' => $dojov,
					'testNum' => $test->testNum,
					'testMethod' => $test->testMethod,
					'testTime' => $test->testTime,
					'testAverage' => $test->testAverage,
					'testCount' => $test->testCount,
					'dijit' => $test->dijit
				);
				print_r($data);
				add_rec($table,$data);
			}
		}

		if(is_array($string->errors)){
			// not saving errors at this point
			print "\nErrors:";
			foreach ($string->errors as $error){
				print_r($error);
			}
		}
	print "</pre>";
}

function add_rec($table, $data){

	if(!is_array($data)){ return FALSE; }

	$keys = array_keys($data);
	$values = array_values($data);
	$field=0;

	for($field;$field<sizeof($data);$field++){
		if(!ereg("^[0-9].*$",$keys[$field])){
			$sqlfields = $sqlfields.$keys[$field]."=\"".$values[$field]."\", ";
       		}
	}
	$sqlfields = (substr($sqlfields,0,(strlen($sqlfields)-2)));

	if($query = mysql_query("insert into $table set $sqlfields")){
		$id = mysql_insert_id();
		return ($id);
	}else{
		return FALSE;
	}
}

?>
