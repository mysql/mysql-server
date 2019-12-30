//>>built
require({cache:{"url:dojox/calendar/templates/LabelRenderer.html":"<div class=\"dojoxCalendarEvent dojoxCalendarLabel\" onselectstart=\"return false;\">\t\n\t<div class=\"labels\">\n\t\t<span data-dojo-attach-point=\"startTimeLabel\" class=\"startTime\"></span>\n\t\t<span data-dojo-attach-point=\"summaryLabel\" class=\"summary\"></span>\n\t\t<span data-dojo-attach-point=\"endTimeLabel\" class=\"endTime\"></span>\n\t</div>\t\n\t<div data-dojo-attach-point=\"moveHandle\" class=\"handle moveHandle\" ></div>\n</div>\n"}});
define("dojox/calendar/LabelRenderer",["dojo/_base/declare","dijit/_WidgetBase","dijit/_TemplatedMixin","dojox/calendar/_RendererMixin","dojo/text!./templates/LabelRenderer.html"],function(_1,_2,_3,_4,_5){
return _1("dojox.calendar.LabelRenderer",[_2,_3,_4],{templateString:_5,_orientation:"horizontal",resizeEnabled:false,visibilityLimits:{resizeStartHandle:50,resizeEndHandle:-1,summaryLabel:15,startTimeLabel:45,endTimeLabel:30},_isElementVisible:function(_6,_7,_8,_9){
switch(_6){
case "startTimeLabel":
var d=this.item.startTime;
if(this.item.isAllDay||d.getHours()==0&&d.getMinutes()==0&&d.getSeconds()==0&&d.getMilliseconds()==0){
return false;
}
break;
}
return this.inherited(arguments);
},_displayValue:"inline",postCreate:function(){
this.inherited(arguments);
this._applyAttributes();
}});
});
