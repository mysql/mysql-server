cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
configure="./configure $base_configs $extra_configs"
for arg in "$@"; do
  configure="$configure "`echo "$arg" | sed -e 's,\([^a-zA-Z0-9_.-]\),\\\\\1,g'`
done


CFLAGS="$cflags" CXX=gcc CXXFLAGS="$cxxflags" eval "$configure"

test "$make" = no || $make $AM_MAKEFLAGS
test -z "$strip" || strip mysqld
