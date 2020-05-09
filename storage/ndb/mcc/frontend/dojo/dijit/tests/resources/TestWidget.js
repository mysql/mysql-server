define([
	"dojo/_base/declare",
	"dijit/_WidgetBase",
	"dijit/_TemplatedMixin"
], function(declare, _WidgetBase, _TemplatedMixin){
	return declare([_WidgetBase, _TemplatedMixin], {
		templateString: '<div data-dojo-attach-point="fooNode">TestWidget</div>',
		foo: "bar"
	});
});