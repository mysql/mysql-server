//>>built
require({cache:{"url:dojox/calendar/templates/VerticalRenderer.html":"<div class=\"dojoxCalendarEvent dojoxCalendarVertical\" onselectstart=\"return false;\">\n\t<div class=\"bg\"></div>\n\t<dl style=\"width:100%;\">\n\t\t<dd data-dojo-attach-point=\"beforeIcon\" class=\"beforeIcon\">▲</dd>\n\t\t<dd data-dojo-attach-point=\"startTimeLabel\" class=\"startTime\"></dd>\n\t\t<dd data-dojo-attach-point=\"summaryLabel\" class=\"summary\"></dd>\n\t</dl>\t\n\t<span data-dojo-attach-point=\"afterIcon\" class=\"afterIcon\">▼</span>\n\t<div data-dojo-attach-point=\"moveHandle\" class=\"handle moveHandle\" ></div>\n\t<div data-dojo-attach-point=\"resizeStartHandle\" class=\"handle resizeStartHandle\"></div>\n\t<div data-dojo-attach-point=\"resizeEndHandle\" class=\"handle resizeEndHandle\" ></div>\n\t<div data-dojo-attach-point=\"endTimeLabel\" class=\"endTime\"></div>\n</div>\n"}});
define("dojox/calendar/VerticalRenderer",["dojo/_base/declare","dijit/_WidgetBase","dijit/_TemplatedMixin","dojox/calendar/_RendererMixin","dojo/text!./templates/VerticalRenderer.html"],function(_1,_2,_3,_4,_5){
return _1("dojox.calendar.VerticalRenderer",[_2,_3,_4],{templateString:_5,postCreate:function(){
this.inherited(arguments);
this._applyAttributes();
},_isElementVisible:function(_6,_7,_8,_9){
var d;
switch(_6){
case "startTimeLabel":
d=this.item.startTime;
if(this.item.allDay||this.owner.isStartOfDay(d)){
return false;
}
break;
case "endTimeLabel":
d=this.item.endTime;
if(this.item.allDay||this.owner.isStartOfDay(d)){
return false;
}
break;
}
return this.inherited(arguments);
}});
});
