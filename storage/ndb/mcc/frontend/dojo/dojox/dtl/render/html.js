define([
	"dojo/_base/lang",
	"../render/dom",
	"../_base"
], function(lang,ddrd,dd){

	var ddrh = lang.getObject("render.html", true, dd);
	/*=====
	 ddrh = {
	 	// TODO: summary
	 };
	 =====*/

	ddrh.Render = ddrd.Render;

	return ddrh;
});
