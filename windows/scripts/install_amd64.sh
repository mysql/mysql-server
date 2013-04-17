#!/usr/bin/bash

#Install BDB
echo Installing BDB  if necessary ...
if ! test -d /usr/local/BerkeleyDB.4.6 ; then
    (
        echo "BDB is missing.  Downloading from svn" &&
        cd /usr/local &&
        svn co -q https://svn.tokutek.com/tokudb/berkeleydb/windows/amd64/BerkeleyDB.4.6
    ) || { echo Failed; exit 1; }
fi
if ! grep 'export BDB=' ~/.bashrc > /dev/null; then
    echo "Adding 'export BDB=/usr/local/BerkeleyDB.4.6' to ~/.bashrc"
    echo 'export BDB=/usr/local/BerkeleyDB.4.6' >> ~/.bashrc
fi
if ! grep 'export BDBDIR=' ~/.bashrc > /dev/null; then
    echo "Adding 'export BDBDIR=C:/cygwin/usr/local/BerkeleyDB.4.6'' to ~/.bashrc"
    echo 'export BDBDIR=C:/cygwin/usr/local/BerkeleyDB.4.6' >> ~/.bashrc
fi
echo Done installing BDB.
echo 


#Install licenses.
if ! test -e ../licenses/install_licenses_amd64.bat; then
    echo Missing ../licenses directory.
    exit 1
fi
echo Installing licenses...
(cd ../licenses && cmd /c install_licenses_amd64.bat)
echo Done installing licenses.
echo


#install icc integration
(
    cd amd64 &&
    if ! diff -q Cygwin.bat /cygdrive/c/cygwin/Cygwin.bat > /dev/null; then
        cp Cygwin.bat /cygdrive/c/cygwin/
    fi
)
if ! grep 'export CC=' ~/.bashrc > /dev/null; then
    echo "Adding 'export CC=icc' to ~/.bashrc"
    echo 'export CC=icc' >> ~/.bashrc
fi
if ! grep 'export CYGWIN=' ~/.bashrc > /dev/null; then
    echo "Adding 'export CYGWIN=CYGWIN' to ~/.bashrc"
    echo 'export CYGWIN=CYGWIN' >> ~/.bashrc
fi

#cygwin link is in the way
if test -e /usr/bin/link; then
    mv /usr/bin/link /usr/bin/link_DISABLED
fi
#cygwin cmake is in the way
if test -e /usr/bin/cmake; then
    mv /usr/bin/cmake /usr/bin/cmake_DISABLED
fi

#Set up aliases
( cd amd64/symlinks && cp -d * /usr/local/bin/ )

echo You can now install the intel compiler.
echo You must restart cygwin after intel compiler is installed.
echo If the intel compiler is already installed, just restart cygwin.



