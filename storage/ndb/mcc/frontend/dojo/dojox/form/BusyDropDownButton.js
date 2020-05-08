define([
	"./_BusyButtonMixin",
	"dijit/form/DropDownButton",
	"dojo/_base/declare"
], function(_BusyButtonMixin, DropDownButton, declare){
return declare("dojox.form.BusyDropDownButton", [DropDownButton, _BusyButtonMixin], {});
});
