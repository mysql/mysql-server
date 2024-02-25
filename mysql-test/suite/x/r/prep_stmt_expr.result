CREATE SCHEMA IF NOT EXISTS xtest;
USE xtest;

command ok

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

1 rows affected

 1. 'cont_in' operator (placholder_scalar in member_array)
==========================================================
RUN recvok
1.a cont_in operator against string
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
1.b cont_in operator against integer
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
1.c cont_in operator against double
doc
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
1.d cont_in operator against bool
doc
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
1.e cont_in operator against null
doc
{"_id": "105", "array": [null], "scalar": null}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
RUN recvok

 2. 'cont_in' operator (member_scalar in placholder_scalar)
===========================================================
RUN recvok
2.a cont_in operator against string
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
command ok
2.b cont_in operator against integer
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
command ok
2.c cont_in operator against double
doc
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
command ok
2.d cont_in operator against bool
doc
{"_id": "104", "array": [true, false], "scalar": true}
command ok
2.e cont_in operator against null
doc
{"_id": "105", "array": [null], "scalar": null}
command ok
RUN recvok

 3. 'not_cont_in' operator (placholder_scalar in member_array)
==============================================================
RUN recvok
3.a not_cont_in operator against string
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
command ok
3.b not_cont_in operator against integer
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
command ok
3.c not_cont_in operator against double
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
command ok
3.d not_cont_in operator against bool
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "105", "array": [null], "scalar": null}
command ok
3.e not_cont_in operator against null
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
command ok
RUN recvok

 4. Simple comparison with placeholder
======================================
RUN recvok
4.a simple comparison with string
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
command ok
4.b simple comparison with integer
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
command ok
4.c simple comparison with double
doc
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
command ok
4.d simple comparison with bool
doc
{"_id": "104", "array": [true, false], "scalar": true}
command ok
4.e simple comparison with null
doc
{"_id": "105", "array": [null], "scalar": null}
command ok
RUN recvok

 5. 'cont_in' operator and '!=' on the same placeholder
=======================================================
RUN recvok
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
command ok
RUN recvok

 6. 'cont_in' operator (member_scalar in placholder_array)
==========================================================
RUN recvok
6.a cont_in operator against string
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
6.b cont_in operator against integer
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
command ok
6.c cont_in operator against double
doc
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
command ok
6.d cont_in operator against bool
doc
{"_id": "104", "array": [true, false], "scalar": true}
command ok
6.e cont_in operator against null
doc
{"_id": "105", "array": [null], "scalar": null}
command ok
6.e cont_in operator against mix of values
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
command ok
RUN recvok

 7. 'not_cont_in' operator (member_scalar in placholder_array)
==============================================================
RUN recvok
7.a not_cont_in operator against string
doc
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
command ok
7.b not_cont_in operator against integer
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
7.c not_cont_in operator against double
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "105", "array": [null], "scalar": null}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
7.d not_cont_in operator against bool
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "105", "array": [null], "scalar": null}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
7.e not_cont_in operator against null
doc
{"_id": "101", "array": ["abc", "cde", "efg"], "scalar": "abc"}
{"_id": "102", "array": [123, 345, 567], "scalar": 123}
{"_id": "103", "array": [2.3, 3.4, 4.5], "scalar": 2.3}
{"_id": "104", "array": [true, false], "scalar": true}
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
7.e not_cont_in operator against mix of values
doc
{"_id": "106", "array": ["cde", 345, 3.4, true, null], "scalar": "cde"}
command ok
RUN recvok
Mysqlx.Ok {
  msg: "bye!"
}
ok
