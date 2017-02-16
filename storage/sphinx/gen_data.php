<?php

$file_name= $argv[1];

//echo $file_name;

$cont= file_get_contents($file_name);

$words= explode(" ", $cont);

//echo "words: ".(count($words))."\n";

$cw = count($words);

echo "REPLACE INTO test.documents ( id, group_id, date_added, title, content ) VALUES\n";


for ($i=1; $i<=100000; $i++)
{
  $count_words= mt_rand(10,30);
  $pred = "";
  for ($j=0; $j<$count_words; $j++)
  {
    $pred .= chop($words[mt_rand(1, $cw-1)])." ";
  }
  $count_words= mt_rand(3,5);
  $tit = "";
  for ($j=0; $j<$count_words; $j++)
  {
    $tit .= chop($words[mt_rand(1, $cw-1)])." ";
  }
  echo "($i,".mt_rand(1,20).",NOW(),'".addslashes($tit)."','".addslashes($pred)."'),\n";
}       
  echo "(0,1,now(),'end','eND');\n";
  

?>
