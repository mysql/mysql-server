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
  -l [ --log_files ] arg        number of log files
  -q [ --uring_sqes ] arg       number of iouring SQEs
  -t [ --worker_threads ] arg   number of worker threads issue log request
  -g [ --log_size ] arg             average log size in bytes
  -u [ --use_iouring ] arg          use io_uring
  -w [ --log_entries_sync ] arg number of log entries wirte before invoke sync
  -s [ --run_seconds ] arg          running time(seoncds)
```

## Example

    TestLogUring 
        --log_size 1024 --log_files 1 --worker_threads 40 
        --use_iouring true --run_seconds 30 --log_entries_sync 4


# Run mysqld

## mysql database initialize

    mysqld --initialize-insecure

## run mysqld

enable environment
    
    export ENABLE_IO_STAT="any value"
    export ENABLE_LOG_URING="any value"
    mysqld