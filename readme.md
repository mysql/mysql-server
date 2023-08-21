# Build

## debug build
```
    cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_C_FLAGS_DEBUG="-g -O0" -DCMAKE_CXX_FLAGS_DEBUG="-g -O0" -DCMAKE_INSTALL_PREFIX="/home/mysql/mysql"
    make
    make DESTDIR=[path to install dir]
```

## build test io_uring log
```
    make TestLogUring
```

# Test



## TestLogUring argument 

```
Allowed options:
-h [ --help ]                     produce help message
-l [ --num_log_files ] arg        number of log files
-s [ --num_uring_sqes ] arg       number of iouring SQEs
-t [ --num_worker_threads ] arg   number of worker threads issue log request
-g [ --log_size ] arg             average log size in bytes
-u [ --use_iouring ] arg          use io_uring
-e [ --num_log_entries_sync ] arg number of log entries before invoke sync
```

## Example

    TestLogUring -g 1024 -l 1 -t 40 -u true


# Run mysqld

## mysql database initialize

    mysqld --initialize-insecure

## run mysqld

enable environment
    
    export ENABLE_IO_STAT="any value"
    export ENABLE_LOG_URING="any value"
    mysqld