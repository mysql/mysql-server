define(["dojo/sniff", "doh", "require"], function(has, doh, require){

	// IE9+ cannot handle quirks mode in test runner, see #14321
	has("ie") >= 9 || doh.register("testsDOH.uacss.sniffQuirks", require.toUrl("./uacss/sniffQuirks.html"));
	doh.register("testsDOH.uacss.sniffStandards", require.toUrl("./uacss/sniffStandards.html"));

});

