//>>built
require({cache:{"url:dijit/templates/Calendar.html":"<div class=\"dijitCalendarContainer dijitInline\" role=\"presentation\" aria-labelledby=\"${id}_mddb ${id}_year\">\n\t<div class=\"dijitReset dijitCalendarMonthContainer\" role=\"presentation\">\n\t\t<div class='dijitReset dijitCalendarArrow dijitCalendarDecrementArrow' data-dojo-attach-point=\"decrementMonth\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarDecrease\" role=\"presentation\"/>\n\t\t\t<span data-dojo-attach-point=\"decreaseArrowNode\" class=\"dijitA11ySideArrow\">-</span>\n\t\t</div>\n\t\t<div class='dijitReset dijitCalendarArrow dijitCalendarIncrementArrow' data-dojo-attach-point=\"incrementMonth\">\n\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarIncrease\" role=\"presentation\"/>\n\t\t\t<span data-dojo-attach-point=\"increaseArrowNode\" class=\"dijitA11ySideArrow\">+</span>\n\t\t</div>\n\t\t<div data-dojo-attach-point=\"monthNode\" class=\"dijitInline\"></div>\n\t</div>\n\t<table cellspacing=\"0\" cellpadding=\"0\" role=\"grid\" data-dojo-attach-point=\"gridNode\">\n\t\t<thead>\n\t\t\t<tr role=\"row\">\n\t\t\t\t${!dayCellsHtml}\n\t\t\t</tr>\n\t\t</thead>\n\t\t<tbody data-dojo-attach-point=\"dateRowsNode\" data-dojo-attach-event=\"ondijitclick: _onDayClick\" class=\"dijitReset dijitCalendarBodyContainer\">\n\t\t\t\t${!dateRowsHtml}\n\t\t</tbody>\n\t</table>\n\t<div class=\"dijitReset dijitCalendarYearContainer\" role=\"presentation\">\n\t\t<div class=\"dijitCalendarYearLabel\">\n\t\t\t<span data-dojo-attach-point=\"previousYearLabelNode\" class=\"dijitInline dijitCalendarPreviousYear\" role=\"button\"></span>\n\t\t\t<span data-dojo-attach-point=\"currentYearLabelNode\" class=\"dijitInline dijitCalendarSelectedYear\" role=\"button\" id=\"${id}_year\"></span>\n\t\t\t<span data-dojo-attach-point=\"nextYearLabelNode\" class=\"dijitInline dijitCalendarNextYear\" role=\"button\"></span>\n\t\t</div>\n\t</div>\n</div>\n"}});
define("dijit/CalendarLite",["dojo/_base/array","dojo/_base/declare","dojo/cldr/supplemental","dojo/date","dojo/date/locale","dojo/date/stamp","dojo/dom","dojo/dom-class","dojo/dom-attr","dojo/_base/lang","dojo/on","dojo/sniff","dojo/string","./_WidgetBase","./_TemplatedMixin","dojo/text!./templates/Calendar.html","./a11yclick","./hccss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,on,_b,_c,_d,_e,_f){
var _10=_2("dijit.CalendarLite",[_d,_e],{templateString:_f,dowTemplateString:"<th class=\"dijitReset dijitCalendarDayLabelTemplate\" role=\"columnheader\" scope=\"col\"><span class=\"dijitCalendarDayLabel\">${d}</span></th>",dateTemplateString:"<td class=\"dijitReset\" role=\"gridcell\" data-dojo-attach-point=\"dateCells\"><span class=\"dijitCalendarDateLabel\" data-dojo-attach-point=\"dateLabels\"></span></td>",weekTemplateString:"<tr class=\"dijitReset dijitCalendarWeekTemplate\" role=\"row\">${d}${d}${d}${d}${d}${d}${d}</tr>",value:new Date(""),datePackage:"",dayWidth:"narrow",tabIndex:"0",dayOffset:-1,currentFocus:new Date(),_setSummaryAttr:"gridNode",baseClass:"dijitCalendar dijitCalendarLite",_isValidDate:function(_11){
return _11&&!isNaN(_11)&&typeof _11=="object"&&_11.toString()!=this.constructor.prototype.value.toString();
},_getValueAttr:function(){
var _12=this._get("value");
if(_12&&!isNaN(_12)){
var _13=new this.dateClassObj(_12);
_13.setHours(0,0,0,0);
if(_13.getDate()<_12.getDate()){
_13=this.dateModule.add(_13,"hour",1);
}
return _13;
}else{
return null;
}
},_setValueAttr:function(_14,_15){
if(typeof _14=="string"){
_14=_6.fromISOString(_14);
}
_14=this._patchDate(_14);
if(this._isValidDate(_14)&&!this.isDisabledDate(_14,this.lang)){
this._set("value",_14);
this.set("currentFocus",_14);
this._markSelectedDates([_14]);
if(this._created&&(_15||typeof _15=="undefined")){
this.onChange(this.get("value"));
}
}else{
this._set("value",null);
this._markSelectedDates([]);
}
},_patchDate:function(_16){
if(_16||_16===0){
_16=new this.dateClassObj(_16);
_16.setHours(1,0,0,0);
}
return _16;
},_setText:function(_17,_18){
while(_17.firstChild){
_17.removeChild(_17.firstChild);
}
_17.appendChild(_17.ownerDocument.createTextNode(_18));
},_populateGrid:function(){
var _19=new this.dateClassObj(this.currentFocus);
_19.setDate(1);
_19=this._patchDate(_19);
var _1a=_19.getDay(),_1b=this.dateModule.getDaysInMonth(_19),_1c=this.dateModule.getDaysInMonth(this.dateModule.add(_19,"month",-1)),_1d=new this.dateClassObj(),_1e=this.dayOffset>=0?this.dayOffset:_3.getFirstDayOfWeek(this.lang);
if(_1e>_1a){
_1e-=7;
}
if(!this.summary){
var _1f=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_19);
this.gridNode.setAttribute("summary",_1f[_19.getMonth()]);
}
this._date2cell={};
_1.forEach(this.dateCells,function(_20,idx){
var i=idx+_1e;
var _21=new this.dateClassObj(_19),_22,_23="dijitCalendar",adj=0;
if(i<_1a){
_22=_1c-_1a+i+1;
adj=-1;
_23+="Previous";
}else{
if(i>=(_1a+_1b)){
_22=i-_1a-_1b+1;
adj=1;
_23+="Next";
}else{
_22=i-_1a+1;
_23+="Current";
}
}
if(adj){
_21=this.dateModule.add(_21,"month",adj);
}
_21.setDate(_22);
if(!this.dateModule.compare(_21,_1d,"date")){
_23="dijitCalendarCurrentDate "+_23;
}
if(this.isDisabledDate(_21,this.lang)){
_23="dijitCalendarDisabledDate "+_23;
_20.setAttribute("aria-disabled","true");
}else{
_23="dijitCalendarEnabledDate "+_23;
_20.removeAttribute("aria-disabled");
_20.setAttribute("aria-selected","false");
}
var _24=this.getClassForDate(_21,this.lang);
if(_24){
_23=_24+" "+_23;
}
_20.className=_23+"Month dijitCalendarDateTemplate";
var _25=_21.valueOf();
this._date2cell[_25]=_20;
_20.dijitDateValue=_25;
var _26=_21.getDateLocalized?_21.getDateLocalized(this.lang):_21.getDate();
this._setText(this.dateLabels[idx],_26);
_9.set(_20,"aria-label",_5.format(_21,{selector:"date",formatLength:"long"}));
},this);
},_populateControls:function(){
var _27=new this.dateClassObj(this.currentFocus);
_27.setDate(1);
this.monthWidget.set("month",_27);
var y=_27.getFullYear()-1;
var d=new this.dateClassObj();
_1.forEach(["previous","current","next"],function(_28){
d.setFullYear(y++);
this._setText(this[_28+"YearLabelNode"],this.dateLocaleModule.format(d,{selector:"year",locale:this.lang}));
},this);
},goToToday:function(){
this.set("value",new this.dateClassObj());
},constructor:function(_29){
this.dateModule=_29.datePackage?_a.getObject(_29.datePackage,false):_4;
this.dateClassObj=this.dateModule.Date||Date;
this.dateLocaleModule=_29.datePackage?_a.getObject(_29.datePackage+".locale",false):_5;
},_createMonthWidget:function(){
return _10._MonthWidget({id:this.id+"_mddb",lang:this.lang,dateLocaleModule:this.dateLocaleModule},this.monthNode);
},buildRendering:function(){
var d=this.dowTemplateString,_2a=this.dateLocaleModule.getNames("days",this.dayWidth,"standAlone",this.lang),_2b=this.dayOffset>=0?this.dayOffset:_3.getFirstDayOfWeek(this.lang);
this.dayCellsHtml=_c.substitute([d,d,d,d,d,d,d].join(""),{d:""},function(){
return _2a[_2b++%7];
});
var r=_c.substitute(this.weekTemplateString,{d:this.dateTemplateString});
this.dateRowsHtml=[r,r,r,r,r,r].join("");
this.dateCells=[];
this.dateLabels=[];
this.inherited(arguments);
_7.setSelectable(this.domNode,false);
var _2c=new this.dateClassObj(this.currentFocus);
this.monthWidget=this._createMonthWidget();
this.set("currentFocus",_2c,false);
},postCreate:function(){
this.inherited(arguments);
this._connectControls();
},_connectControls:function(){
var _2d=_a.hitch(this,function(_2e,_2f,_30){
this[_2e].dojoClick=true;
return on(this[_2e],"click",_a.hitch(this,function(){
this._setCurrentFocusAttr(this.dateModule.add(this.currentFocus,_2f,_30));
}));
});
this.own(_2d("incrementMonth","month",1),_2d("decrementMonth","month",-1),_2d("nextYearLabelNode","year",1),_2d("previousYearLabelNode","year",-1));
},_setCurrentFocusAttr:function(_31,_32){
var _33=this.currentFocus,_34=this._getNodeByDate(_33);
_31=this._patchDate(_31);
this._set("currentFocus",_31);
if(!this._date2cell||this.dateModule.difference(_33,_31,"month")!=0){
this._populateGrid();
this._populateControls();
this._markSelectedDates([this.value]);
}
var _35=this._getNodeByDate(_31);
_35.setAttribute("tabIndex",this.tabIndex);
if(this.focused||_32){
_35.focus();
}
if(_34&&_34!=_35){
if(_b("webkit")){
_34.setAttribute("tabIndex","-1");
}else{
_34.removeAttribute("tabIndex");
}
}
},focus:function(){
this._setCurrentFocusAttr(this.currentFocus,true);
},_onDayClick:function(evt){
evt.stopPropagation();
evt.preventDefault();
for(var _36=evt.target;_36&&!_36.dijitDateValue&&_36.dijitDateValue!==0;_36=_36.parentNode){
}
if(_36&&!_8.contains(_36,"dijitCalendarDisabledDate")){
this.set("value",_36.dijitDateValue);
}
},_getNodeByDate:function(_37){
_37=this._patchDate(_37);
return _37&&this._date2cell?this._date2cell[_37.valueOf()]:null;
},_markSelectedDates:function(_38){
function _39(_3a,_3b){
_8.toggle(_3b,"dijitCalendarSelectedDate",_3a);
_3b.setAttribute("aria-selected",_3a?"true":"false");
};
_1.forEach(this._selectedCells||[],_a.partial(_39,false));
this._selectedCells=_1.filter(_1.map(_38,this._getNodeByDate,this),function(n){
return n;
});
_1.forEach(this._selectedCells,_a.partial(_39,true));
},onChange:function(){
},isDisabledDate:function(){
},getClassForDate:function(){
}});
_10._MonthWidget=_2("dijit.CalendarLite._MonthWidget",_d,{_setMonthAttr:function(_3c){
var _3d=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_3c),_3e=(_b("ie")==6?"":"<div class='dijitSpacer'>"+_1.map(_3d,function(s){
return "<div>"+s+"</div>";
}).join("")+"</div>");
this.domNode.innerHTML=_3e+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_3d[_3c.getMonth()]+"</div>";
}});
return _10;
});
