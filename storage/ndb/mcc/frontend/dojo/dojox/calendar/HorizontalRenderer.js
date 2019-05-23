//>>built
require({cache:{"url:dojox/calendar/templates/HorizontalRenderer.html":"<div class=\"dojoxCalendarEvent dojoxCalendarHorizontal\" onselectstart=\"return false;\">\n\t<div class=\"bg\" ></div>\n\t<div style=\"position:absolute;left:2px;bottom:2px\" data-dojo-attach-point=\"beforeIcon\" class=\"beforeIcon\">◄</div>\t\n\t<div data-dojo-attach-point=\"labelContainer\" class=\"labels\">\t\t\n\t\t<span data-dojo-attach-point=\"startTimeLabel\" class=\"startTime\"></span>\n\t\t<span data-dojo-attach-point=\"summaryLabel\" class=\"summary\"></span>\n\t\t<span  data-dojo-attach-point=\"endTimeLabel\" class=\"endTime\"></span>\n\t</div>\n\t<div style=\"position:absolute;right:2px;bottom:2px\" data-dojo-attach-point=\"afterIcon\" class=\"afterIcon\">►</div>\n\t<div data-dojo-attach-point=\"moveHandle\" class=\"handle moveHandle\" ></div>\n\t<div data-dojo-attach-point=\"resizeStartHandle\" class=\"handle resizeStartHandle\"></div>\n\t<div data-dojo-attach-point=\"resizeEndHandle\" class=\"handle resizeEndHandle\" ></div>\t\n</div>\n"}});
define("dojox/calendar/HorizontalRenderer",["dojo/_base/declare","dojo/dom-style","dijit/_WidgetBase","dijit/_TemplatedMixin","dojox/calendar/_RendererMixin","dojo/text!./templates/HorizontalRenderer.html"],function(_1,_2,_3,_4,_5,_6){
return _1("dojox.calendar.HorizontalRenderer",[_3,_4,_5],{templateString:_6,_orientation:"horizontal",visibilityLimits:{resizeStartHandle:50,resizeEndHandle:-1,summaryLabel:15,startTimeLabel:32,endTimeLabel:30},_displayValueMap:{"beforeIcon":"inline","afterIcon":"inline"},_displayValue:"inline",arrowPadding:12,_isElementVisible:function(_7,_8,_9,_a){
var d;
var _b=this.isLeftToRight();
if(_7=="startTimeLabel"){
if(this.labelContainer&&(_b&&_9||!_b&&_8)){
_2.set(this.labelContainer,"marginRight",this.arrowPadding+"px");
}else{
_2.set(this.labelContainer,"marginRight",0);
}
if(this.labelContainer&&(!_b&&_9||_b&&_8)){
_2.set(this.labelContainer,"marginLeft",this.arrowPadding+"px");
}else{
_2.set(this.labelContainer,"marginLeft",0);
}
}
switch(_7){
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
},getDisplayValue:function(_c){
var _d=this._displayValueMap[_c];
if(_d){
return _d;
}
return this._displayValue;
},postCreate:function(){
this.inherited(arguments);
this._applyAttributes();
}});
});
