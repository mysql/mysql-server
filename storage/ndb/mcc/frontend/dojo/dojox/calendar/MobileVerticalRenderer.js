//>>built
require({cache:{"url:dojox/calendar/templates/MobileVerticalRenderer.html":"<div class=\"dojoxCalendarEvent dojoxCalendarVertical\">\n\t<div class=\"bg\"></div>\t\n\t<div data-dojo-attach-point=\"resizeStartHandle\" class=\"resizeStartHandle resizeHandle\"><div></div></div>\n\t<dl style=\"width:100%;\">\t\t\t\t\n\t\t<dd data-dojo-attach-point=\"beforeIcon\" class=\"beforeIcon\">▲</dd>\n\t\t<dd data-dojo-attach-point=\"startTimeLabel\" class=\"startTime\"></dd>\n\t\t<dd data-dojo-attach-point=\"summaryLabel\" class=\"summary\"></dd>\n\t</dl>\n\t<span data-dojo-attach-point=\"afterIcon\" class=\"afterIcon\">▼</span>\t\n\t<div data-dojo-attach-point=\"resizeEndHandle\" class=\"resizeEndHandle resizeHandle\"><div></div></div>\n\t<span data-dojo-attach-point=\"endTimeLabel\" class=\"endTime\"></span>\n</div>\n"}});
define("dojox/calendar/MobileVerticalRenderer",["dojo/_base/declare","dijit/_WidgetBase","dijit/_TemplatedMixin","dojox/calendar/_RendererMixin","dojo/text!./templates/MobileVerticalRenderer.html"],function(_1,_2,_3,_4,_5){
return _1("dojox.calendar.MobileVerticalRenderer",[_2,_3,_4],{templateString:_5,mobile:true,visibilityLimits:{resizeStartHandle:75,resizeEndHandle:-1,summaryLabel:55,startTimeLabel:75,endTimeLabel:20},postCreate:function(){
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
