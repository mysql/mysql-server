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
var _14,_15=this._getMonthNames("abbr"),_16=this.get("value").getFullYear(),_17=_15[this.get("value").getMonth()],_18=this.get("displayedYear");
_4(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_8.hitch(this,function(_19,cnt){
this._setText(_19,_15[cnt]);
_14=((_17===_15[cnt])&&(_16===_18));
_5.toggle(_19.parentNode,["dijitCalendarSelectedDate","dijitCalendarCurrentDate"],_14);
}));
var _1a=this.get("constraints");
if(_1a){
var _1b=new Date();
_1b.setFullYear(this._year);
var min=-1,max=12;
if(_1a.min){
var _1c=_1a.min.getFullYear();
if(_1c>this._year){
min=12;
}else{
if(_1c==this._year){
min=_1a.min.getMonth();
}
}
}
if(_1a.max){
var _1d=_1a.max.getFullYear();
if(_1d<this._year){
max=-1;
}else{
if(_1d==this._year){
max=_1a.max.getMonth();
}
}
}
_4(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_8.hitch(this,function(_1e,cnt){
_5[(cnt<min||cnt>max)?"add":"remove"](_1e,"dijitCalendarDisabledDate");
}));
}
},_populateYears:function(_1f){
var _20,_21=this.get("constraints"),_22=this.get("value").getFullYear(),_23=_1f||_22,_24=_23-Math.floor(this.displayedYears/2),min=_21&&_21.min?_21.min.getFullYear():_24-10000;
this._displayedYear=_23;
var _25=_4(".dojoxCalendarYearLabel",this.yearContainer);
var max=_21&&_21.max?_21.max.getFullYear()-_24:_25.length;
var _26="dijitCalendarDisabledDate";
var _27;
_25.forEach(_8.hitch(this,function(_28,cnt){
if(cnt<=max){
this._setText(_28,_24+cnt);
}
_27=(_24+cnt)==_22;
_5.toggle(_28.parentNode,["dijitCalendarSelectedDate","dijitCalendarCurrentDate"],_27);
_5.toggle(_28,_26,cnt>max);
_20=(_24+cnt)==_22;
_5.toggle(_28.parentNode,["dijitCalendarSelectedDate","dijitCalendarCurrentDate"],_20);
}));
if(this._incBtn){
_5.toggle(this._incBtn,_26,max<_25.length);
}
if(this._decBtn){
_5.toggle(this._decBtn,_26,min>=_24);
}
var h=this.getHeader();
if(h){
this._setText(this.getHeader(),_24+" - "+(_24+11));
}
},_updateSelectedYear:function(){
this._year=String((this._cachedDate||this.get("value")).getFullYear());
this._updateSelectedNode(".dojoxCalendarYearLabel",_8.hitch(this,function(_29){
return this._year!==null&&_29.innerHTML==this._year;
}));
},_updateSelectedMonth:function(){
var _2a=(this._cachedDate||this.get("value")).getMonth();
this._month=_2a;
this._updateSelectedNode(".dojoxCalendarMonthLabel",function(_2b,idx){
return idx==_2a;
});
},_updateSelectedNode:function(_2c,_2d){
var sel="dijitCalendarSelectedDate";
_4(_2c,this.domNode).forEach(function(_2e,idx,_2f){
_5.toggle(_2e.parentNode,sel,_2d(_2e,idx,_2f));
});
var _30=_4(".dojoxCal-MY-M-Template div",this.myContainer).filter(function(_31){
return _5.contains(_31.parentNode,sel);
})[0];
if(!_30){
return;
}
var _32=_5.contains(_30,"dijitCalendarDisabledDate");
_5.toggle(this.okBtn,"dijitDisabled",_32);
},onClick:function(evt){
var _33;
function hc(c){
return _5.contains(evt.target,c);
};
if(hc("dijitCalendarDisabledDate")){
_7.stop(evt);
return false;
}
if(hc("dojoxCalendarMonthLabel")){
_33="dojoxCal-MY-M-Template";
this._month=evt.target.parentNode.cellIndex+(evt.target.parentNode.parentNode.rowIndex*2);
this._cachedDate.setMonth(this._month);
this._updateSelectedMonth();
}else{
if(hc("dojoxCalendarYearLabel")){
_33="dojoxCal-MY-Y-Template";
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
