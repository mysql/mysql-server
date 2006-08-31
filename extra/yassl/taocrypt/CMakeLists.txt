INCLUDE_DIRECTORIES(../mySTL include)

ADD_LIBRARY(taocrypt src/aes.cpp src/aestables.cpp src/algebra.cpp src/arc4.cpp src/asn.cpp src/coding.cpp 
				src/des.cpp src/dh.cpp src/dsa.cpp src/file.cpp src/hash.cpp src/integer.cpp src/md2.cpp 
				src/md4.cpp src/md5.cpp src/misc.cpp src/random.cpp src/ripemd.cpp src/rsa.cpp src/sha.cpp
				include/aes.hpp include/algebra.hpp include/arc4.hpp include/asn.hpp include/block.hpp 
				include/coding.hpp include/des.hpp include/dh.hpp include/dsa.hpp include/dsa.hpp
				include/error.hpp include/file.hpp include/hash.hpp include/hmac.hpp include/integer.hpp 
				include/md2.hpp include/md5.hpp include/misc.hpp include/modarith.hpp include/modes.hpp
				include/random.hpp include/ripemd.hpp include/rsa.hpp include/sha.hpp)
