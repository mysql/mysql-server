#!/bin/bash

# Copyright (c) 2007, 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#############################################################
# This script created by Jonas does the following	    #
# Cleans up clones and pevious builds, pulls new clones,    #
# builds, deploys, configures the tests and launches ATRT   #
#############################################################

###############
#Script setup #
##############

save_args=$*
VERSION="autotest-run.sh version 1.25"

DATE=`date '+%Y-%m-%d'`
if [ `uname -s` != "SunOS" ]
then
  if [ `uname | grep -ic cygwin || true` -ne 0 ]
  then
    HOST=`hostname`
    # Returns windows CRLF
    HOST=`echo $HOST | tr -d "\015"`
    echo "Host: '$HOST'"
  else
    HOST=`hostname -s`
  fi
else
  HOST=`hostname`
fi
export DATE HOST

set -e
ulimit -Sc unlimited

echo "`date` starting: $*"

RSYNC_RSH=ssh
export RSYNC_RSH

verbose=0
report=yes
nolock=
clonename=
RUN="daily-basic"
conf=autotest.conf
LOCK=$HOME/.autotest-lock

############################
# Read command line entries#
############################

while [ "${1}" ]; do
  case "${1}" in
    --atrt-*) atrt_arg=$(echo "${1}" | sed s/--atrt-/--/);
        atrt_args="${atrt_args} ${atrt_arg}";;
    --verbose=*) verbose=$(echo "${1}" | sed s/--verbose=//);;
    --verbose) verbose=$(expr "${verbose}" + 1);;
    --conf=*) conf=$(echo "${1}" | sed s/--conf=//);;
    --version) echo "${VERSION}"; exit;;
    --suite=*) RUN=$(echo "${1}" | sed s/--suite=//);;
    --suite-suffix=*) suite_suffix=$(echo "${1}" | sed s/--suite-suffix=//);;
    --base-dir=*) base_dir=$(echo $1 | sed s/--base-dir=//);;
    --clusters=*) clusters_arg="${1}"; atrt_args="${atrt_args} ${1}";;
    --run-dir=*) run_dir=$(echo "${1}" | sed s/--run-dir=//);;
    --custom-atrt=*) custom_atrt=$(echo "${1}" | sed s/--custom-atrt=//);;
    --custom-cpcc=*) custom_cpcc=$(echo "${1}" | sed s/--custom-cpcc=//);;
    --install-dir=*) install_dir=$(echo "${1}" | sed s/--install-dir=//);;
    --install-dir0=*) install_dir0=$(echo "${1}" | sed s/--install-dir0=//);;
    --install-dir1=*) install_dir1=$(echo "${1}" | sed s/--install-dir1=//);;
    --clone=*) clone0=$(echo "${1}" | sed s/--clone=//);;
    --clone0=*) clone0=$(echo "${1}" | sed s/--clone0=//);;
    --clone1=*) clone1=$(echo "${1}" | sed s/--clone1=//);;
    --nolock) nolock=true;;
    --clonename=*) clonename=$(echo "${1}" | sed s/--clonename=//);;
  esac
  shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			#
#################################

install_dir=${install_dir:-$install_dir0}
install_dir0=${install_dir0:-$install_dir}

install_dir_save=$install_dir0
if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: $conf"
	exit
fi
install_dir0=$install_dir_save

if [ -z "$run_dir" ]
then
    if [ "$install_dir1" ]
    then
	echo "--run-dir not specified but install_dir1 specified"
	echo "giving up"
	exit
    fi
    run_dir=$install_dir0
fi

###############################
# Validate that all interesting
#   variables where set in conf
###############################
vars="target base_dir install_dir0 hosts"
if [ "$report" ]
then
	vars="$vars result_host result_path"
fi
for i in $vars
do
  t=`echo echo \\$$i`
  if [ -z "`eval $t`" ]
  then
      echo "Invalid config: $conf, variable $i is not set"
      exit
  fi
done

###############################
#Print out the enviroment vars#
###############################

if [ $verbose -gt 0 ]
then
	env
fi

#######################################
# Check to see if the lock file exists#
# If it does exit. 		      #
#######################################

if [ -z "$nolock" ]
then
    if [ -f $LOCK ]
    then
	echo "Lock file exists: $LOCK"
	exit 1
    fi
    echo "$DATE $RUN" > $LOCK
fi

on_exit() {
####################################
# Revert copy of test programs
####################################
  for f in "${run_dir}/revert_copy_missing_ndbclient_test_programs"*
  do
    if [ -f "${f}" ]
    then
      source "${f}"
    fi
  done
####################################
# Remove the lock file before exit #
####################################
  if [ -z "${nolock}" ] &&
     [ -f "${LOCK}" ]
  then
    rm -f "${LOCK}"
  fi
}
trap on_exit EXIT

###############################################
# Check that all interesting files are present#
###############################################

test_dir=$install_dir/mysql-test/ndb

# Check if executables in $install_dir0 is executable at current
# platform, they could be built for another kind of platform
unset NDB_CPCC_HOSTS

if [ -n "$custom_atrt" ];
then
  echo "Using custom atrt ${custom_atrt}"
  atrt="${custom_atrt}"
elif ${install_dir}/bin/ndb_cpcc 2>/dev/null
then
  echo "Using atrt from test build"
  atrt="${test_dir}/atrt"
else
  echo "Note: Cross platform testing, atrt used from server path" >&2
  atrt=`which atrt`
fi

if [ -n "$custom_cpcc" ];
then
  echo "Using custom ndb_cpcc ${custom_cpcc}"
  ndb_cpcc="${custom_cpcc}"
elif ${install_dir}/bin/ndb_cpcc 2>/dev/null
then
  echo "Using ndb_cpcc from test build"
  ndb_cpcc="${install_dir}/bin/ndb_cpcc"
else
  echo "Note: Cross platform testing, ndb_cpcc used from server path" >&2
  ndb_cpcc=`which ndb_cpcc`
fi

if [ -n "${suite_suffix}" ]; then
  suite_suffix="--${suite_suffix}"
fi

test_file="${test_dir}/${RUN}${suite_suffix}-tests.txt"
if [ ! -f "$test_file" ]; then
    echo "Cant find testfile: $test_file"
    exit 1
fi

if [ ! -x "$atrt" ]
then
    echo "Cant find atrt binary at $atrt"
    exit 1
fi

############################
# check ndb_cpcc fail hosts#
############################
failed=`$ndb_cpcc $hosts | awk '{ if($1=="Failed"){ print;}}'`
if [ "$failed" ]
then
  echo "Cant contact cpcd on $failed, exiting"
  exit 1
fi

#############################
# Function for replacing the#
# choose host with real host#
# names. Note $$ = PID	    #
#############################
choose(){
        SRC=$1
        TMP1=/tmp/choose.$$
        TMP2=/tmp/choose.$$.$$
        shift

        cp $SRC $TMP1
        i=1
        while [ $# -gt 0 ]
        do
                sed -E s/"CHOOSE_host${i}([^0-9]|\$)"/"${1}\1"/g < $TMP1 > $TMP2
                mv $TMP2 $TMP1
                shift
                i=`expr $i + 1`
        done
        cat $TMP1
        rm -f $TMP1
}

choose_conf() {
  local testsuite="${1}"

  local search_path=(
    "${test_dir}/conf-${testsuite}-${HOST}.cnf"
    "${test_dir}/conf-${testsuite}.cnf"
    "${test_dir}/conf-${HOST}.cnf"
    "${test_dir}/conf-${testsuite}-autotest.cnf"
    "${test_dir}/conf-autotest.cnf"
  )

  for conf in "${search_path[@]}"; do
    if [ -f "${conf}" ]; then
      echo "${conf}"
      return
    fi
  done

  echo "Unable to find conf file looked for" 1>&2
  for conf in "${search_path[@]}"; do
    echo " * ${conf}" 1>&2
  done
  exit 1
}

#########################################
# Count how many computers we have ready#
#########################################

count_hosts(){
    ch="CHOOSE_host"
    list=`grep $ch $1 | sed 's!,! !g'`
    cnt=`for i in $list; do echo $i; done | grep $ch | sort | uniq | wc -l`
    echo $cnt
}

conf=`choose_conf $RUN`
count=`count_hosts $conf`
avail=`echo $hosts | wc -w`
if  [ $count -gt $avail ]
    then
    echo "Not enough hosts"
    echo "Needs: $count available: $avail ($avail_hosts)"
    exit 1
fi

###
# Make directories needed

p=`pwd`
run_dir=$run_dir/run-$RUN-$clone0-$target
res_dir=$base_dir/result-$RUN-$clone0-$target/$DATE
tar_dir=$base_dir/saved-results

mkdir -p $run_dir $res_dir $tar_dir
rm -rf $res_dir/* $run_dir/*

###
#
# Do sed substitiutions
#
cd $run_dir
mkdir run

if [ `uname | grep -ic cygwin || true` -ne 0 ]
then
  run_dir=`cygpath -m $run_dir`
  install_dir0=`cygpath -u $install_dir0`
  if [ "$install_dir1" ]
  then
    install_dir1=`cygpath -u $install_dir1`
  fi
  test_dir=`cygpath -m $test_dir`
fi

choose $conf $hosts > d.tmp.$$
sed -e s,CHOOSE_dir,"$run_dir/run",g < d.tmp.$$ > my.cnf

clusters=`echo ${clusters_arg} | sed s/--clusters=//`
for cluster_name in ${clusters//,/ }; do
  conf_base=$(echo "${conf}" | sed 's/\.cnf$//')
  config_ini="${conf_base}${cluster_name}.ini"

  if [ -f "$config_ini" ]; then
    [ -r "$config_ini" ] || (echo "Failed to read ${config_ini}" && exit 1)

    choose "$config_ini" $hosts > d.tmp.$$
    sed -e s,CHOOSE_dir,"$run_dir/run",g < d.tmp.$$ > "config${cluster_name}.ini"
  fi
done

rm -f d.tmp.$$

copy_missing_ndbclient_test_programs() {
  if [ -f "${2}/bin/testDowngrade" ]
  then
    # Assume nothing need to be copied
    return
  fi
  (
    export LD_LIBRARY_PATH="${1}/bin:${1}/lib"
    for prog in testDowngrade testUpgrade
    do
      if [ -f "${1}/bin/${prog}" ] &&
         [ ! -f "${2}/bin/${prog}" ] &&
         ldd "${1}/bin/${prog}" | grep -c ndbclient
      then
        echo "rm -f '$(realpath ${2}/bin/${prog})'" &&
          cp -p "${1}/bin/${prog}" "${2}/bin/${prog}"
      fi
    done
    # May for example copy suite files *grade*-tests.txt and
    # config files conf-*grade*.cnf
    for file in "${1}"/mysql-test/ndb/*grade*
    do
      f=$(basename "${file}")
      if [ -f "${file}" ] &&
         [ ! -f "${2}/mysql-test/ndb/${f}" ]
      then
        echo "rm -f '$(realpath ${2}/mysql-test/ndb/${f})'" &&
          cp -p "${file}" "${2}/mysql-test/ndb/${f}"
      fi
    done
  ) > revert_copy_missing_ndbclient_test_programs${3}
}
prefix="--prefix=$install_dir --prefix0=$install_dir0"
if [ -n "$install_dir1" ]
then
    prefix="$prefix --prefix1=$install_dir1"
    copy_missing_ndbclient_test_programs ${install_dir0} ${install_dir1} _0_1
    copy_missing_ndbclient_test_programs ${install_dir1} ${install_dir0} _1_0
fi

# If verbose level 0, use default verbose mode (1) for atrt anyway
# otherwise it will not write test progress to log file
if [ ${verbose} -gt 0 ] ; then
  verbose_arg=--verbose=${verbose}
fi

# Setup configuration
${atrt} ${atrt_args} Cdq \
   ${prefix} \
   ${verbose_arg} \
   my.cnf \
   | tee log.txt

atrt_conf_status=${PIPESTATUS[0]}
if [ ${atrt_conf_status} -ne 0 ]; then
    return_code=$atrt_conf_status
    echo "Setup configuration failure"
else
    args="${atrt_args}"
    args="${args} --report-file=report.txt"
    args="${args} --testcase-file=${test_file}"
    args="${args} ${prefix}"
    args="${args} ${verbose_arg}"
    ${atrt} ${args} my.cnf | tee -a log.txt

    atrt_test_status=${PIPESTATUS[0]}
    if [ $atrt_test_status -ne 0 ]; then
        return_code=$atrt_test_status
        echo "ERROR: ${atrt_test_status}: ${atrt} ${args} my.cnf"
    fi
fi

# Make tar-ball
[ -f my.cnf ] && mv my.cnf $res_dir
[ -f log.txt ] && mv log.txt $res_dir
[ -f report.txt ] && mv report.txt $res_dir
[ "`find . -name 'result*'`" ] && mv result* $res_dir
[ -f coverage.info ] && mv coverage.info "${res_dir}"
[ -d test_coverage ] && mv test_coverage "${res_dir}"
cd $res_dir

echo "date=$DATE" > info.txt
echo "suite=$RUN" >> info.txt
echo "clone=$clone0" >> info.txt
echo "arch=${target/-*}" >> info.txt
echo "os=${target/*-}" >> info.txt
echo "host=$HOST" >> info.txt
[ -z "${clusters_arg}" ] || echo "clusters=${clusters_arg/--clusters=/}" >> info.txt
echo "test_hosts='$hosts'" >> info.txt
echo "test_atrt_command='${atrt} ${args} my.cnf'" >> info.txt
if [ "$clone1" ]
then
    echo "clone1=$clone1" >> info.txt
    [ ! -f $install_dir1/code1.txt ] || cp $install_dir1/code1.txt .
fi
[ ! -f $install_dir0/code0.txt ] || cp $install_dir0/code0.txt .
if [ "$clonename" ]
then
    echo "clonename=$clonename" >> info.txt
else
    echo "clonename=$clone0" >> info.txt
fi
for f in $(find "${install_dir0}/docs" -name 'INFO_*'); do
  cp "${f}" "$(basename ${f}).0"
done
if [ -d "${install_dir1}/docs" ]; then
  for f in $(find "${install_dir1}/docs" -name 'INFO_*'); do
    cp "${f}" "$(basename ${f}).1"
  done
fi
find . | xargs chmod ugo+r

# Try to pack and transfer as much as possible
set +e

cd ..
p2=`pwd`
cd ..
tarfile=res.$RUN.$clone0.$target.$DATE.$HOST.$$.tgz
if [ `uname -s` != "SunOS" ]
then
    tar cfz $tar_dir/$tarfile `basename $p2`/$DATE
else
    tarfile2=res.$RUN.$clone0.$target.$DATE.$HOST.$$.tar
    tar cf $tar_dir/$tarfile2 `basename $p2`/$DATE
    gzip -c $tar_dir/$tarfile2 > $tar_dir/$tarfile
    rm -f $tar_dir/$tarfile2
fi

if [ "$report" ]
then
    scp $tar_dir/$tarfile $result_host:$result_path/${tarfile}.upload
    ssh $result_host mv $result_path/${tarfile}.upload $result_path/${tarfile}
fi

cd $p
rm -rf $res_dir $run_dir

exit $return_code
