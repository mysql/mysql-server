define(["dojo/_base/declare", "dijit/_WidgetBase", "dijit/_TemplatedMixin",
	"./_RendererMixin", "dojo/text!./templates/LabelRenderer.html"],

	function(declare, _WidgetBase, _TemplatedMixin, _RendererMixin, template){

	return declare("dojox.calendar.LabelRenderer", [_WidgetBase, _TemplatedMixin, _RendererMixin], {

		// summary:
		//		The default item label renderer.

		templateString: template,

		_orientation: "horizontal",

		resizeEnabled: false,

		visibilityLimits: {
			resizeStartHandle: 50,
			resizeEndHandle: -1,
			summaryLabel: 15,
			startTimeLabel: 45,
			endTimeLabel: 30
		},

		_isElementVisible: function(elt, startHidden, endHidden, size){
			switch(elt){
				case "startTimeLabel":
					// hide hour part of all day events on subsequent days
					if(this.item.allDay && this.item.range[0].getTime() !== this.item.startTime.getTime()){
						return false;
					}
					break;
			}
			return this.inherited(arguments);
		},

		_displayValue: "inline",

		postCreate: function() {
			this.inherited(arguments);
			this._applyAttributes();
		}
	});
});
