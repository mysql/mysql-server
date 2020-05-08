define(["doh/runner", "dojox/string/BidiEngine"], function(doh, BidiEngine) {

	var txt1 = "\u05d0\u05d1\u05d2 123 ABC 456.";
	var lengthErr = "Wrong length of the map";
	var contErr = "Wrong content of the map";
	var engine = new BidiEngine();
	var result;

	doh.register("BidiEngine Maps and Levels Test",
	[
		{
			name: "(1) Maps: implicit ltr -> visual ltr",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "ILYNN", "VLNNN");
				t.is(engine.sourceToTarget.length, result.length, lengthErr);
				t.is(engine.targetToSource.length, txt1.length, lengthErr);
				dojo.forEach(engine.sourceToTarget, function (val, ind) {
					t.is(txt1.charAt(val), result.charAt(ind), contErr);
				});
				dojo.forEach(engine.targetToSource, function (val, ind) {
					t.is(txt1.charAt(ind), result.charAt(val), contErr);
				});
			}
		},
		{
			name: "(2) Maps: implicit ltr -> visual rtl",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "ILYNN", "VRNNN");
				t.is(engine.sourceToTarget.length, result.length, lengthErr);
				t.is(engine.targetToSource.length, txt1.length, lengthErr);
				dojo.forEach(engine.sourceToTarget, function (val, ind) {
					t.is(txt1.charAt(val), result.charAt(ind), contErr);
				});
				dojo.forEach(engine.targetToSource, function (val, ind) {
					t.is(txt1.charAt(ind), result.charAt(val), contErr);
				});
			}
		},
		{
			name: "(3) Maps: implicit rtl -> visual ltr",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "IRYNN", "VLNNN");
				t.is(engine.sourceToTarget.length, result.length, lengthErr);
				t.is(engine.targetToSource.length, txt1.length, lengthErr);
				dojo.forEach(engine.sourceToTarget, function (val, ind) {
					t.is(txt1.charAt(val), result.charAt(ind), contErr);
				});
				dojo.forEach(engine.targetToSource, function (val, ind) {
					t.is(txt1.charAt(ind), result.charAt(val), contErr);
				});
			}
		},
		{
			name: "(4) Maps: implicit rtl -> visual rtl",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "ILYNN", "VRNNN");
				t.is(engine.sourceToTarget.length, result.length, lengthErr);
				t.is(engine.targetToSource.length, txt1.length, lengthErr);
				dojo.forEach(engine.sourceToTarget, function (val, ind) {
					t.is(txt1.charAt(val), result.charAt(ind), contErr);
				});
				dojo.forEach(engine.targetToSource, function (val, ind) {
					t.is(txt1.charAt(ind), result.charAt(val), contErr);
				});
			}
		},
		{
			name: "(5) Levels: ltr",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "ILYNN", "VRNNN");
				t.is(engine.levels.join(""), "1111222000000000", contErr);
			}
		},
		{
			name: "(6) Levels: rtl",
			runTest: function (t) {
				result = engine.bidiTransform(txt1, "IRYNN", "VLNNN");
				t.is(engine.levels.join(""), "1111222122222221", contErr);
			}
		}		
	]);
});