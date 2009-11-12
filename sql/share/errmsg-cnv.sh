#
# This shell script converts errmsg.txt
# from a mixed-charset format
# to utf8 format
# and writes the result to errmgs-utf8.txt
# 


cat errmsg.txt | while IFS= ; read -r a
do
cs=""

var="${a#"${a%%[![:space:]]*}"}"

case $var in
cze*|hun*|pol*|rum*|slo*)
  cs=latin2
  ;;
dan*|nla*|eng*|fre*|ger*|ita*|nor*|por*|spa*|swe*)
  cs=latin1
  ;;
est*)
  cs=latin7
  ;;
greek*)
  cs=windows-1253
  ;;
jpn*)
  cs=euc-jp
  ;;
jps*)
  cs=shift-jis
  ;;
kor*)
  cs=euc-kr
  ;;
serbian*)
  cs=windows-1250
  ;;
rus*)
  cs=koi8-r
  ;;
ukr*)
  cs=koi8-u
  ;;
*)
  echo $a
esac

if [ "x$cs" != "x" ]
then
  b=`echo $a | iconv -f $cs -t utf-8` ; rc=$?
  if [ "$rc" == "0" ]
  then
    echo "$b"
  else
    echo "# This message failed to convert from $cs, skipped"
  fi
fi
done > errmsg-utf8.txt

