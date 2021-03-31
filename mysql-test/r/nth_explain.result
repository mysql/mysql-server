# Test of SQL window functions NTH_VALUE
# ----------------------------------------------------------------------
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
CREATE TABLE t1 (d DOUBLE, id INT, sex CHAR(1));
INSERT INTO t1 VALUES (1.0, 1, 'M'),
(2.0, 2, 'F'),
(3.0, 3, 'F'),
(4.0, 4, 'F'),
(5.0, 5, 'M'),
(NULL, NULL, 'M'),
(10.0, 10, NULL),
(10.0, 10, NULL),
(11.0, 11, NULL);
ANALYZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	analyze	status	OK
Ok, default semantics:
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 1) RESPECT NULLS OVER () FROM t1;
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "1.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "<unnamed window>",
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,1) OVER ()  AS `NTH_VALUE(id, 1) RESPECT NULLS OVER ()` from `test`.`t1`
static wf
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 1) OVER () FROM t1;
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "1.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "<unnamed window>",
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,1) OVER ()  AS `NTH_VALUE(id, 1) OVER ()` from `test`.`t1`
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 3) OVER () FROM t1;
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "1.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "<unnamed window>",
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,3) OVER ()  AS `NTH_VALUE(id, 3) OVER ()` from `test`.`t1`
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY sex);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ) 
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY id);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`id` ) 
unbuffered
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID ROWS UNBOUNDED PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id`"
          ],
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID RANGE UNBOUNDED PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) 
test unoptimized path: trick: add DOUBLE type w/SUM which is unoptimized by default
ascending
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, sex, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY D ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`d` ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, sex, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY D RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`d` RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D ASC ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D ASC ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d`"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
descending
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, sex, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY D DESC ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`d` desc ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, sex, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY D DESC RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`d` desc RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT d, SUM(d) OVER w, COUNT(*) OVER w, NTH_VALUE(d, 2) OVER w FROM t1 WINDOW w as (ORDER BY D DESC RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`d` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true
          },
          "functions": [
            "sum",
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "d"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`d` AS `d`,sum(`test`.`t1`.`d`) OVER `w` AS `SUM(d) OVER w`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`d`,2) OVER `w` AS `NTH_VALUE(d, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`d` desc RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
Dynamic upper
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY id);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` ) 
optimized path
ascending
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID ASC ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID ASC ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id`"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
descending
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID DESC ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` desc ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, sex, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (PARTITION BY SEX ORDER BY ID DESC RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`sex`",
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id",
          "sex"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,`test`.`t1`.`sex` AS `sex`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (PARTITION BY `test`.`t1`.`sex` ORDER BY `test`.`t1`.`id` desc RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC ROWS 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc ROWS BETWEEN 2 PRECEDING AND 1 PRECEDING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc ROWS BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC RANGE 2 PRECEDING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc RANGE BETWEEN 2 PRECEDING AND CURRENT ROW) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc RANGE BETWEEN 1 PRECEDING AND 1 FOLLOWING) 
EXPLAIN FORMAT=JSON SELECT id, COUNT(*) OVER w, NTH_VALUE(id, 2) OVER w FROM t1 WINDOW w as (ORDER BY ID DESC RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING);
EXPLAIN
{
  "query_block": {
    "select_id": 1,
    "cost_info": {
      "query_cost": "10.15"
    },
    "windowing": {
      "windows": [
        {
          "name": "w",
          "using_filesort": true,
          "filesort_key": [
            "`id` desc"
          ],
          "frame_buffer": {
            "using_temporary_table": true,
            "optimized_frame_evaluation": true
          },
          "functions": [
            "count",
            "nth_value"
          ]
        }
      ],
      "cost_info": {
        "sort_cost": "9.00"
      },
      "table": {
        "table_name": "t1",
        "access_type": "ALL",
        "rows_examined_per_scan": 9,
        "rows_produced_per_join": 9,
        "filtered": "100.00",
        "cost_info": {
          "read_cost": "0.25",
          "eval_cost": "0.90",
          "prefix_cost": "1.15",
          "data_read_per_join": "216"
        },
        "used_columns": [
          "id"
        ]
      }
    }
  }
}
Warnings:
Note	1003	/* select#1 */ select `test`.`t1`.`id` AS `id`,count(0) OVER `w` AS `COUNT(*) OVER w`,nth_value(`test`.`t1`.`id`,2) OVER `w` AS `NTH_VALUE(id, 2) OVER w` from `test`.`t1` window `w` AS (ORDER BY `test`.`t1`.`id` desc RANGE BETWEEN 1 FOLLOWING AND 2 FOLLOWING) 
many nth_value calls on one window, optimized path
EXPLAIN FORMAT=TREE SELECT id, COUNT(*) OVER w,
NTH_VALUE(id, 3) OVER w,
FIRST_VALUE(id) OVER w,
NTH_VALUE(id, 1) OVER w,
NTH_VALUE(id, 2) OVER w FROM t1
WINDOW w AS (ORDER BY id ASC ROWS BETWEEN 2 PRECEDING AND CURRENT ROW);
EXPLAIN
-> Window aggregate with buffering: count(0) OVER w, nth_value(t1.id,3) OVER w, first_value(t1.id) OVER w, nth_value(t1.id,1) OVER w, nth_value(t1.id,2) OVER w
    -> Sort: t1.id
        -> Table scan on t1

many nth_value calls on one window, unoptimized path
EXPLAIN FORMAT=TREE SELECT id, SUM(d) OVER w, COUNT(*) OVER w,
NTH_VALUE(id, 3) OVER w,
FIRST_VALUE(id) OVER w,
NTH_VALUE(id, 1) OVER w,
NTH_VALUE(id, 2) OVER w FROM t1
WINDOW w AS (ORDER BY id ASC ROWS BETWEEN 2 PRECEDING AND CURRENT ROW);
EXPLAIN
-> Window multi-pass aggregate with buffering: sum(t1.d) OVER w, count(0) OVER w, nth_value(t1.id,3) OVER w, first_value(t1.id) OVER w, nth_value(t1.id,1) OVER w, nth_value(t1.id,2) OVER w
    -> Sort: t1.id
        -> Table scan on t1

DROP TABLE t1;
