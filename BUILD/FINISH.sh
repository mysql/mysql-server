cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
configure="./configure $base_configs $extra_configs"
for arg in "$@"; do
  configure="$configure "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.-]\),\\\\\1,g'`
done


eval "CFLAGS='$cflags' CXX=gcc CXXFLAGS='$cxxflags' $configure"

if [ "x$do_make" = "xno" ] ; then
 exit 0
fi

$make $AM_MAKEFLAGS
if [ "x$strip" = "xyes" ]; then
  mkdir -p tmp
  nm --numeric-sort sql/mysqld  > tmp/mysqld.sym
  objdump -d sql/mysqld > tmp/mysqld.S
  strip sql/mysqld
fi  
