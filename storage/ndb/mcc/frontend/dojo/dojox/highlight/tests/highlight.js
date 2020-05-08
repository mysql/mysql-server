dojo.provide("dojox.highlight.tests.highlight");

dojo.require("dojox.highlight");
dojo.require("dojox.highlight.languages._all");

doh.register("dojox.highlight.tests.highlight", [
	function test_validjavascript(){
		// summary:
		//		Test a valid javascript block is highlighted correctly
		var unformatted = "console.debug('hello'); /*Hi*/";
		var expected = "console.debug(<span class=\"string\">'hello'</span>); <span class=\"comment\">/*Hi*/</span>";
		var result = dojox.highlight.processString(unformatted, "javascript");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("javascript", result.langName);
	},
	function test_invalidjavascript(){
		// summary:
		//		Test an invalid javascript block with partial result
		var unformatted = "console.debug('hello);\n /*Hi*/";
		//			                     ^_ unmatched quote
		var expected = "console.debug(<span class=\"string\">";
		var result = dojox.highlight.processString(unformatted, "javascript");
		doh.assertEqual(unformatted, result.result);
		doh.assertEqual(expected, result.partialResult);
		doh.assertEqual("javascript", result.langName);
	},
	function test_validpython_raw3singlequotestring(){
		// summary:
		//    Test a valid Python raw triple single-quoted string literal
		//    is highlighted correctly
		var unformatted = "s = r'''value'''";
		var expected = "s = <span class=\"string\">r'''value'''</span>";
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
	},
	function test_validpython_3singlequoteunicode(){
		// summary:
		//    Test a valid Python triple single-quoted unicode literal
		//    is highlighted correctly
		var unformatted = "s = u'''value'''";
		var expected = "s = <span class=\"string\">u'''value'''</span>";
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
 	},
	function test_validpython_raw3singlequoteunicode(){
		// summary:
		//    Test a valid Python raw triple single-quoted unicode literal
		//    is highlighted correctly
		var unformatted = "s = ur'''value'''";
		var expected = "s = <span class=\"string\">ur'''value'''</span>";
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
	},
	function test_validpython_raw3doublequotestring(){
		// summary:
		//    Test a valid Python raw triple double-quoted string literal
		//    is highlighted correctly
		var unformatted = 's = r"""value"""';
		var expected = 's = <span class="string">r"""value"""</span>';
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
	},
	function test_validpython_3doublequoteunicode(){
		// summary:
		//    Test a valid Python triple double-quoted unicode literal
		//    is highlighted correctly
		var unformatted = 's = u"""value"""';
		var expected = 's = <span class="string">u"""value"""</span>';
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
	},
	function test_validpython_raw3doublequoteunicode(){
		// summary:
		//    Test a valid Python raw triple double-quoted unicode literal
		//    is highlighted correctly
		var unformatted = 's = ur"""value"""';
		var expected = 's = <span class="string">ur"""value"""</span>';
		var result = dojox.highlight.processString(unformatted, "python");
		doh.assertEqual(expected, result.result);
		doh.assertTrue(!result.partialResult);
		doh.assertEqual("python", result.langName);
	}
]);
