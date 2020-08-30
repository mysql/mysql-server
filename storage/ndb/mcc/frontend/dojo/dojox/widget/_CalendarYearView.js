//>>built
require({cache:{"url:dojox/widget/Calendar/CalendarYear.html":"<div class=\"dojoxCalendarYearLabels\" style=\"left: 0px;\" dojoAttachPoint=\"yearContainer\">\n    <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\" dojoAttachEvent=\"onclick: onClick\">\n        <tbody>\n            <tr class=\"dojoxCalendarYearGroupTemplate\">\n                <td class=\"dojoxCalendarNextMonth dojoxCalendarYearTemplate\">\n                    <div class=\"dojoxCalendarYearLabel\">\n                    </div>\n                </td>\n            </tr>\n        </tbody>\n    </table>\n</div>\n"}});
define("dojox/widget/_CalendarYearView",["dojo/_base/declare","./_CalendarView","dijit/_TemplatedMixin","dojo/date","dojo/dom-class","dojo/_base/event","dojo/text!./Calendar/CalendarYear.html","./_CalendarMonthYearView"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _1("dojox.widget._CalendarYearView",[_2,_3],{templateString:_7,displayedYears:6,postCreate:function(){
this.cloneClass(".dojoxCalendarYearTemplate",3);
this.cloneClass(".dojoxCalendarYearGroupTemplate",2);
this._populateYears();
this.addFx(".dojoxCalendarYearLabel",this.domNode);
},_setValueAttr:function(_9){
this._populateYears(_9.getFullYear());
},_populateYears:_8.prototype._populateYears,adjustDate:function(_a,_b){
return _4.add(_a,"year",_b*12);
},onClick:function(_c){
if(!_5.contains(_c.target,"dojoxCalendarYearLabel")){
_6.stop(_c);
return;
}
var _d=Number(_c.target.innerHTML);
var _e=this.get("value");
_e.setYear(_d);
this.onValueSelected(_e,_d);
}});
});
