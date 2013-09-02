//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_Widget,dijit/_Templated,dijit/_Container,dijit/typematic,dojo/date,dojo/date/locale"],function(_1,_2,_3){
_2.provide("dojox.widget.Calendar");
_2.experimental("dojox.widget.Calendar");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.require("dijit._Container");
_2.require("dijit.typematic");
_2.require("dojo.date");
_2.require("dojo.date.locale");
_2.declare("dojox.widget._CalendarBase",[_1._Widget,_1._Templated,_1._Container],{templateString:_2.cache("dojox.widget","Calendar/Calendar.html","<div class=\"dojoxCalendar\">\n    <div tabindex=\"0\" class=\"dojoxCalendarContainer\" style=\"visibility: visible;\" dojoAttachPoint=\"container\">\n\t\t<div style=\"display:none\">\n\t\t\t<div dojoAttachPoint=\"previousYearLabelNode\"></div>\n\t\t\t<div dojoAttachPoint=\"nextYearLabelNode\"></div>\n\t\t\t<div dojoAttachPoint=\"monthLabelSpacer\"></div>\n\t\t</div>\n        <div class=\"dojoxCalendarHeader\">\n            <div>\n                <div class=\"dojoxCalendarDecrease\" dojoAttachPoint=\"decrementMonth\"></div>\n            </div>\n            <div class=\"\">\n                <div class=\"dojoxCalendarIncrease\" dojoAttachPoint=\"incrementMonth\"></div>\n            </div>\n            <div class=\"dojoxCalendarTitle\" dojoAttachPoint=\"header\" dojoAttachEvent=\"onclick: onHeaderClick\">\n            </div>\n        </div>\n        <div class=\"dojoxCalendarBody\" dojoAttachPoint=\"containerNode\"></div>\n        <div class=\"\">\n            <div class=\"dojoxCalendarFooter\" dojoAttachPoint=\"footer\">                        \n            </div>\n        </div>\n    </div>\n</div>\n"),_views:null,useFx:true,widgetsInTemplate:true,value:new Date(),constraints:null,footerFormat:"medium",constructor:function(){
this._views=[];
this.value=new Date();
},postMixInProperties:function(){
var c=this.constraints;
if(c){
var _4=_2.date.stamp.fromISOString;
if(typeof c.min=="string"){
c.min=_4(c.min);
}
if(typeof c.max=="string"){
c.max=_4(c.max);
}
}
this.value=this.parseInitialValue(this.value);
},parseInitialValue:function(_5){
if(!_5||_5===-1){
return new Date();
}else{
if(_5.getFullYear){
return _5;
}else{
if(!isNaN(_5)){
if(typeof this.value=="string"){
_5=parseInt(_5);
}
_5=this._makeDate(_5);
}
}
}
return _5;
},_makeDate:function(_6){
return _6;
},postCreate:function(){
this.displayMonth=new Date(this.get("value"));
if(this._isInvalidDate(this.displayMonth)){
this.displayMonth=new Date();
}
var _7={parent:this,_getValueAttr:_2.hitch(this,function(){
return new Date(this._internalValue||this.value);
}),_getDisplayMonthAttr:_2.hitch(this,function(){
return new Date(this.displayMonth);
}),_getConstraintsAttr:_2.hitch(this,function(){
return this.constraints;
}),getLang:_2.hitch(this,function(){
return this.lang;
}),isDisabledDate:_2.hitch(this,this.isDisabledDate),getClassForDate:_2.hitch(this,this.getClassForDate),addFx:this.useFx?_2.hitch(this,this.addFx):function(){
}};
_2.forEach(this._views,function(_8){
var _9=new _8(_7,_2.create("div"));
this.addChild(_9);
var _a=_9.getHeader();
if(_a){
this.header.appendChild(_a);
_2.style(_a,"display","none");
}
_2.style(_9.domNode,"visibility","hidden");
_2.connect(_9,"onValueSelected",this,"_onDateSelected");
_9.set("value",this.get("value"));
},this);
if(this._views.length<2){
_2.style(this.header,"cursor","auto");
}
this.inherited(arguments);
this._children=this.getChildren();
this._currentChild=0;
var _b=new Date();
this.footer.innerHTML="Today: "+_2.date.locale.format(_b,{formatLength:this.footerFormat,selector:"date",locale:this.lang});
_2.connect(this.footer,"onclick",this,"goToToday");
var _c=this._children[0];
_2.style(_c.domNode,"top","0px");
_2.style(_c.domNode,"visibility","visible");
var _d=_c.getHeader();
if(_d){
_2.style(_c.getHeader(),"display","");
}
_2[_c.useHeader?"removeClass":"addClass"](this.container,"no-header");
_c.onDisplay();
var _e=this;
var _f=function(_10,_11,adj){
_1.typematic.addMouseListener(_e[_10],_e,function(_12){
if(_12>=0){
_e._adjustDisplay(_11,adj);
}
},0.8,500);
};
_f("incrementMonth","month",1);
_f("decrementMonth","month",-1);
this._updateTitleStyle();
},addFx:function(_13,_14){
},_isInvalidDate:function(_15){
return !_15||isNaN(_15)||typeof _15!="object"||_15.toString()==this._invalidDate;
},_setValueAttr:function(_16){
if(!_16){
_16=new Date();
}
if(!_16["getFullYear"]){
_16=_2.date.stamp.fromISOString(_16+"");
}
if(this._isInvalidDate(_16)){
return false;
}
if(!this.value||_2.date.compare(_16,this.value)){
_16=new Date(_16);
this.displayMonth=new Date(_16);
this._internalValue=_16;
if(!this.isDisabledDate(_16,this.lang)&&this._currentChild==0){
this.value=_16;
this.onChange(_16);
}
if(this._children&&this._children.length>0){
this._children[this._currentChild].set("value",this.value);
}
return true;
}
return false;
},isDisabledDate:function(_17,_18){
var c=this.constraints;
var _19=_2.date.compare;
return c&&(c.min&&(_19(c.min,_17,"date")>0)||(c.max&&_19(c.max,_17,"date")<0));
},onValueSelected:function(_1a){
},_onDateSelected:function(_1b,_1c,_1d){
this.displayMonth=_1b;
this.set("value",_1b);
if(!this._transitionVert(-1)){
if(!_1c&&_1c!==0){
_1c=this.get("value");
}
this.onValueSelected(_1c);
}
},onChange:function(_1e){
},onHeaderClick:function(e){
this._transitionVert(1);
},goToToday:function(){
this.set("value",new Date());
this.onValueSelected(this.get("value"));
},_transitionVert:function(_1f){
var _20=this._children[this._currentChild];
var _21=this._children[this._currentChild+_1f];
if(!_21){
return false;
}
_2.style(_21.domNode,"visibility","visible");
var _22=_2.style(this.containerNode,"height");
_21.set("value",this.displayMonth);
if(_20.header){
_2.style(_20.header,"display","none");
}
if(_21.header){
_2.style(_21.header,"display","");
}
_2.style(_21.domNode,"top",(_22*-1)+"px");
_2.style(_21.domNode,"visibility","visible");
this._currentChild+=_1f;
var _23=_22*_1f;
var _24=0;
_2.style(_21.domNode,"top",(_23*-1)+"px");
var _25=_2.animateProperty({node:_20.domNode,properties:{top:_23},onEnd:function(){
_2.style(_20.domNode,"visibility","hidden");
}});
var _26=_2.animateProperty({node:_21.domNode,properties:{top:_24},onEnd:function(){
_21.onDisplay();
}});
_2[_21.useHeader?"removeClass":"addClass"](this.container,"no-header");
_25.play();
_26.play();
_20.onBeforeUnDisplay();
_21.onBeforeDisplay();
this._updateTitleStyle();
return true;
},_updateTitleStyle:function(){
_2[this._currentChild<this._children.length-1?"addClass":"removeClass"](this.header,"navToPanel");
},_slideTable:function(_27,_28,_29){
var _2a=_27.domNode;
var _2b=_2a.cloneNode(true);
var _2c=_2.style(_2a,"width");
_2a.parentNode.appendChild(_2b);
_2.style(_2a,"left",(_2c*_28)+"px");
_29();
var _2d=_2.animateProperty({node:_2b,properties:{left:_2c*_28*-1},duration:500,onEnd:function(){
_2b.parentNode.removeChild(_2b);
}});
var _2e=_2.animateProperty({node:_2a,properties:{left:0},duration:500});
_2d.play();
_2e.play();
},_addView:function(_2f){
this._views.push(_2f);
},getClassForDate:function(_30,_31){
},_adjustDisplay:function(_32,_33,_34){
var _35=this._children[this._currentChild];
var _36=this.displayMonth=_35.adjustDate(this.displayMonth,_33);
this._slideTable(_35,_33,function(){
_35.set("value",_36);
});
}});
_2.declare("dojox.widget._CalendarView",_1._Widget,{headerClass:"",useHeader:true,cloneClass:function(_37,n,_38){
var _39=_2.query(_37,this.domNode)[0];
var i;
if(!_38){
for(i=0;i<n;i++){
_39.parentNode.appendChild(_39.cloneNode(true));
}
}else{
var _3a=_2.query(_37,this.domNode)[0];
for(i=0;i<n;i++){
_39.parentNode.insertBefore(_39.cloneNode(true),_3a);
}
}
},_setText:function(_3b,_3c){
if(_3b.innerHTML!=_3c){
_2.empty(_3b);
_3b.appendChild(_2.doc.createTextNode(_3c));
}
},getHeader:function(){
return this.header||(this.header=this.header=_2.create("span",{"class":this.headerClass}));
},onValueSelected:function(_3d){
},adjustDate:function(_3e,_3f){
return _2.date.add(_3e,this.datePart,_3f);
},onDisplay:function(){
},onBeforeDisplay:function(){
},onBeforeUnDisplay:function(){
}});
_2.declare("dojox.widget._CalendarDay",null,{parent:null,constructor:function(){
this._addView(_3.widget._CalendarDayView);
}});
_2.declare("dojox.widget._CalendarDayView",[_3.widget._CalendarView,_1._Templated],{templateString:_2.cache("dojox.widget","Calendar/CalendarDay.html","<div class=\"dijitCalendarDayLabels\" style=\"left: 0px;\" dojoAttachPoint=\"dayContainer\">\n\t<div dojoAttachPoint=\"header\">\n\t\t<div dojoAttachPoint=\"monthAndYearHeader\">\n\t\t\t<span dojoAttachPoint=\"monthLabelNode\" class=\"dojoxCalendarMonthLabelNode\"></span>\n\t\t\t<span dojoAttachPoint=\"headerComma\" class=\"dojoxCalendarComma\">,</span>\n\t\t\t<span dojoAttachPoint=\"yearLabelNode\" class=\"dojoxCalendarDayYearLabel\"></span>\n\t\t</div>\n\t</div>\n\t<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n\t\t<thead>\n\t\t\t<tr>\n\t\t\t\t<td class=\"dijitCalendarDayLabelTemplate\"><div class=\"dijitCalendarDayLabel\"></div></td>\n\t\t\t</tr>\n\t\t</thead>\n\t\t<tbody dojoAttachEvent=\"onclick: _onDayClick\">\n\t\t\t<tr class=\"dijitCalendarWeekTemplate\">\n\t\t\t\t<td class=\"dojoxCalendarNextMonth dijitCalendarDateTemplate\">\n\t\t\t\t\t<div class=\"dijitCalendarDateLabel\"></div>\n\t\t\t\t</td>\n\t\t\t</tr>\n\t\t</tbody>\n\t</table>\n</div>\n"),datePart:"month",dayWidth:"narrow",postCreate:function(){
this.cloneClass(".dijitCalendarDayLabelTemplate",6);
this.cloneClass(".dijitCalendarDateTemplate",6);
this.cloneClass(".dijitCalendarWeekTemplate",5);
var _40=_2.date.locale.getNames("days",this.dayWidth,"standAlone",this.getLang());
var _41=_2.cldr.supplemental.getFirstDayOfWeek(this.getLang());
_2.query(".dijitCalendarDayLabel",this.domNode).forEach(function(_42,i){
this._setText(_42,_40[(i+_41)%7]);
},this);
},onDisplay:function(){
if(!this._addedFx){
this._addedFx=true;
this.addFx(".dijitCalendarDateTemplate div",this.domNode);
}
},_onDayClick:function(e){
if(typeof (e.target._date)=="undefined"){
return;
}
var _43=new Date(this.get("displayMonth"));
var p=e.target.parentNode;
var c="dijitCalendar";
var d=_2.hasClass(p,c+"PreviousMonth")?-1:(_2.hasClass(p,c+"NextMonth")?1:0);
if(d){
_43=_2.date.add(_43,"month",d);
}
_43.setDate(e.target._date);
if(this.isDisabledDate(_43)){
_2.stopEvent(e);
return;
}
this.parent._onDateSelected(_43);
},_setValueAttr:function(_44){
this._populateDays();
},_populateDays:function(){
var _45=new Date(this.get("displayMonth"));
_45.setDate(1);
var _46=_45.getDay();
var _47=_2.date.getDaysInMonth(_45);
var _48=_2.date.getDaysInMonth(_2.date.add(_45,"month",-1));
var _49=new Date();
var _4a=this.get("value");
var _4b=_2.cldr.supplemental.getFirstDayOfWeek(this.getLang());
if(_4b>_46){
_4b-=7;
}
var _4c=_2.date.compare;
var _4d=".dijitCalendarDateTemplate";
var _4e="dijitCalendarSelectedDate";
var _4f=this._lastDate;
var _50=_4f==null||_4f.getMonth()!=_45.getMonth()||_4f.getFullYear()!=_45.getFullYear();
this._lastDate=_45;
if(!_50){
_2.query(_4d,this.domNode).removeClass(_4e).filter(function(_51){
return _51.className.indexOf("dijitCalendarCurrent")>-1&&_51._date==_4a.getDate();
}).addClass(_4e);
return;
}
_2.query(_4d,this.domNode).forEach(function(_52,i){
i+=_4b;
var _53=new Date(_45);
var _54,_55="dijitCalendar",adj=0;
if(i<_46){
_54=_48-_46+i+1;
adj=-1;
_55+="Previous";
}else{
if(i>=(_46+_47)){
_54=i-_46-_47+1;
adj=1;
_55+="Next";
}else{
_54=i-_46+1;
_55+="Current";
}
}
if(adj){
_53=_2.date.add(_53,"month",adj);
}
_53.setDate(_54);
if(!_4c(_53,_49,"date")){
_55="dijitCalendarCurrentDate "+_55;
}
if(!_4c(_53,_4a,"date")&&!_4c(_53,_4a,"month")&&!_4c(_53,_4a,"year")){
_55=_4e+" "+_55;
}
if(this.isDisabledDate(_53,this.getLang())){
_55=" dijitCalendarDisabledDate "+_55;
}
var _56=this.getClassForDate(_53,this.getLang());
if(_56){
_55=_56+" "+_55;
}
_52.className=_55+"Month dijitCalendarDateTemplate";
_52.dijitDateValue=_53.valueOf();
var _57=_2.query(".dijitCalendarDateLabel",_52)[0];
this._setText(_57,_53.getDate());
_57._date=_57.parentNode._date=_53.getDate();
},this);
var _58=_2.date.locale.getNames("months","wide","standAlone",this.getLang());
this._setText(this.monthLabelNode,_58[_45.getMonth()]);
this._setText(this.yearLabelNode,_45.getFullYear());
}});
_2.declare("dojox.widget._CalendarMonthYear",null,{constructor:function(){
this._addView(_3.widget._CalendarMonthYearView);
}});
_2.declare("dojox.widget._CalendarMonthYearView",[_3.widget._CalendarView,_1._Templated],{templateString:_2.cache("dojox.widget","Calendar/CalendarMonthYear.html","<div class=\"dojoxCal-MY-labels\" style=\"left: 0px;\"\t\n\tdojoAttachPoint=\"myContainer\" dojoAttachEvent=\"onclick: onClick\">\n\t\t<table cellspacing=\"0\" cellpadding=\"0\" border=\"0\" style=\"margin: auto;\">\n\t\t\t\t<tbody>\n\t\t\t\t\t\t<tr class=\"dojoxCal-MY-G-Template\">\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-M-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarMonthLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-M-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarMonthLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-Y-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarYearLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t\t\t<td class=\"dojoxCal-MY-Y-Template\">\n\t\t\t\t\t\t\t\t\t\t<div class=\"dojoxCalendarYearLabel\"></div>\n\t\t\t\t\t\t\t\t</td>\n\t\t\t\t\t\t </tr>\n\t\t\t\t\t\t <tr class=\"dojoxCal-MY-btns\">\n\t\t\t\t\t\t \t <td class=\"dojoxCal-MY-btns\" colspan=\"4\">\n\t\t\t\t\t\t \t\t <span class=\"dijitReset dijitInline dijitButtonNode ok-btn\" dojoAttachEvent=\"onclick: onOk\" dojoAttachPoint=\"okBtn\">\n\t\t\t\t\t\t \t \t \t <button\tclass=\"dijitReset dijitStretch dijitButtonContents\">OK</button>\n\t\t\t\t\t\t\t\t </span>\n\t\t\t\t\t\t\t\t <span class=\"dijitReset dijitInline dijitButtonNode cancel-btn\" dojoAttachEvent=\"onclick: onCancel\" dojoAttachPoint=\"cancelBtn\">\n\t\t\t\t\t\t \t \t\t <button\tclass=\"dijitReset dijitStretch dijitButtonContents\">Cancel</button>\n\t\t\t\t\t\t\t\t </span>\n\t\t\t\t\t\t \t </td>\n\t\t\t\t\t\t </tr>\n\t\t\t\t</tbody>\n\t\t</table>\n</div>\n"),datePart:"year",displayedYears:10,useHeader:false,postCreate:function(){
this.cloneClass(".dojoxCal-MY-G-Template",5,".dojoxCal-MY-btns");
this.monthContainer=this.yearContainer=this.myContainer;
var _59="dojoxCalendarYearLabel";
var _5a="dojoxCalendarDecrease";
var _5b="dojoxCalendarIncrease";
_2.query("."+_59,this.myContainer).forEach(function(_5c,idx){
var _5d=_5b;
switch(idx){
case 0:
_5d=_5a;
case 1:
_2.removeClass(_5c,_59);
_2.addClass(_5c,_5d);
break;
}
});
this._decBtn=_2.query("."+_5a,this.myContainer)[0];
this._incBtn=_2.query("."+_5b,this.myContainer)[0];
_2.query(".dojoxCal-MY-M-Template",this.domNode).filter(function(_5e){
return _5e.cellIndex==1;
}).addClass("dojoxCal-MY-M-last");
_2.connect(this,"onBeforeDisplay",_2.hitch(this,function(){
this._cachedDate=new Date(this.get("value").getTime());
this._populateYears(this._cachedDate.getFullYear());
this._populateMonths();
this._updateSelectedMonth();
this._updateSelectedYear();
}));
_2.connect(this,"_populateYears",_2.hitch(this,function(){
this._updateSelectedYear();
}));
_2.connect(this,"_populateMonths",_2.hitch(this,function(){
this._updateSelectedMonth();
}));
this._cachedDate=this.get("value");
this._populateYears();
this._populateMonths();
this.addFx(".dojoxCalendarMonthLabel,.dojoxCalendarYearLabel ",this.myContainer);
},_setValueAttr:function(_5f){
if(_5f&&_5f.getFullYear()){
this._populateYears(_5f.getFullYear());
}
},getHeader:function(){
return null;
},_getMonthNames:function(_60){
this._monthNames=this._monthNames||_2.date.locale.getNames("months",_60,"standAlone",this.getLang());
return this._monthNames;
},_populateMonths:function(){
var _61=this._getMonthNames("abbr");
_2.query(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_2.hitch(this,function(_62,cnt){
this._setText(_62,_61[cnt]);
}));
var _63=this.get("constraints");
if(_63){
var _64=new Date();
_64.setFullYear(this._year);
var min=-1,max=12;
if(_63.min){
var _65=_63.min.getFullYear();
if(_65>this._year){
min=12;
}else{
if(_65==this._year){
min=_63.min.getMonth();
}
}
}
if(_63.max){
var _66=_63.max.getFullYear();
if(_66<this._year){
max=-1;
}else{
if(_66==this._year){
max=_63.max.getMonth();
}
}
}
_2.query(".dojoxCalendarMonthLabel",this.monthContainer).forEach(_2.hitch(this,function(_67,cnt){
_2[(cnt<min||cnt>max)?"addClass":"removeClass"](_67,"dijitCalendarDisabledDate");
}));
}
var h=this.getHeader();
if(h){
this._setText(this.getHeader(),this.get("value").getFullYear());
}
},_populateYears:function(_68){
var _69=this.get("constraints");
var _6a=_68||this.get("value").getFullYear();
var _6b=_6a-Math.floor(this.displayedYears/2);
var min=_69&&_69.min?_69.min.getFullYear():_6b-10000;
_6b=Math.max(min,_6b);
this._displayedYear=_6a;
var _6c=_2.query(".dojoxCalendarYearLabel",this.yearContainer);
var max=_69&&_69.max?_69.max.getFullYear()-_6b:_6c.length;
var _6d="dijitCalendarDisabledDate";
_6c.forEach(_2.hitch(this,function(_6e,cnt){
if(cnt<=max){
this._setText(_6e,_6b+cnt);
_2.removeClass(_6e,_6d);
}else{
_2.addClass(_6e,_6d);
}
}));
if(this._incBtn){
_2[max<_6c.length?"addClass":"removeClass"](this._incBtn,_6d);
}
if(this._decBtn){
_2[min>=_6b?"addClass":"removeClass"](this._decBtn,_6d);
}
var h=this.getHeader();
if(h){
this._setText(this.getHeader(),_6b+" - "+(_6b+11));
}
},_updateSelectedYear:function(){
this._year=String((this._cachedDate||this.get("value")).getFullYear());
this._updateSelectedNode(".dojoxCalendarYearLabel",_2.hitch(this,function(_6f,idx){
return this._year!==null&&_6f.innerHTML==this._year;
}));
},_updateSelectedMonth:function(){
var _70=(this._cachedDate||this.get("value")).getMonth();
this._month=_70;
this._updateSelectedNode(".dojoxCalendarMonthLabel",function(_71,idx){
return idx==_70;
});
},_updateSelectedNode:function(_72,_73){
var sel="dijitCalendarSelectedDate";
_2.query(_72,this.domNode).forEach(function(_74,idx,_75){
_2[_73(_74,idx,_75)?"addClass":"removeClass"](_74.parentNode,sel);
});
var _76=_2.query(".dojoxCal-MY-M-Template div",this.myContainer).filter(function(_77){
return _2.hasClass(_77.parentNode,sel);
})[0];
if(!_76){
return;
}
var _78=_2.hasClass(_76,"dijitCalendarDisabledDate");
_2[_78?"addClass":"removeClass"](this.okBtn,"dijitDisabled");
},onClick:function(evt){
var _79;
var _7a=this;
var sel="dijitCalendarSelectedDate";
function hc(c){
return _2.hasClass(evt.target,c);
};
if(hc("dijitCalendarDisabledDate")){
_2.stopEvent(evt);
return false;
}
if(hc("dojoxCalendarMonthLabel")){
_79="dojoxCal-MY-M-Template";
this._month=evt.target.parentNode.cellIndex+(evt.target.parentNode.parentNode.rowIndex*2);
this._cachedDate.setMonth(this._month);
this._updateSelectedMonth();
}else{
if(hc("dojoxCalendarYearLabel")){
_79="dojoxCal-MY-Y-Template";
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
_2.stopEvent(evt);
return false;
},onOk:function(evt){
_2.stopEvent(evt);
if(_2.hasClass(this.okBtn,"dijitDisabled")){
return false;
}
this.onValueSelected(this._cachedDate);
return false;
},onCancel:function(evt){
_2.stopEvent(evt);
this.onValueSelected(this.get("value"));
return false;
}});
_2.declare("dojox.widget.Calendar2Pane",[_3.widget._CalendarBase,_3.widget._CalendarDay,_3.widget._CalendarMonthYear],{});
_2.declare("dojox.widget.Calendar",[_3.widget._CalendarBase,_3.widget._CalendarDay,_3.widget._CalendarMonthYear],{});
_2.declare("dojox.widget.DailyCalendar",[_3.widget._CalendarBase,_3.widget._CalendarDay],{_makeDate:function(_7b){
var now=new Date();
now.setDate(_7b);
return now;
}});
_2.declare("dojox.widget.MonthAndYearlyCalendar",[_3.widget._CalendarBase,_3.widget._CalendarMonthYear],{});
});
