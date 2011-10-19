#!/bin/sh

if [ "`uname -o`" = "Cygwin" ]; then
  export DIFF='diff --ignore-space --strip-trailing-cr'
elif [ "`uname`" = "Darwin" ]; then
  export DIFF='diff'
else
  export DIFF='diff --ignore-space --strip-trailing-cr'
fi

compile_c() {
  if [ "`uname -o`" = "Cygwin" ]; then
    cl /W3 /I../.. /EHsc /FD /MD "$1" /link /DLL "/OUT:$2.dll" \
      ../../libase.lib 2> cl.log
  else
    $CXX -I../.. -O3 -g -Wall -fPIC -shared "$1" -o "$2.so"
  fi
}

compile_j() {
  if [ "`uname -o`" = "Cygwin" ]; then
    jdk="`echo /cygdrive/c/Program\ Files/Java/jdk* | head -1`"
  else
    jdk="$SUNJDK"
  fi
  "$jdk/bin/javac" -g "$1"/*.java
  "$jdk/bin/jar" -cf "$1.jar" "$1"/*.class
}

