
# Warning - first line left blank for sh/csh/ksh compatibility.  Do not
# remove it.  fnf@Unisoft

# doinstall.sh --- figure out environment and do recursive make with
# appropriate pathnames.  Works under SV or BSD.

if [ -r /usr/include/search.h ]
then
	# System V
	$* LLIB=/usr/lib
else
	# 4.2 BSD
	$* LLIB=/usr/lib/lint
fi
