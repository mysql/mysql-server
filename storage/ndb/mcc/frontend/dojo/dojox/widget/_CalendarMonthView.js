//>>built
require({cache:{"url:dojox/widget/Calendar/CalendarMonth.html":"<div class=\"dojoxCalendarMonthLabels\" style=\"left: 0px;\"  \n\tdojoAttachPoint=\"monthContainer\" dojoAttachEvent=\"onclick: onClick\">\n    <table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n        <tbody>\n            <tr class=\"dojoxCalendarMonthGroupTemplate\">\n                <td class=\"dojoxCalendarMonthTemplate\">\n                    <div class=\"dojoxCalendarMonthLabel\"></div>\n                </td>\n             </tr>\n        </tbody>\n    </table>\n</div>\n"}});
define("dojox/widget/_CalendarMonthView",["dojo/_base/declare","./_CalendarView","dijit/_TemplatedMixin","./_CalendarMonthYearView","dojo/dom-class","dojo/_base/event","dojo/text!./Calendar/CalendarMonth.html"],function(_1,_2,_3,_4,_5,_6,_7){
return _1("dojox.widget._CalendarMonthView",[_2,_3],{templateString:_7,datePart:"year",headerClass:"dojoxCalendarMonthHeader",displayedYear:"",postCreate:function(){
this.cloneClass(".dojoxCalendarMonthTemplate",3);
this.cloneClass(".dojoxCalendarMonthGroupTemplate",2);
this._populateMonths();
this.addFx(".dojoxCalendarMonthLabel",this.domNode);
},_setValueAttr:function(_8){
var _9=this.header.innerHTML=_8.getFullYear();
this.set("displayedYear",_9);
this._populateMonths();
},_getMonthNames:_4.prototype._getMonthNames,_populateMonths:_4.prototype._populateMonths,onClick:function(_a){
if(!_5.contains(_a.target,"dojoxCalendarMonthLabel")){
_6.stop(_a);
return;
}
var _b=_a.target.parentNode;
var _c=_b.cellIndex+(_b.parentNode.rowIndex*4);
var _d=this.get("value");
_d.setMonth(_c);
_d.setMonth(_c);
_d.setYear(this.displayedYear);
this.onValueSelected(_d,_c);
}});
});
