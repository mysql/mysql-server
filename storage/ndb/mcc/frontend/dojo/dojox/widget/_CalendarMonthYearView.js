//>>built
require({cache:{"url:dojox/widget/Calendar/CalendarMonthYear.html":"<div class=\"dojoxCal-MY-labels\" style=\"left: 0px;\"\t\n\tdojoAttachPoint=\"myContainer\" dojoAttachEvent=\"onclick: onClick\">\n\t\t<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n\t\t\t\t<tbody>\n\t\t\t\t\t\t<tr class=\"dojoxCal-MY-G-Template\">\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-M-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarMonthLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-M-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarMonthLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-Y-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarYearLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-Y-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarYearLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t </tr>\n\t\t\t\t\t\t <tr class=\"dojoxCal-MY-btns\">\n\t\t\t\t\t\t \t <td class=\"dojoxCal-MY-btns\" colspan=\"4\">\n\t\t\t\t\t\t \t\t <span class=\"dijitReset dijitInline dijitButtonNode ok-btn\" dojoAttachEvent=\"onclick: onOk\" dojoAttachPoint=\"okBtn\">\n\t\t\t\t\t\t \t \t \t <button\tclass=\"dijitReset dijitStretch dijitButtonContents\">OK</button>\n\t\t\t\t\t\t\t\t </span>\n\t\t\t\t\t\t\t\t <span class=\"dijitReset dijitInline dijitButtonNode cancel-btn\" dojoAttachEvent=\"onclick: onCancel\" dojoAttachPoint=\"cancelBtn\">\n\t\t\t\t\t\t \t \t\t <button\tclass=\"dijitReset dijitStretch dijitButtonContents\">Cancel</button>\n\t\t\t\t\t\t\t\t </span>\n\t\t\t\t\t\t \t </td>\n\t\t\t\t\t\t </tr>\n\t\t\t\t</tbody>\n\t\t</table>\n</div>\n"}});
define("dojox/widget/_CalendarMonthYearView",["dojo/_base/declare","./_CalendarView","dijit/_TemplatedMixin","dojo/query","dojo/dom-class","dojo/_base/connect","dojo/_base/event","dojo/_base/lang","dojo/date/locale","dojo/text!./Calendar/CalendarMonthYear.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
return _1("dojox.widget._CalendarMonthYearView",[_2,_3],{templateString:_a,datePart:"year",displayedYears:10,useHeader:false,postCreate:function(){
this.cloneClass(".dojoxCal-MY-G-Template",5,".dojoxCal-MY-btns");
this.monthContainer=this.yearContainer=this.myContainer;
var _b="dojoxCalendarYearLabel";
var _c="dojoxCalendarDecrease";
var _d="dojoxCalendarIncrease";
_4("."+_b,this.myContainer).forEach(function(_e,_f){
var _10=_d;
switch(_f){
case 0:
_10=_c;
case 1:
_5.remove(_e,_b);
_5.add(_e,_10);
break;
}
});
this._decBtn=_4("."+_c,this.myContainer)[0];
this._incBtn=_4("."+_d,this.myContainer)[0];
_4(".dojoxCal-MY-M-Template",this.domNode).filter(function(_11){
return _11.cellIndex==1;
}).addClass("dojoxCal-MY-M-last");
_6.connect(this,"onBeforeDisplay",_8.hitch(this,function(){
this._cachedDate=new Date(this.get("value").getTime());
this._populateYears(this._cachedDate.getFullYear());
this._populateMonths();
this._updateSelectedMonth();
this._updateSelectedYear();
}));
_6.connect(this,"_populateYears",_8.hitch(this,function(){
this._updateSelectedYear();
}));
_6.connect(this,"_populateMonths",_8.hitch(this,function(){
this._updateSelectedMonth();
}));
this._cachedDate=this.get("value");
this._populateYears();
this._populateMonths();
this.addFx(".dojoxCalendarMonthLabel,.dojoxCalendarYearLabel ",this.myContainer);
},_setValueAttr:function(_12){
if(_12&&_12.getFullYear()){
this._populateYears(_12.getFullYear());
}
},getHeader:function(){
return null;
},_getMonthNames:function(_13){
this._monthNames=this._monthNames||_9.getNames("months",_13,"standAlone",this.getLang());
return this._monthNames;
},_populateMonths:function(){
var _14=this._getMonthNames("abbr");
_4(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_8.hitch(this,function(_15,cnt){
this._setText(_15,_14[cnt]);
}));
var _16=this.get("constraints");
if(_16){
var _17=new Date();
_17.setFullYear(this._year);
var min=-1,max=12;
if(_16.min){
var _18=_16.min.getFullYear();
if(_18>this._year){
min=12;
}else{
if(_18==this._year){
min=_16.min.getMonth();
}
}
}
if(_16.max){
var _19=_16.max.getFullYear();
if(_19<this._year){
max=-1;
}else{
if(_19==this._year){
max=_16.max.getMonth();
}
}
}
_4(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_8.hitch(this,function(_1a,cnt){
_5[(cnt<min||cnt>max)?"add":"remove"](_1a,"dijitCalendarDisabledDate");
}));
}
var h=this.getHeader();
if(h){
this._setText(this.getHeader(),this.get("value").getFullYear());
}
},_populateYears:function(_1b){
var _1c=this.get("constraints");
var _1d=_1b||this.get("value").getFullYear();
var _1e=_1d-Math.floor(this.displayedYears/2);
var min=_1c&&_1c.min?_1c.min.getFullYear():_1e-10000;
_1e=Math.max(min,_1e);
this._displayedYear=_1d;
var _1f=_4(".dojoxCalendarYearLabel",this.yearContainer);
var max=_1c&&_1c.max?_1c.max.getFullYear()-_1e:_1f.length;
var _20="dijitCalendarDisabledDate";
_1f.forEach(_8.hitch(this,function(_21,cnt){
if(cnt<=max){
this._setText(_21,_1e+cnt);
}
_5.toggle(_21,_20,cnt>max);
}));
if(this._incBtn){
_5.toggle(this._incBtn,_20,max<_1f.length);
}
if(this._decBtn){
_5.toggle(this._decBtn,_20,min>=_1e);
}
var h=this.getHeader();
if(h){
this._setText(this.getHeader(),_1e+" - "+(_1e+11));
}
},_updateSelectedYear:function(){
this._year=String((this._cachedDate||this.get("value")).getFullYear());
this._updateSelectedNode(".dojoxCalendarYearLabel",_8.hitch(this,function(_22){
return this._year!==null&&_22.innerHTML==this._year;
}));
},_updateSelectedMonth:function(){
var _23=(this._cachedDate||this.get("value")).getMonth();
this._month=_23;
this._updateSelectedNode(".dojoxCalendarMonthLabel",function(_24,idx){
return idx==_23;
});
},_updateSelectedNode:function(_25,_26){
var sel="dijitCalendarSelectedDate";
_4(_25,this.domNode).forEach(function(_27,idx,_28){
_5.toggle(_27.parentNode,sel,_26(_27,idx,_28));
});
var _29=_4(".dojoxCal-MY-M-Template div",this.myContainer).filter(function(_2a){
return _5.contains(_2a.parentNode,sel);
})[0];
if(!_29){
return;
}
var _2b=_5.contains(_29,"dijitCalendarDisabledDate");
_5.toggle(this.okBtn,"dijitDisabled",_2b);
},onClick:function(evt){
var _2c;
function hc(c){
return _5.contains(evt.target,c);
};
if(hc("dijitCalendarDisabledDate")){
_7.stop(evt);
return false;
}
if(hc("dojoxCalendarMonthLabel")){
_2c="dojoxCal-MY-M-Template";
this._month=evt.target.parentNode.cellIndex+(evt.target.parentNode.parentNode.rowIndex*2);
this._cachedDate.setMonth(this._month);
this._updateSelectedMonth();
}else{
if(hc("dojoxCalendarYearLabel")){
_2c="dojoxCal-MY-Y-Template";
this._year=Number(evt.target.innerHTML);
this._cachedDate.setYear(this._year);
this._populateMonths();
this._updateSelectedYear();
}else{
if(hc("dojoxCalendarDecrease")){
this._populateYears(this._displayedYear-10);
return true;
}else{
if(hc("dojoxCalendarIncrease")){
this._populateYears(this._displayedYear+10);
return true;
}else{
return true;
}
}
}
}
_7.stop(evt);
return false;
},onOk:function(evt){
_7.stop(evt);
if(_5.contains(this.okBtn,"dijitDisabled")){
return false;
}
this.onValueSelected(this._cachedDate);
return false;
},onCancel:function(evt){
_7.stop(evt);
this.onValueSelected(this.get("value"));
return false;
}});
});
