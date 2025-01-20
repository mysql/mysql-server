#!/bin/bash

# Arguments

# SCRIPT_DIR=$( cd -- "$( dirname -- "$0" )" &> /dev/null && pwd )
# echo `date +"%x %X"` Script directory is $SCRIPT_DIR

echo `date +"%x %X"`  Initial directory `pwd`

REDHAT_VERSION=`sed -e 's/.*release \([0-9]*\).*/\1/' /etc/redhat-release`
echo `date +"%x %X"` "Running on RedHat version $REDHAT_VERSION"

GCC_VERSION=`gcc --version | head -n1 | sed -e 's/.*(GCC) \([0-9].[0-9]*\).*/\1/'`
echo `date +"%x %X"` "gcc version is $GCC_VERSION"

PYTHON3_EXISTS=`which python3 2> /dev/null | wc -l`
if [ $PYTHON3_EXISTS -lt 1 ]; then 
	echo `date +"%x %X"` "python3 is required for conan, but it does not seem to be installed. Aborting."
	exit 1
fi

PYTHON_EXISTS=`which python 2> /dev/null | wc -l`
if [ $PYTHON_EXISTS -ge 1 ]; then 
	PYTHON_VERSION=`python --version 2>&1 | sed -e 's/.*Python \([0-9]\).*/\1/'`
	echo `date +"%x %X"` "python version is $PYTHON_VERSION"

	if [ "$PYTHON_VERSION" == "2" ]; then 
		echo `date +"%x %X"` To use conan, link /usr/bin/python to /usr/bin/python3
		rm -f /usr/bin/python
		ln -s /usr/bin/python3 /usr/bin/python
	elif [ "$PYTHON_VERSION" != "3" ]; then
		echo `date +"%x %X"` Unknown version of python, $PYTHON_VERSION. Aborting
		exit 1
	fi
fi


# Import third party libs from conan and set env variables to point to the downloaded libraries

echo `date +"%x %X"` Fetching third party librairies from conan

export PATH=$PATH:/opt/cmake/bin

# Be sure the conanfile.txt contains the generator compiler_args
generator_folder=generatorfiles_lnx${REDHAT_VERSION}
rm -Rf $generator_folder; mkdir $generator_folder
conan install -pr profile.txt --build missing conanfile.txt -if $generator_folder -s compiler.version="$GCC_VERSION"

if [[ $? -ne 0 ]]; then
	echo `date +"%x %X"` Error: conan install failed. Aborting.
	exit 1
fi

echo `date +"%x %X"` Setting environment variables pointing to librairies

SSLDIR=`grep -m 1 'openssl.*include$'  $generator_folder/conanbuildinfo.txt` 
SSLDIR=${SSLDIR%/*} 
if [ -z "${SSLDIR}" ]; then
	echo `date +"%x %X"` Error: opennssl library has not been downloaded. Aborting.
	exit 1
fi
export SSLDIR
echo "Openssl dir $SSLDIR"

BOOSTDIR=`grep -m 1 'boost.*include$'  $generator_folder/conanbuildinfo.txt` 
BOOSTDIR=${BOOSTDIR%/*} 
if [ -z "${BOOSTDIR}" ]; then
	echo `date +"%x %X"` Error: boost library has not been downloaded. Aborting.
	exit 1
fi
export BOOSTDIR
echo "Boost dir $BOOSTDIR"


if [ "$PYTHON_VERSION" == "2" ]; then 
	echo `date +"%x %X"` Set the link /usr/bin/python back to /usr/bin/python2
	rm -f /usr/bin/python
	ln -s /usr/bin/python2 /usr/bin/python
fi
