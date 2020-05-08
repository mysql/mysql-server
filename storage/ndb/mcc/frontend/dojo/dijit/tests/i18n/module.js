define(["doh/main", "require"], function(doh, require){

	doh.register("i18n.currency", require.toUrl("./currency.html"), 999999);
	doh.register("i18n.date", require.toUrl("./date.html"), 999999);
	doh.register("i18n.number", require.toUrl("./number.html"), 999999);
	doh.register("i18n.textbox", require.toUrl("./textbox.html"), 999999);
	doh.register("i18n.time", require.toUrl("./time.html"), 999999);
	doh.register("i18n.digit", require.toUrl("./digit.html"), 999999);

});
