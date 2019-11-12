require({cache:{
'url:dojox/calendar/templates/MobileHorizontalRenderer.html':"<div class=\"dojoxCalendarEvent dojoxCalendarHorizontal\" onselectstart=\"return false;\">\n\t<div class=\"bg\" ></div>\n\t<div style=\"position:absolute;left:2px;bottom:2px\"><span data-dojo-attach-point=\"beforeIcon\" class=\"beforeIcon\">◄</span></div>\t\n\t<div data-dojo-attach-point=\"labelContainer\" class=\"labels\">\t\t\n\t\t<span data-dojo-attach-point=\"startTimeLabel\" class=\"startTime\"></span>\n\t\t<span data-dojo-attach-point=\"summaryLabel\" class=\"summary\"></span>\n\t\t<span  data-dojo-attach-point=\"endTimeLabel\" class=\"endTime\"></span>\n\t</div>\n\t<div style=\"position:absolute;right:2px;bottom:2px\"><span data-dojo-attach-point=\"afterIcon\" class=\"afterIcon\">►</span></div>\n\t<div data-dojo-attach-point=\"moveHandle\" class=\"moveHandle\" ></div>\t\n\t<div data-dojo-attach-point=\"resizeStartHandle\" class=\"resizeHandle resizeStartHandle\"><div></div></div>\t\n\t<div data-dojo-attach-point=\"resizeEndHandle\" class=\"resizeHandle resizeEndHandle\"><div></div></div>\t\n</div>\n"}});
define("dojox/calendar/MobileHorizontalRenderer", [
"dojo/_base/declare", 
"dojo/dom-style", 
"dijit/_WidgetBase", 
"dijit/_TemplatedMixin",
"dojox/calendar/_RendererMixin", 
"dojo/text!./templates/MobileHorizontalRenderer.html"],
	 
function(
declare, 
domStyle, 
_WidgetBase, 
_TemplatedMixin, 
_RendererMixin, 
template){
	
	return declare("dojox.calendar.MobileHorizontalRenderer", [_WidgetBase, _TemplatedMixin, _RendererMixin], {
		
		// summary:
		//		The mobile specific item horizontal renderer.
		
		templateString: template,
		
		_orientation: "horizontal",
		
		mobile: true,
		
		visibilityLimits: {
			resizeStartHandle: 50,
			resizeEndHandle: -1,
			summaryLabel: 15,
			startTimeLabel: 32,
			endTimeLabel: 30
		},
		
		_displayValue: "inline",
		
		// arrowPadding: Integer
		//		The padding size in pixels to apply to the label container on left and/or right side, to show the arrows correctly.
		arrowPadding: 12, 
		
		_isElementVisible: function(elt, startHidden, endHidden, size){
			var d;
			var ltr = this.isLeftToRight();
			
			if(elt == "startTimeLabel"){
				if(this.labelContainer && (ltr && endHidden || !ltr && startHidden)){
					domStyle.set(this.labelContainer, "marginRight", this.arrowPadding+"px");
				}else{
					domStyle.set(this.labelContainer, "marginRight", 0);
				}
				if(this.labelContainer && (!ltr && endHidden || ltr && startHidden)){
					domStyle.set(this.labelContainer, "marginLeft", this.arrowPadding+"px");
				}else{
					domStyle.set(this.labelContainer, "marginLeft", 0);
				}
			}
			
			switch(elt){
				case "startTimeLabel":
					d = this.item.startTime;
					if(this.item.allDay || this.owner.isStartOfDay(d)){
						return false;
					}
					break;
				case "endTimeLabel":
					d = this.item.endTime;
					if(this.item.allDay || this.owner.isStartOfDay(d)){
						return false;
					}
					break;
			}
			return this.inherited(arguments);
		},
		
		postCreate: function() {
			this.inherited(arguments);
			this._applyAttributes();
		}
	});
});
