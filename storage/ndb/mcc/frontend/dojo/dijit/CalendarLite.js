//>>built
require({cache:{"url:dijit/templates/Calendar.html":"<table cellspacing=\"0\" cellpadding=\"0\" class=\"dijitCalendarContainer\" role=\"grid\" aria-labelledby=\"${id}_mddb ${id}_year\">\n\t<thead>\n\t\t<tr class=\"dijitReset dijitCalendarMonthContainer\" valign=\"top\">\n\t\t\t<th class='dijitReset dijitCalendarArrow' data-dojo-attach-point=\"decrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarDecrease\" role=\"presentation\"/>\n\t\t\t\t<span data-dojo-attach-point=\"decreaseArrowNode\" class=\"dijitA11ySideArrow\">-</span>\n\t\t\t</th>\n\t\t\t<th class='dijitReset' colspan=\"5\">\n\t\t\t\t<div data-dojo-attach-point=\"monthNode\">\n\t\t\t\t</div>\n\t\t\t</th>\n\t\t\t<th class='dijitReset dijitCalendarArrow' data-dojo-attach-point=\"incrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarIncrease\" role=\"presentation\"/>\n\t\t\t\t<span data-dojo-attach-point=\"increaseArrowNode\" class=\"dijitA11ySideArrow\">+</span>\n\t\t\t</th>\n\t\t</tr>\n\t\t<tr>\n\t\t\t${!dayCellsHtml}\n\t\t</tr>\n\t</thead>\n\t<tbody data-dojo-attach-point=\"dateRowsNode\" data-dojo-attach-event=\"onclick: _onDayClick\" class=\"dijitReset dijitCalendarBodyContainer\">\n\t\t\t${!dateRowsHtml}\n\t</tbody>\n\t<tfoot class=\"dijitReset dijitCalendarYearContainer\">\n\t\t<tr>\n\t\t\t<td class='dijitReset' valign=\"top\" colspan=\"7\" role=\"presentation\">\n\t\t\t\t<div class=\"dijitCalendarYearLabel\">\n\t\t\t\t\t<span data-dojo-attach-point=\"previousYearLabelNode\" class=\"dijitInline dijitCalendarPreviousYear\" role=\"button\"></span>\n\t\t\t\t\t<span data-dojo-attach-point=\"currentYearLabelNode\" class=\"dijitInline dijitCalendarSelectedYear\" role=\"button\" id=\"${id}_year\"></span>\n\t\t\t\t\t<span data-dojo-attach-point=\"nextYearLabelNode\" class=\"dijitInline dijitCalendarNextYear\" role=\"button\"></span>\n\t\t\t\t</div>\n\t\t\t</td>\n\t\t</tr>\n\t</tfoot>\n</table>\n"}});
define("dijit/CalendarLite",["dojo/_base/array","dojo/_base/declare","dojo/cldr/supplemental","dojo/date","dojo/date/locale","dojo/dom","dojo/dom-class","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/string","dojo/_base/window","./_WidgetBase","./_TemplatedMixin","dojo/text!./templates/Calendar.html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var _10=_2("dijit.CalendarLite",[_d,_e],{templateString:_f,dowTemplateString:"<th class=\"dijitReset dijitCalendarDayLabelTemplate\" role=\"columnheader\"><span class=\"dijitCalendarDayLabel\">${d}</span></th>",dateTemplateString:"<td class=\"dijitReset\" role=\"gridcell\" data-dojo-attach-point=\"dateCells\"><span class=\"dijitCalendarDateLabel\" data-dojo-attach-point=\"dateLabels\"></span></td>",weekTemplateString:"<tr class=\"dijitReset dijitCalendarWeekTemplate\" role=\"row\">${d}${d}${d}${d}${d}${d}${d}</tr>",value:new Date(""),datePackage:_4,dayWidth:"narrow",tabIndex:"0",currentFocus:new Date(),baseClass:"dijitCalendar",_isValidDate:function(_11){
return _11&&!isNaN(_11)&&typeof _11=="object"&&_11.toString()!=this.constructor.prototype.value.toString();
},_getValueAttr:function(){
if(this.value&&!isNaN(this.value)){
var _12=new this.dateClassObj(this.value);
_12.setHours(0,0,0,0);
if(_12.getDate()<this.value.getDate()){
_12=this.dateFuncObj.add(_12,"hour",1);
}
return _12;
}else{
return null;
}
},_setValueAttr:function(_13,_14){
if(_13){
_13=new this.dateClassObj(_13);
}
if(this._isValidDate(_13)){
if(!this._isValidDate(this.value)||this.dateFuncObj.compare(_13,this.value)){
_13.setHours(1,0,0,0);
if(!this.isDisabledDate(_13,this.lang)){
this._set("value",_13);
this.set("currentFocus",_13);
if(_14||typeof _14=="undefined"){
this.onChange(this.get("value"));
}
}
}
}else{
this._set("value",null);
this.set("currentFocus",this.currentFocus);
}
},_setText:function(_15,_16){
while(_15.firstChild){
_15.removeChild(_15.firstChild);
}
_15.appendChild(_c.doc.createTextNode(_16));
},_populateGrid:function(){
var _17=new this.dateClassObj(this.currentFocus);
_17.setDate(1);
var _18=_17.getDay(),_19=this.dateFuncObj.getDaysInMonth(_17),_1a=this.dateFuncObj.getDaysInMonth(this.dateFuncObj.add(_17,"month",-1)),_1b=new this.dateClassObj(),_1c=_3.getFirstDayOfWeek(this.lang);
if(_1c>_18){
_1c-=7;
}
this._date2cell={};
_1.forEach(this.dateCells,function(_1d,idx){
var i=idx+_1c;
var _1e=new this.dateClassObj(_17),_1f,_20="dijitCalendar",adj=0;
if(i<_18){
_1f=_1a-_18+i+1;
adj=-1;
_20+="Previous";
}else{
if(i>=(_18+_19)){
_1f=i-_18-_19+1;
adj=1;
_20+="Next";
}else{
_1f=i-_18+1;
_20+="Current";
}
}
if(adj){
_1e=this.dateFuncObj.add(_1e,"month",adj);
}
_1e.setDate(_1f);
if(!this.dateFuncObj.compare(_1e,_1b,"date")){
_20="dijitCalendarCurrentDate "+_20;
}
if(this._isSelectedDate(_1e,this.lang)){
_20="dijitCalendarSelectedDate "+_20;
_1d.setAttribute("aria-selected",true);
}else{
_1d.setAttribute("aria-selected",false);
}
if(this.isDisabledDate(_1e,this.lang)){
_20="dijitCalendarDisabledDate "+_20;
_1d.setAttribute("aria-disabled",true);
}else{
_20="dijitCalendarEnabledDate "+_20;
_1d.removeAttribute("aria-disabled");
}
var _21=this.getClassForDate(_1e,this.lang);
if(_21){
_20=_21+" "+_20;
}
_1d.className=_20+"Month dijitCalendarDateTemplate";
var _22=_1e.valueOf();
this._date2cell[_22]=_1d;
_1d.dijitDateValue=_22;
this._setText(this.dateLabels[idx],_1e.getDateLocalized?_1e.getDateLocalized(this.lang):_1e.getDate());
},this);
this.monthWidget.set("month",_17);
var y=_17.getFullYear()-1;
var d=new this.dateClassObj();
_1.forEach(["previous","current","next"],function(_23){
d.setFullYear(y++);
this._setText(this[_23+"YearLabelNode"],this.dateLocaleModule.format(d,{selector:"year",locale:this.lang}));
},this);
},goToToday:function(){
this.set("value",new this.dateClassObj());
},constructor:function(_24){
this.datePackage=_24.datePackage||this.datePackage;
this.dateFuncObj=typeof this.datePackage=="string"?_9.getObject(this.datePackage,false):this.datePackage;
this.dateClassObj=this.dateFuncObj.Date||Date;
this.dateLocaleModule=_9.getObject("locale",false,this.dateFuncObj);
},_createMonthWidget:function(){
return _10._MonthWidget({id:this.id+"_mw",lang:this.lang,dateLocaleModule:this.dateLocaleModule},this.monthNode);
},buildRendering:function(){
var d=this.dowTemplateString,_25=this.dateLocaleModule.getNames("days",this.dayWidth,"standAlone",this.lang),_26=_3.getFirstDayOfWeek(this.lang);
this.dayCellsHtml=_b.substitute([d,d,d,d,d,d,d].join(""),{d:""},function(){
return _25[_26++%7];
});
var r=_b.substitute(this.weekTemplateString,{d:this.dateTemplateString});
this.dateRowsHtml=[r,r,r,r,r,r].join("");
this.dateCells=[];
this.dateLabels=[];
this.inherited(arguments);
_6.setSelectable(this.domNode,false);
var _27=new this.dateClassObj(this.currentFocus);
this._supportingWidgets.push(this.monthWidget=this._createMonthWidget());
this.set("currentFocus",_27,false);
var _28=_9.hitch(this,function(_29,_2a,_2b){
this.connect(this[_29],"onclick",function(){
this._setCurrentFocusAttr(this.dateFuncObj.add(this.currentFocus,_2a,_2b));
});
});
_28("incrementMonth","month",1);
_28("decrementMonth","month",-1);
_28("nextYearLabelNode","year",1);
_28("previousYearLabelNode","year",-1);
},_setCurrentFocusAttr:function(_2c,_2d){
var _2e=this.currentFocus,_2f=_2e&&this._date2cell?this._date2cell[_2e.valueOf()]:null;
_2c=new this.dateClassObj(_2c);
_2c.setHours(1,0,0,0);
this._set("currentFocus",_2c);
this._populateGrid();
var _30=this._date2cell[_2c.valueOf()];
_30.setAttribute("tabIndex",this.tabIndex);
if(this.focused||_2d){
_30.focus();
}
if(_2f&&_2f!=_30){
if(_a("webkit")){
_2f.setAttribute("tabIndex","-1");
}else{
_2f.removeAttribute("tabIndex");
}
}
},focus:function(){
this._setCurrentFocusAttr(this.currentFocus,true);
},_onDayClick:function(evt){
_8.stop(evt);
for(var _31=evt.target;_31&&!_31.dijitDateValue;_31=_31.parentNode){
}
if(_31&&!_7.contains(_31,"dijitCalendarDisabledDate")){
this.set("value",_31.dijitDateValue);
}
},onChange:function(){
},_isSelectedDate:function(_32){
return this._isValidDate(this.value)&&!this.dateFuncObj.compare(_32,this.value,"date");
},isDisabledDate:function(){
},getClassForDate:function(){
}});
_10._MonthWidget=_2("dijit.CalendarLite._MonthWidget",_d,{_setMonthAttr:function(_33){
var _34=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_33),_35=(_a("ie")==6?"":"<div class='dijitSpacer'>"+_1.map(_34,function(s){
return "<div>"+s+"</div>";
}).join("")+"</div>");
this.domNode.innerHTML=_35+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_34[_33.getMonth()]+"</div>";
}});
return _10;
});
