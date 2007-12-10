#!/bin/sh
# Copyright (C) 2000-2002, 2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Create a bug report and mail it to the mysql mailing list
# Based on glibc bug reporting script.

echo "Finding system information for a MySQL bug report"

VERSION="@VERSION@@MYSQL_SERVER_SUFFIX@"
COMPILATION_COMMENT="@COMPILATION_COMMENT@"
BUGmysql="mysql@lists.mysql.com"
# This is set by configure
COMP_CALL_INFO="CC='@SAVE_CC@'  CFLAGS='@SAVE_CFLAGS@'  CXX='@SAVE_CXX@'  CXXFLAGS='@SAVE_CXXFLAGS@'  LDFLAGS='@SAVE_LDFLAGS@'  ASFLAGS='@SAVE_ASFLAGS@'"
COMP_RUN_INFO="CC='@CC@'  CFLAGS='@CFLAGS@'  CXX='@CXX@'  CXXFLAGS='@CXXFLAGS@'  LDFLAGS='@LDFLAGS@'  ASFLAGS='@ASFLAGS@'"
CONFIGURE_LINE="@CONF_COMMAND@"

LIBC_INFO=""
for pat in /lib/libc.* /lib/libc-* /usr/lib/libc.* /usr/lib/libc-*
do
    TMP=`ls -l $pat 2>/dev/null`
    if test $? = 0
    then
      LIBC_INFO="$LIBC_INFO
$TMP"
    fi
done

PATH=../client:$PATH:/bin:/usr/bin:/usr/local/bin
export PATH

BUGADDR=${1-$BUGmysql}
ENVIRONMENT=`uname -a`

: ${USER=${LOGNAME-`whoami`}}

COMMAND=`echo $0|sed 's%.*/\([^/]*\)%\1%'`

# Try to create a secure tmpfile
umask 077
TEMPDIR=/tmp/mysqlbug-$$
mkdir $TEMPDIR || (echo "can not create directory in /tmp, aborting"; exit 1;)
TEMP=${TEMPDIR}/mysqlbug

trap 'rm -f $TEMP $TEMP.x; rmdir $TEMPDIR; exit 1' 1 2 3 13 15
trap 'rm -f $TEMP $TEMP.x; rmdir $TEMPDIR' 0

# How to read the passwd database.
PASSWD="cat /etc/passwd"

if test -f /usr/lib/sendmail
then
  MAIL_AGENT="/usr/lib/sendmail -oi -t"
elif test -f /usr/sbin/sendmail
then
  MAIL_AGENT="/usr/sbin/sendmail -oi -t"
else
  MAIL_AGENT="rmail $BUGmysql"
fi

# Figure out how to echo a string without a trailing newline
N=`echo 'hi there\c'`
case "$N" in
  *c)	ECHON1='echo -n' ECHON2= ;;
  *)	ECHON1=echo ECHON2='\c' ;;
esac

# Find out the name of the originator of this PR.
if test -n "$NAME" 
then
  ORIGINATOR="$NAME"
elif test -f $HOME/.fullname
then
  ORIGINATOR="`sed -e '1q' $HOME/.fullname`"
else
  # Must use temp file due to incompatibilities in quoting behavior
  # and to protect shell metacharacters in the expansion of $LOGNAME
  $PASSWD | grep "^$LOGNAME:" | awk -F: '{print $5}' | sed -e 's/,.*//' > $TEMP
  ORIGINATOR="`cat $TEMP`"
  rm -f $TEMP
fi

if test -n "$ORGANIZATION"
then
  if test -f "$ORGANIZATION"
  then
    ORGANIZATION="`cat $ORGANIZATION`"
  fi
else
  if test -f $HOME/.organization
  then
    ORGANIZATION="`cat $HOME/.organization`"
  elif test -f $HOME/.signature
  then
    ORGANIZATION=`sed -e "s/^/  /" $HOME/.signature; echo ">"`
  fi
fi

PATH_DIRS=`echo $PATH | sed -e 's/^:/. /' -e 's/:$/ ./' -e 's/::/ . /g' -e 's/:/ /g' `

which_1 ()
{
  for cmd
  do
    # Absolute path ?. 
    if expr "x$cmd" : "x/" > /dev/null
    then
      echo "$cmd"
      exit 0
    else
      for d in $PATH_DIRS
      do
	file="$d/$cmd"
	if test -x "$file" -a ! -d "$file"
	then
	  echo "$file"
	  exit 0
	fi
      done
    fi
  done
  exit 1
}

change_editor ()
{
  echo "You can change editor by setting the environment variable VISUAL."
  echo "If your shell is a bourne shell (sh) do"
  echo "VISUAL=your_editors_name; export VISUAL"
  echo "If your shell is a C shell (csh) do"
  echo "setenv VISUAL your_editors_name"
}

# If they don't have a preferred editor set, then use emacs
if test -z "$VISUAL"
then
  if test -z "$EDITOR"
  then
    # Honor debian sensible-editor
    if test -x "/usr/bin/sensible-editor"
    then
      EDIT=/usr/bin/sensible-editor
    else
      EDIT=emacs
    fi
  else
    EDIT="$EDITOR"
  fi
else
  EDIT="$VISUAL"
fi

#which_1 $EDIT
used_editor=`which_1 $EDIT`

echo "test -x $used_editor"
if test -x "$used_editor"
then
  echo "Using editor $used_editor";
  change_editor
  sleep 2
else
  echo "Could not find a text editor. (tried $EDIT)"
  change_editor
  exit 1
fi

# Find out some information.
SYSTEM=`( test -f /bin/uname  && /bin/uname -a ) || \
        ( test -f /usr/bin/uname  && /usr/bin/uname -a ) || echo ""`
ARCH=`test -f /bin/arch  && /bin/arch`
MACHINE=`test -f /bin/machine  && /bin/machine`
FILE_PATHS=

for cmd in perl make gmake gcc cc
do
  file=`which_1 $cmd`
  if test $? = 0
  then
    if test $cmd = "gcc"
    then
      GCC_INFO=`$file -v 2>&1`
    elif test $cmd = "perl"
    then
      PERL_INFO=`$file -v | grep -i version 2>&1`
    fi
    FILE_PATHS="$FILE_PATHS $file"
  fi
done

admin=`which_1 mysqladmin`
MYSQL_SERVER=
if test -x "$admin"
then
  MYSQL_SERVER=`$admin version 2> /dev/null`
  if test "$?" = "1"
  then
    MYSQL_SERVER=""
  fi
fi

SUBJECT_C="[50 character or so descriptive subject here (for reference)]"
ORGANIZATION_C='<organization of PR author (multiple lines)>'
LICENCE_C='[none | licence | email support | extended email support ]'
SYNOPSIS_C='<synopsis of the problem (one line)>'
SEVERITY_C='<[ non-critical | serious | critical ] (one line)>'
PRIORITY_C='<[ low | medium | high ] (one line)>'
CLASS_C='<[ sw-bug | doc-bug | change-request | support ] (one line)>'
RELEASE_C='<release number or tag (one line)>'
ENVIRONMENT_C='<machine, os, target, libraries (multiple lines)>'
DESCRIPTION_C='<precise description of the problem (multiple lines)>'
HOW_TO_REPEAT_C='<code/input/activities to reproduce the problem (multiple lines)>'
FIX_C='<how to correct or work around the problem, if known (multiple lines)>'


cat > $TEMP <<EOF
SEND-PR: -*- send-pr -*-
SEND-PR: Lines starting with \`SEND-PR' will be removed automatically, as
SEND-PR: will all comments (text enclosed in \`<' and \`>').
SEND-PR:
From: ${USER}
To: ${BUGADDR}
Subject: $SUBJECT_C

>Description:
	$DESCRIPTION_C
>How-To-Repeat:
	$HOW_TO_REPEAT_C
>Fix:
	$FIX_C

>Submitter-Id:	<submitter ID>
>Originator:	${ORIGINATOR}
>Organization:
${ORGANIZATION- $ORGANIZATION_C}
>MySQL support: $LICENCE_C
>Synopsis:	$SYNOPSIS_C
>Severity:	$SEVERITY_C
>Priority:	$PRIORITY_C
>Category:	mysql
>Class:		$CLASS_C
>Release:	mysql-${VERSION} ($COMPILATION_COMMENT)
`test -n "$MYSQL_SERVER" && echo ">Server: $MYSQL_SERVER"`
>C compiler:    @CC_VERSION@
>C++ compiler:  @CXX_VERSION@
>Environment:
	$ENVIRONMENT_C
`test -n "$SYSTEM"  && echo "System: $SYSTEM"`
`test -n "$ARCH"  && echo "Architecture: $ARCH"`
`test -n "$MACHINE"  && echo "Machine: $MACHINE"`
`test -n "$FILE_PATHS"  && echo "Some paths: $FILE_PATHS"`
`test -n "$GCC_INFO"  && echo "GCC: $GCC_INFO"`
`test -n "$COMP_CALL_INFO"  && echo "Compilation info (call): $COMP_CALL_INFO"`
`test -n "$COMP_RUN_INFO"   && echo "Compilation info (used): $COMP_RUN_INFO"`
`test -n "$LIBC_INFO"  && echo "LIBC: $LIBC_INFO"`
`test -n "$CONFIGURE_LINE"  && echo "Configure command: $CONFIGURE_LINE"`
`test -n "$PERL_INFO"  && echo "Perl: $PERL_INFO"`
EOF

chmod u+w $TEMP
cp $TEMP $TEMP.x

eval $EDIT $TEMP

if cmp -s $TEMP $TEMP.x
then
  echo "File not changed, no bug report submitted."
  mv -f $TEMP /tmp/failed-mysql-bugreport
  echo "The raw bug report exists in /tmp/failed-mysql-bugreport"
  echo "If you use this remember that the first lines of the report are now a lie.."
  exit 1
fi

#
#       Check the enumeration fields

# This is a "sed-subroutine" with one keyword parameter
# (with workaround for Sun sed bug)
#
SED_CMD='
/$PATTERN/{
s|||
s|<.*>||
s|^[ 	]*||
s|[ 	]*$||
p
q
}'


while :; do
  CNT=0

  #
  # 1) Severity
  #
  PATTERN=">Severity:"
  SEVERITY=`eval sed -n -e "\"$SED_CMD\"" $TEMP`
  case "$SEVERITY" in
    ""|non-critical|serious|critical) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$SEVERITY' is not a valid value for \`Severity'."
  esac
  #
  # 2) Priority
  #
  PATTERN=">Priority:"
  PRIORITY=`eval sed -n -e "\"$SED_CMD\"" $TEMP`
  case "$PRIORITY" in
    ""|low|medium|high) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$PRIORITY' is not a valid value for \`Priority'."
  esac
  #
  # 3) Class
  #
  PATTERN=">Class:"
  CLASS=`eval sed -n -e "\"$SED_CMD\"" $TEMP`
  case "$CLASS" in
    ""|sw-bug|doc-bug|change-request|support) CNT=`expr $CNT + 1` ;;
    *)  echo "$COMMAND: \`$CLASS' is not a valid value for \`Class'."
  esac

  #
  # 4) Synopsis
  #
  VALUE=`grep "^>Synopsis:" $TEMP | sed 's/>Synopsis:[ 	]*//'`
  case "$VALUE" in
    "$SYNOPSIS_C")  echo "$COMMAND: \`$VALUE' is not a valid value for \`Synopsis'." ;;
    *) CNT=`expr $CNT + 1` 
  esac

  test $CNT -lt 4  &&
    echo "Errors were found with the problem report."


  #       Check if subject of mail was changed, if not, use Synopsis field
  #
  subject=`grep "^Subject" $TEMP| sed 's/^Subject:[ 	]*//'`
  if [ X"$subject" = X"$SUBJECT_C" -o X"$subject" = X"$SYNOPSIS_C" ]; then
    subject=`grep Synopsis $TEMP | sed 's/>Synopsis:[     ]*//'`
    sed "s/^Subject:[ 	]*.*/Subject: $subject/" $TEMP > $TEMP.tmp
    mv -f $TEMP.tmp $TEMP
  fi

  while :; do
    $ECHON1 "a)bort, e)dit or s)end? $ECHON2"
    read input
    case "$input" in
      a*)
	echo "$COMMAND: problem report saved in $HOME/dead.mysqlbug."
	cat $TEMP >> $HOME/dead.mysqlbug
        xs=1; exit
        ;;
      e*)
        eval $EDIT $TEMP
        continue 2
        ;;
      s*)
        break 2
        ;;
    esac
  done
done
#
#       Remove comments and send the problem report
#       (we have to use patterns, where the comment contains regex chars)
#
# /^>Originator:/s;$ORIGINATOR;;
sed  -e "
/^SEND-PR:/d
/^>Organization:/,/^>[A-Za-z-]*:/s;$ORGANIZATION_C;;
/^>Confidential:/s;<.*>;;
/^>Synopsis:/s;$SYNOPSIS_C;;
/^>Severity:/s;<.*>;;
/^>Priority:/s;<.*>;;
/^>Class:/s;<.*>;;
/^>Release:/,/^>[A-Za-z-]*:/s;$RELEASE_C;;
/^>Environment:/,/^>[A-Za-z-]*:/s;$ENVIRONMENT_C;;
/^>Description:/,/^>[A-Za-z-]*:/s;$DESCRIPTION_C;;
/^>How-To-Repeat:/,/^>[A-Za-z-]*:/s;$HOW_TO_REPEAT_C;;
/^>Fix:/,/^>[A-Za-z-]*:/s;$FIX_C;;
" $TEMP > $TEMP.x

if $MAIL_AGENT < $TEMP.x
then
  echo "$COMMAND: problem report sent"
  xs=0; exit
else
  echo "$COMMAND: mysterious mail failure, report not sent."
  echo "$COMMAND: problem report saved in $HOME/dead.mysqlbug."
  cat $TEMP >> $HOME/dead.mysqlbug
fi

exit 0
