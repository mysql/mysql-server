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

test setting:

    MAXITEMS=100000
    CUST_PER_DIST=1000
    DIST_PER_WARE=10
    ORD_PER_DIST=1000
    MAX_NUM_ITEMS=15
    MAX_ITEM_LEN=14
    warehouse number = 20

1, 10, 50, 100, 150  threads:

    total write: 1513984 bytes, append count: 1568, avg log size: 965.551 bytes, append/s: 783.954, sync/s: 520.97
    total write: 3271168 bytes, append count: 2050, avg log size: 1595.69 bytes, append/s: 1024.96, sync/s: 544.479
    total write: 4778496 bytes, append count: 2158, avg log size: 2214.32 bytes, append/s: 1078.96, sync/s: 445.483
    total write: 4897792 bytes, append count: 2411, avg log size: 2031.44 bytes, append/s: 1205.45, sync/s: 439.984
    total write: 4748800 bytes, append count: 2249, avg log size: 2111.52 bytes, append/s: 1124.46, sync/s: 461.482


    {"terminals": 1, "warehouses": 20, "num_item": 100000, "tpm": 2733.6}
    {"terminals": 10, "warehouses": 20, "num_item": 100000, "tpm": 7140.0}
    {"terminals": 50, "warehouses": 20, "num_item": 100000, "tpm": 11761.8}
    {"terminals": 100, "warehouses": 20, "num_item": 100000, "tpm": 11793.6}
    {"terminals": 150, "warehouses": 20, "num_item": 100000, "tpm": 11567.4}


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