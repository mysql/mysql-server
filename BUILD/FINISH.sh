cflags="$c_warnings $extra_flags"
cxxflags="$cxx_warnings $base_cxxflags $extra_flags"
configure="./configure $base_configs $extra_configs"

CFLAGS="$cflags" CXX=gcc CXXFLAGS="$cxxflags" eval "$configure"

test "$make" = no || $make $AM_MAKEFLAGS
test -z "$strip" || strip mysqld
