//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/widget/Calendar"],function(_1,_2,_3){
_2.provide("dojox.widget.CalendarViews");
_2.experimental("dojox.widget.CalendarViews");
_2.require("dojox.widget.Calendar");
_2.declare("dojox.widget._CalendarMonth",null,{constructor:function(){
this._addView(_3.widget._CalendarMonthView);
}});
_2.declare("dojox.widget._CalendarMonthView",[_3.widget._CalendarView,_1._Templated],{templateString:_2.cache("dojox.widget","Calendar/CalendarMonth.html","<div class=\"dojoxCalendarMonthLabels\" style=\"left: 0px;\"  \n\tdojoAttachPoint=\"monthContainer\" dojoAttachEvent=\"onclick: onClick\">\n    <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n        <tbody>\n            <tr class=\"dojoxCalendarMonthGroupTemplate\">\n                <td class=\"dojoxCalendarMonthTemplate\">\n                    <div class=\"dojoxCalendarMonthLabel\"></div>\n                </td>\n             </tr>\n        </tbody>\n    </table>\n</div>\n"),datePart:"year",headerClass:"dojoxCalendarMonthHeader",postCreate:function(){
this.cloneClass(".dojoxCalendarMonthTemplate",3);
this.cloneClass(".dojoxCalendarMonthGroupTemplate",2);
this._populateMonths();
this.addFx(".dojoxCalendarMonthLabel",this.domNode);
},_setValueAttr:function(_4){
this.header.innerHTML=_4.getFullYear();
},_getMonthNames:_3.widget._CalendarMonthYearView.prototype._getMonthNames,_populateMonths:_3.widget._CalendarMonthYearView.prototype._populateMonths,onClick:function(_5){
if(!_2.hasClass(_5.target,"dojoxCalendarMonthLabel")){
_2.stopEvent(_5);
return;
}
var _6=_5.target.parentNode;
var _7=_6.cellIndex+(_6.parentNode.rowIndex*4);
var _8=this.get("value");
_8.setMonth(_7);
_8.setMonth(_7);
this.onValueSelected(_8,_7);
}});
_2.declare("dojox.widget._CalendarYear",null,{parent:null,constructor:function(){
this._addView(_3.widget._CalendarYearView);
}});
_2.declare("dojox.widget._CalendarYearView",[_3.widget._CalendarView,_1._Templated],{templateString:_2.cache("dojox.widget","Calendar/CalendarYear.html","<div class=\"dojoxCalendarYearLabels\" style=\"left: 0px;\" dojoAttachPoint=\"yearContainer\">\n    <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\" dojoAttachEvent=\"onclick: onClick\">\n        <tbody>\n            <tr class=\"dojoxCalendarYearGroupTemplate\">\n                <td class=\"dojoxCalendarNextMonth dojoxCalendarYearTemplate\">\n                    <div class=\"dojoxCalendarYearLabel\">\n                    </div>\n                </td>\n            </tr>\n        </tbody>\n    </table>\n</div>\n"),displayedYears:6,postCreate:function(){
this.cloneClass(".dojoxCalendarYearTemplate",3);
this.cloneClass(".dojoxCalendarYearGroupTemplate",2);
this._populateYears();
this.addFx(".dojoxCalendarYearLabel",this.domNode);
},_setValueAttr:function(_9){
this._populateYears(_9.getFullYear());
},_populateYears:_3.widget._CalendarMonthYearView.prototype._populateYears,adjustDate:function(_a,_b){
return _2.date.add(_a,"year",_b*12);
},onClick:function(_c){
if(!_2.hasClass(_c.target,"dojoxCalendarYearLabel")){
_2.stopEvent(_c);
return;
}
var _d=Number(_c.target.innerHTML);
var _e=this.get("value");
_e.setYear(_d);
this.onValueSelected(_e,_d);
}});
_2.declare("dojox.widget.Calendar3Pane",[_3.widget._CalendarBase,_3.widget._CalendarDay,_3.widget._CalendarMonth,_3.widget._CalendarYear],{});
_2.declare("dojox.widget.MonthlyCalendar",[_3.widget._CalendarBase,_3.widget._CalendarMonth],{_makeDate:function(_f){
var now=new Date();
now.setMonth(_f);
return now;
}});
_2.declare("dojox.widget.YearlyCalendar",[_3.widget._CalendarBase,_3.widget._CalendarYear],{_makeDate:function(_10){
var now=new Date();
now.setFullYear(_10);
return now;
}});
});
