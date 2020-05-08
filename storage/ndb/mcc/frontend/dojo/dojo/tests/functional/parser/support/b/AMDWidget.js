define([
	"require",
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dojo/parser",
	"./AMDWidget2"
], function(require, declare, domConstruct, parser) {

	return declare(null, {
		constructor : function() {
			var node = domConstruct.create("div", {
				innerHTML : '<div data-testing-type="./AMDWidget2"></div>'
			});
			this.child = parser.parse(node, {
				contextRequire : require
			})[0];
		}
	});
});