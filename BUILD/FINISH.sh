cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
extra_configs="$extra_configs $local_infile_configs"
configure="./configure $base_configs $extra_configs"
for arg
do
  # Escape special characters so they don't confuse eval
  configure="$configure "`echo "$arg" | \
  		sed -e 's,\([^a-zA-Z0-9_.=-]\),\\\\\1,g'`
done

commands="\
$make -k clean || true 
/bin/rm -rf */.deps/*.P config.cache innobase/config.cache bdb/build_unix/config.cache bdb/dist/autom4te.cache autom4te.cache innobase/autom4te.cache;

aclocal    || (echo \"Can't execute aclocal\"     && exit 1)
autoheader || (echo \"Can't execute autoheader\"  && exit 1)
aclocal    || (echo \"Can't execute aclocal\"     && exit 1)
automake   || (echo \"Can't execute automake\"    && exit 1)
autoconf   || (echo \"Can't execute autoconf\"    && exit 1)
(cd bdb/dist && sh s_all)
(cd innobase && aclocal && autoheader && aclocal && automake && autoconf)
if [ -d gemini ]
then
   (cd gemini && aclocal && autoheader && aclocal && automake && autoconf)
fi

CFLAGS=\"$cflags\" CXX=\"$CXX\" CXXFLAGS=\"$cxxflags\" CXXLDFLAGS=\"$CXXLDFLAGS\" \
$configure"

if [ -z "$just_configure" ]
then
  commands="$commands

$make $AM_MAKEFLAGS"

  if [ "x$strip" = "xyes" ]
  then
    commands="$commands

mkdir -p tmp
nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
objdump -d sql/mysqld > tmp/mysqld.S
strip sql/mysqld"
  fi
fi

if test -z "$nonono"
then
  eval "set -x; $commands"
else
  echo "$commands"
fi
