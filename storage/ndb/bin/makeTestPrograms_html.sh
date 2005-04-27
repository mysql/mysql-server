#!/bin/sh
rm $1
touch $1
echo "<table border="1" width=640>" >> $1
echo "<tr>" >> $1
echo "<td><b>Name</b></td><td>&nbsp;</td><td width="70%"><b>Description</b></td>" >> $1
echo "</tr>" >> $1
testBasic --print_html >> $1
testBackup --print_html >> $1
testBasicAsynch --print_html >> $1
testDict --print_html >> $1
testBank --print_html >> $1
testIndex --print_html >> $1
testNdbApi --print_html >> $1
testNodeRestart --print_html >> $1
testOperations --print_html >> $1
testRestartGci --print_html >> $1
testScan --print_html >> $1
testScanInterpreter --print_html >> $1
testSystemRestart --print_html >> $1
echo "</table>" >> $1

