##############################################
## Mysqlx_compression_algorithms = default (deflate,lz4,zstd_stream)

SET GLOBAL Mysqlx_compression_algorithms = DEFAULT;
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "deflate_stream"
          }
        }
      }
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "lz4_message"
          }
        }
      }
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "zstd_stream"
          }
        }
      }
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Ok {
}

ok
compression_algorithm = deflate_stream
Mysqlx.Ok {
}

ok
compression_algorithm = zstd_stream
Mysqlx.Ok {
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
##############################################
## Mysqlx_compression_algorithms = deflate

SET GLOBAL Mysqlx_compression_algorithms = deflate_stream;
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "deflate_stream"
          }
        }
      }
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = deflate_stream
Mysqlx.Ok {
}

ok
compression_algorithm = zstd_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
##############################################
## Mysqlx_compression_algorithms = lz4

SET GLOBAL Mysqlx_compression_algorithms = lz4_message;
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "lz4_message"
          }
        }
      }
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Ok {
}

ok
compression_algorithm = deflate_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = zstd_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
##############################################
## Mysqlx_compression_algorithms = zstd_stream

SET GLOBAL Mysqlx_compression_algorithms = zstd_stream;
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "zstd_stream"
          }
        }
      }
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = deflate_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = zstd_stream
Mysqlx.Ok {
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
##############################################
## Mysqlx_compression_algorithms = empty

SET GLOBAL Mysqlx_compression_algorithms = '';
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = deflate_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = zstd_stream
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
##############################################
## Mysqlx_compression_algorithms = deflate,lz4,zstd_stream

SET GLOBAL Mysqlx_compression_algorithms = 'deflate_stream,lz4_message,zstd_stream';
send Mysqlx.Connection.CapabilitiesGet {
}

Mysqlx.Connection.Capabilities(capabilities[4].value.obj.fld[0]) = Mysqlx.Datatypes.Object.ObjectField {
  key: "algorithm"
  value {
    type: ARRAY
    array {
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "deflate_stream"
          }
        }
      }
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "lz4_message"
          }
        }
      }
      value {
        type: SCALAR
        scalar {
          type: V_STRING
          v_string {
            value: "zstd_stream"
          }
        }
      }
    }
  }
}

ok
compression_algorithm = lz4_message
Mysqlx.Ok {
}

ok
compression_algorithm = deflate_stream
Mysqlx.Ok {
}

ok
compression_algorithm = zstd_stream
Mysqlx.Ok {
}

ok
compression_algorithm = wrong_name
Mysqlx.Error {
  severity: ERROR
  code: 5175
  msg: "Invalid or unsupported value for \'compression.algorithm\'"
  sql_state: "HY000"
}

ok
SET GLOBAL Mysqlx_compression_algorithms = DEFAULT;
