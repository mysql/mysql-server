define("dojox/form/BusyComboButton", [
	"./_BusyButtonMixin",
	"dijit/form/ComboButton",
	"dojo/_base/declare"
], function(_BusyButtonMixin, ComboButton, declare){
return declare("dojox.form.BusyComboButton", [ComboButton, _BusyButtonMixin], {});
});

