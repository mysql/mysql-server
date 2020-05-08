dojo.provide("dojox.math.tests.BigInteger-ext");

dojo.require("dojox.math.BigInteger-ext");

tests.register("dojox.math.tests.BigInteger-ext",
	[
		function sanity_check(t){
			var x = new dojox.math.BigInteger("abcd1234", 16),
				y = new dojox.math.BigInteger("beef", 16),
				z = x.mod(y);
			t.is("b60c", z.toString(16));
		},
		function constructor_array(t){
			var x = new dojox.math.BigInteger([0, -1, -1, -1]);

			t.is(1, x.t)
		},
		function constructor_base10(t){
			var x = new dojox.math.BigInteger("10", 10);

			t.is("10", x.toString(10))
		},
		function constructor_without_arg(t){
			var x = new dojox.math.BigInteger("100");

			t.is("100", x.toString())
		},
		function minus_one_num_bytes(t){
			var x = new dojox.math.BigInteger("-1", 10);

			t.is(1, x.t)
		},
		function compare_pl0(t){
			var x = new dojox.math.BigInteger("18446744073709551616"),
				y = new dojox.math.BigInteger("18446744073709551616"),
				z = x.compareTo(y);

				t.is("0", z);
		},
		function compare_pl1(t){
			var x = new dojox.math.BigInteger("9223372036854775807"),
				y = new dojox.math.BigInteger("9223372036854775808"),
				z = x.compareTo(y);

				t.is("-1", z);
		},
		function compare_pl2(t){
			var x = new dojox.math.BigInteger("2147483647"),
				y = new dojox.math.BigInteger("65535"),
				z = x.compareTo(y);

				t.is("1", z);
		},
		function compare_pl3(t){
			var x = new dojox.math.BigInteger("65535"),
				y = new dojox.math.BigInteger("2147483647"),
				z = x.compareTo(y);

				t.is("-1", z);
		},
		function compare_mi0(t){
			var x = new dojox.math.BigInteger("-9223372036854775809"),
				y = new dojox.math.BigInteger("-9223372036854775809"),
				z = x.compareTo(y);

				t.is("0", z);
		},
		function compare_mi1(t){
			var x = new dojox.math.BigInteger("-9223372036854775808"),
				y = new dojox.math.BigInteger("-9223372036854775809"),
				z = x.compareTo(y);

				t.is("1", z);
		},
		function compare_mi2(t){
			var x = new dojox.math.BigInteger("-32768"),
				y = new dojox.math.BigInteger("-9223372036854775808"),
				z = x.compareTo(y);

				t.is("1", z);
		},
		function compare_mi3(t){
			var x = new dojox.math.BigInteger("-2147483648"),
				y = new dojox.math.BigInteger("-32768"),
				z = x.compareTo(y);

				t.is("-1", z);
		},
		function compare_mi1_mi1(t){
			var x = new dojox.math.BigInteger("-1"),
				y = new dojox.math.BigInteger("-1"),
				z = x.compareTo(y);

				t.is("0", z);
		},
		function compare_mi1_mi2(t){
			var x = new dojox.math.BigInteger("-1"),
				y = new dojox.math.BigInteger("-2"),
				z = x.compareTo(y);

				t.is("1", z);
		}
		,
		function compare_mi2_mi1(t){
			var x = new dojox.math.BigInteger("-2"),
				y = new dojox.math.BigInteger("-1"),
				z = x.compareTo(y);

				t.is("-1", z);
		}
	]
);