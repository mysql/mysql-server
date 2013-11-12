//>>built
require({cache:{"url:dojox/widget/MultiSelectCalendar/MultiSelectCalendar.html":"<table cellspacing=\"0\" cellpadding=\"0\" class=\"dijitCalendarContainer\" role=\"grid\" dojoAttachEvent=\"onkeypress: _onKeyPress\" aria-labelledby=\"${id}_year\">\n\t<thead>\n\t\t<tr class=\"dijitReset dijitCalendarMonthContainer\" valign=\"top\">\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"decrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarDecrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"decreaseArrowNode\" class=\"dijitA11ySideArrow\">-</span>\n\t\t\t</th>\n\t\t\t<th class='dijitReset' colspan=\"5\">\n\t\t\t\t<div dojoType=\"dijit.form.DropDownButton\" dojoAttachPoint=\"monthDropDownButton\"\n\t\t\t\t\tid=\"${id}_mddb\" tabIndex=\"-1\">\n\t\t\t\t</div>\n\t\t\t</th>\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"incrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarIncrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"increaseArrowNode\" class=\"dijitA11ySideArrow\">+</span>\n\t\t\t</th>\n\t\t</tr>\n\t\t<tr>\n\t\t\t<th class=\"dijitReset dijitCalendarDayLabelTemplate\" role=\"columnheader\"><span class=\"dijitCalendarDayLabel\"></span></th>\n\t\t</tr>\n\t</thead>\n\t<tbody dojoAttachEvent=\"onclick: _onDayClick, onmouseover: _onDayMouseOver, onmouseout: _onDayMouseOut, onmousedown: _onDayMouseDown, onmouseup: _onDayMouseUp\" class=\"dijitReset dijitCalendarBodyContainer\">\n\t\t<tr class=\"dijitReset dijitCalendarWeekTemplate\" role=\"row\">\n\t\t\t<td class=\"dijitReset dijitCalendarDateTemplate\" role=\"gridcell\"><span class=\"dijitCalendarDateLabel\"></span></td>\n\t\t</tr>\n\t</tbody>\n\t<tfoot class=\"dijitReset dijitCalendarYearContainer\">\n\t\t<tr>\n\t\t\t<td class='dijitReset' valign=\"top\" colspan=\"7\">\n\t\t\t\t<h3 class=\"dijitCalendarYearLabel\">\n\t\t\t\t\t<span dojoAttachPoint=\"previousYearLabelNode\" class=\"dijitInline dijitCalendarPreviousYear\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"currentYearLabelNode\" class=\"dijitInline dijitCalendarSelectedYear\" id=\"${id}_year\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"nextYearLabelNode\" class=\"dijitInline dijitCalendarNextYear\"></span>\n\t\t\t\t</h3>\n\t\t\t</td>\n\t\t</tr>\n\t</tfoot>\n</table>"}});
define("dojox/widget/MultiSelectCalendar",["dojo/main","dijit","dojo/text!./MultiSelectCalendar/MultiSelectCalendar.html","dojo/cldr/supplemental","dojo/date","dojo/date/locale","dijit/_Widget","dijit/_Templated","dijit/_CssStateMixin","dijit/form/DropDownButton","dijit/typematic"],function(_1,_2,_3){
_1.experimental("dojox.widget.MultiSelectCalendar");
_1.declare("dojox.widget.MultiSelectCalendar",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin,_2._CssStateMixin],{templateString:_3,widgetsInTemplate:true,value:{},datePackage:"dojo.date",dayWidth:"narrow",tabIndex:"0",returnIsoRanges:false,currentFocus:new Date(),baseClass:"dijitCalendar",cssStateNodes:{"decrementMonth":"dijitCalendarArrow","incrementMonth":"dijitCalendarArrow","previousYearLabelNode":"dijitCalendarPreviousYear","nextYearLabelNode":"dijitCalendarNextYear"},_areValidDates:function(_4){
for(var _5 in this.value){
valid=(_5&&!isNaN(_5)&&typeof _4=="object"&&_5.toString()!=this.constructor.prototype.value.toString());
if(!valid){
return false;
}
}
return true;
},_getValueAttr:function(){
if(this.returnIsoRanges){
datesWithRanges=this._returnDatesWithIsoRanges(this._sort());
return datesWithRanges;
}else{
return this._sort();
}
},_setValueAttr:function(_6,_7){
this.value={};
if(_1.isArray(_6)){
_1.forEach(_6,function(_8,i){
var _9=_8.indexOf("/");
if(_9==-1){
this.value[_8]=1;
}else{
var _a=new _1.date.stamp.fromISOString(_8.substr(0,10));
var _b=new _1.date.stamp.fromISOString(_8.substr(11,10));
this.toggleDate(_a,[],[]);
if((_a-_b)>0){
this._addToRangeRTL(_a,_b,[],[]);
}else{
this._addToRangeLTR(_a,_b,[],[]);
}
}
},this);
if(_6.length>0){
this.focusOnLastDate(_6[_6.length-1]);
}
}else{
if(_6){
_6=new this.dateClassObj(_6);
}
if(this._isValidDate(_6)){
_6.setHours(1,0,0,0);
if(!this.isDisabledDate(_6,this.lang)){
dateIndex=_1.date.stamp.toISOString(_6).substring(0,10);
this.value[dateIndex]=1;
this.set("currentFocus",_6);
if(_7||typeof _7=="undefined"){
this.onChange(this.get("value"));
this.onValueSelected(this.get("value"));
}
}
}
}
this._populateGrid();
},focusOnLastDate:function(_c){
var _d=_c.indexOf("/");
var _e,_f;
if(_d==-1){
lastDate=_c;
}else{
_e=new _1.date.stamp.fromISOString(_c.substr(0,10));
_f=new _1.date.stamp.fromISOString(_c.substr(11,10));
if((_e-_f)>0){
lastDate=_e;
}else{
lastDate=_f;
}
}
this.set("currentFocus",lastDate);
},_isValidDate:function(_10){
return _10&&!isNaN(_10)&&typeof _10=="object"&&_10.toString()!=this.constructor.prototype.value.toString();
},_setText:function(_11,_12){
while(_11.firstChild){
_11.removeChild(_11.firstChild);
}
_11.appendChild(_1.doc.createTextNode(_12));
},_populateGrid:function(){
var _13=new this.dateClassObj(this.currentFocus);
_13.setDate(1);
var _14=_13.getDay(),_15=this.dateFuncObj.getDaysInMonth(_13),_16=this.dateFuncObj.getDaysInMonth(this.dateFuncObj.add(_13,"month",-1)),_17=new this.dateClassObj(),_18=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
if(_18>_14){
_18-=7;
}
this.listOfNodes=_1.query(".dijitCalendarDateTemplate",this.domNode);
this.listOfNodes.forEach(function(_19,i){
i+=_18;
var _1a=new this.dateClassObj(_13),_1b,_1c="dijitCalendar",adj=0;
if(i<_14){
_1b=_16-_14+i+1;
adj=-1;
_1c+="Previous";
}else{
if(i>=(_14+_15)){
_1b=i-_14-_15+1;
adj=1;
_1c+="Next";
}else{
_1b=i-_14+1;
_1c+="Current";
}
}
if(adj){
_1a=this.dateFuncObj.add(_1a,"month",adj);
}
_1a.setDate(_1b);
if(!this.dateFuncObj.compare(_1a,_17,"date")){
_1c="dijitCalendarCurrentDate "+_1c;
}
dateIndex=_1.date.stamp.toISOString(_1a).substring(0,10);
if(!this.isDisabledDate(_1a,this.lang)){
if(this._isSelectedDate(_1a,this.lang)){
if(this.value[dateIndex]){
_1c="dijitCalendarSelectedDate "+_1c;
}else{
_1c=_1c.replace("dijitCalendarSelectedDate ","");
}
}
}
if(this._isSelectedDate(_1a,this.lang)){
_1c="dijitCalendarBrowsingDate "+_1c;
}
if(this.isDisabledDate(_1a,this.lang)){
_1c="dijitCalendarDisabledDate "+_1c;
}
var _1d=this.getClassForDate(_1a,this.lang);
if(_1d){
_1c=_1d+" "+_1c;
}
_19.className=_1c+"Month dijitCalendarDateTemplate";
_19.dijitDateValue=_1a.valueOf();
_1.attr(_19,"dijitDateValue",_1a.valueOf());
var _1e=_1.query(".dijitCalendarDateLabel",_19)[0],_1f=_1a.getDateLocalized?_1a.getDateLocalized(this.lang):_1a.getDate();
this._setText(_1e,_1f);
},this);
var _20=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_13);
this.monthDropDownButton.dropDown.set("months",_20);
this.monthDropDownButton.containerNode.innerHTML=(_1.isIE==6?"":"<div class='dijitSpacer'>"+this.monthDropDownButton.dropDown.domNode.innerHTML+"</div>")+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_20[_13.getMonth()]+"</div>";
var y=_13.getFullYear()-1;
var d=new this.dateClassObj();
_1.forEach(["previous","current","next"],function(_21){
d.setFullYear(y++);
this._setText(this[_21+"YearLabelNode"],this.dateLocaleModule.format(d,{selector:"year",locale:this.lang}));
},this);
},goToToday:function(){
this.set("currentFocus",new this.dateClassObj(),false);
},constructor:function(_22){
var _23=(_22.datePackage&&(_22.datePackage!="dojo.date"))?_22.datePackage+".Date":"Date";
this.dateClassObj=_1.getObject(_23,false);
this.datePackage=_22.datePackage||this.datePackage;
this.dateFuncObj=_1.getObject(this.datePackage,false);
this.dateLocaleModule=_1.getObject(this.datePackage+".locale",false);
},buildRendering:function(){
this.inherited(arguments);
_1.setSelectable(this.domNode,false);
var _24=_1.hitch(this,function(_25,n){
var _26=_1.query(_25,this.domNode)[0];
for(var i=0;i<n;i++){
_26.parentNode.appendChild(_26.cloneNode(true));
}
});
_24(".dijitCalendarDayLabelTemplate",6);
_24(".dijitCalendarDateTemplate",6);
_24(".dijitCalendarWeekTemplate",5);
var _27=this.dateLocaleModule.getNames("days",this.dayWidth,"standAlone",this.lang);
var _28=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
_1.query(".dijitCalendarDayLabel",this.domNode).forEach(function(_29,i){
this._setText(_29,_27[(i+_28)%7]);
},this);
var _2a=new this.dateClassObj(this.currentFocus);
this.monthDropDownButton.dropDown=new dojox.widget._MonthDropDown({id:this.id+"_mdd",onChange:_1.hitch(this,"_onMonthSelect")});
this.set("currentFocus",_2a,false);
var _2b=this;
var _2c=function(_2d,_2e,adj){
_2b._connects.push(_2.typematic.addMouseListener(_2b[_2d],_2b,function(_2f){
if(_2f>=0){
_2b._adjustDisplay(_2e,adj);
}
},0.8,500));
};
_2c("incrementMonth","month",1);
_2c("decrementMonth","month",-1);
_2c("nextYearLabelNode","year",1);
_2c("previousYearLabelNode","year",-1);
},_adjustDisplay:function(_30,_31){
this._setCurrentFocusAttr(this.dateFuncObj.add(this.currentFocus,_30,_31));
},_setCurrentFocusAttr:function(_32,_33){
var _34=this.currentFocus,_35=_34?_1.query("[dijitDateValue="+_34.valueOf()+"]",this.domNode)[0]:null;
_32=new this.dateClassObj(_32);
_32.setHours(1,0,0,0);
this._set("currentFocus",_32);
var _36=_1.date.stamp.toISOString(_32).substring(0,7);
if(_36!=this.previousMonth){
this._populateGrid();
this.previousMonth=_36;
}
var _37=_1.query("[dijitDateValue="+_32.valueOf()+"]",this.domNode)[0];
_37.setAttribute("tabIndex",this.tabIndex);
if(this._focused||_33){
_37.focus();
}
if(_35&&_35!=_37){
if(_1.isWebKit){
_35.setAttribute("tabIndex","-1");
}else{
_35.removeAttribute("tabIndex");
}
}
},focus:function(){
this._setCurrentFocusAttr(this.currentFocus,true);
},_onMonthSelect:function(_38){
this.currentFocus=this.dateFuncObj.add(this.currentFocus,"month",_38-this.currentFocus.getMonth());
this._populateGrid();
},toggleDate:function(_39,_3a,_3b){
var _3c=_1.date.stamp.toISOString(_39).substring(0,10);
if(this.value[_3c]){
this.unselectDate(_39,_3b);
}else{
this.selectDate(_39,_3a);
}
},selectDate:function(_3d,_3e){
var _3f=this._getNodeByDate(_3d);
var _40=_3f.className;
var _41=_1.date.stamp.toISOString(_3d).substring(0,10);
this.value[_41]=1;
_3e.push(_41);
_40="dijitCalendarSelectedDate "+_40;
_3f.className=_40;
},unselectDate:function(_42,_43){
var _44=this._getNodeByDate(_42);
var _45=_44.className;
var _46=_1.date.stamp.toISOString(_42).substring(0,10);
delete (this.value[_46]);
_43.push(_46);
_45=_45.replace("dijitCalendarSelectedDate ","");
_44.className=_45;
},_getNodeByDate:function(_47){
var _48=new this.dateClassObj(this.listOfNodes[0].dijitDateValue);
var _49=Math.abs(_1.date.difference(_48,_47,"day"));
return this.listOfNodes[_49];
},_onDayClick:function(evt){
_1.stopEvent(evt);
for(var _4a=evt.target;_4a&&!_4a.dijitDateValue;_4a=_4a.parentNode){
}
if(_4a&&!_1.hasClass(_4a,"dijitCalendarDisabledDate")){
value=new this.dateClassObj(_4a.dijitDateValue);
if(!this.rangeJustSelected){
this.toggleDate(value,[],[]);
this.previouslySelectedDay=value;
this.set("currentFocus",value);
this.onValueSelected([_1.date.stamp.toISOString(value).substring(0,10)]);
}else{
this.rangeJustSelected=false;
this.set("currentFocus",value);
}
}
},_onDayMouseOver:function(evt){
var _4b=_1.hasClass(evt.target,"dijitCalendarDateLabel")?evt.target.parentNode:evt.target;
if(_4b&&(_4b.dijitDateValue||_4b==this.previousYearLabelNode||_4b==this.nextYearLabelNode)){
_1.addClass(_4b,"dijitCalendarHoveredDate");
this._currentNode=_4b;
}
},_setEndRangeAttr:function(_4c){
_4c=new this.dateClassObj(_4c);
_4c.setHours(1);
this.endRange=_4c;
},_getEndRangeAttr:function(){
var _4d=new this.dateClassObj(this.endRange);
_4d.setHours(0,0,0,0);
if(_4d.getDate()<this.endRange.getDate()){
_4d=this.dateFuncObj.add(_4d,"hour",1);
}
return _4d;
},_onDayMouseOut:function(evt){
if(!this._currentNode){
return;
}
if(evt.relatedTarget&&evt.relatedTarget.parentNode==this._currentNode){
return;
}
var cls="dijitCalendarHoveredDate";
if(_1.hasClass(this._currentNode,"dijitCalendarActiveDate")){
cls+=" dijitCalendarActiveDate";
}
_1.removeClass(this._currentNode,cls);
this._currentNode=null;
},_onDayMouseDown:function(evt){
var _4e=evt.target.parentNode;
if(_4e&&_4e.dijitDateValue){
_1.addClass(_4e,"dijitCalendarActiveDate");
this._currentNode=_4e;
}
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_4e.dijitDateValue);
this._selectRange();
}else{
this.selectingRange=false;
this.previousRangeStart=null;
this.previousRangeEnd=null;
}
},_onDayMouseUp:function(evt){
var _4f=evt.target.parentNode;
if(_4f&&_4f.dijitDateValue){
_1.removeClass(_4f,"dijitCalendarActiveDate");
}
},handleKey:function(evt){
var dk=_1.keys,_50=-1,_51,_52=this.currentFocus;
switch(evt.keyCode){
case dk.RIGHT_ARROW:
_50=1;
case dk.LEFT_ARROW:
_51="day";
if(!this.isLeftToRight()){
_50*=-1;
}
break;
case dk.DOWN_ARROW:
_50=1;
case dk.UP_ARROW:
_51="week";
break;
case dk.PAGE_DOWN:
_50=1;
case dk.PAGE_UP:
_51=evt.ctrlKey||evt.altKey?"year":"month";
break;
case dk.END:
_52=this.dateFuncObj.add(_52,"month",1);
_51="day";
case dk.HOME:
_52=new this.dateClassObj(_52);
_52.setDate(1);
break;
case dk.ENTER:
case dk.SPACE:
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_52);
this._selectRange();
}else{
this.selectingRange=false;
this.toggleDate(_52,[],[]);
this.previouslySelectedDay=_52;
this.previousRangeStart=null;
this.previousRangeEnd=null;
this.onValueSelected([_1.date.stamp.toISOString(_52).substring(0,10)]);
}
break;
default:
return true;
}
if(_51){
_52=this.dateFuncObj.add(_52,_51,_50);
}
this.set("currentFocus",_52);
return false;
},_onKeyPress:function(evt){
if(!this.handleKey(evt)){
_1.stopEvent(evt);
}
},_removeFromRangeLTR:function(_53,end,_54,_55){
difference=Math.abs(_1.date.difference(_53,end,"day"));
for(var i=0;i<=difference;i++){
var _56=_1.date.add(_53,"day",i);
this.toggleDate(_56,_54,_55);
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=end;
}
}
if(this.previousRangeStart==null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")>0){
this.previousRangeStart=end;
}
}
this.previouslySelectedDay=_1.date.add(_56,"day",1);
},_removeFromRangeRTL:function(_57,end,_58,_59){
difference=Math.abs(_1.date.difference(_57,end,"day"));
for(var i=0;i<=difference;i++){
var _5a=_1.date.add(_57,"day",-i);
this.toggleDate(_5a,_58,_59);
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")<0){
this.previousRangeEnd=end;
}
}
if(this.previousRangeStart==null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")<0){
this.previousRangeStart=end;
}
}
this.previouslySelectedDay=_1.date.add(_5a,"day",-1);
},_addToRangeRTL:function(_5b,end,_5c,_5d){
difference=Math.abs(_1.date.difference(_5b,end,"day"));
for(var i=1;i<=difference;i++){
var _5e=_1.date.add(_5b,"day",-i);
this.toggleDate(_5e,_5c,_5d);
}
if(this.previousRangeStart==null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")<0){
this.previousRangeStart=end;
}
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=_5b;
}else{
if(_1.date.compare(_5b,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=_5b;
}
}
this.previouslySelectedDay=_5e;
},_addToRangeLTR:function(_5f,end,_60,_61){
difference=Math.abs(_1.date.difference(_5f,end,"day"));
for(var i=1;i<=difference;i++){
var _62=_1.date.add(_5f,"day",i);
this.toggleDate(_62,_60,_61);
}
if(this.previousRangeStart==null){
this.previousRangeStart=_5f;
}else{
if(_1.date.compare(_5f,this.previousRangeStart,"date")<0){
this.previousRangeStart=_5f;
}
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=end;
}
}
this.previouslySelectedDay=_62;
},_selectRange:function(){
var _63=[];
var _64=[];
var _65=this.previouslySelectedDay;
var end=this.get("endRange");
if(!this.previousRangeStart&&!this.previousRangeEnd){
removingFromRange=false;
}else{
if((_1.date.compare(end,this.previousRangeStart,"date")<0)||(_1.date.compare(end,this.previousRangeEnd,"date")>0)){
removingFromRange=false;
}else{
removingFromRange=true;
}
}
if(removingFromRange==true){
if(_1.date.compare(end,_65,"date")<0){
this._removeFromRangeRTL(_65,end,_63,_64);
}else{
this._removeFromRangeLTR(_65,end,_63,_64);
}
}else{
if(_1.date.compare(end,_65,"date")<0){
this._addToRangeRTL(_65,end,_63,_64);
}else{
this._addToRangeLTR(_65,end,_63,_64);
}
}
if(_63.length>0){
this.onValueSelected(_63);
}
if(_64.length>0){
this.onValueUnselected(_64);
}
this.rangeJustSelected=true;
},onValueSelected:function(_66){
},onValueUnselected:function(_67){
},onChange:function(_68){
},_isSelectedDate:function(_69,_6a){
dateIndex=_1.date.stamp.toISOString(_69).substring(0,10);
return this.value[dateIndex];
},isDisabledDate:function(_6b,_6c){
},getClassForDate:function(_6d,_6e){
},_sort:function(){
if(this.value=={}){
return [];
}
var _6f=[];
for(var _70 in this.value){
_6f.push(_70);
}
_6f.sort(function(a,b){
var _71=new Date(a),_72=new Date(b);
return _71-_72;
});
return _6f;
},_returnDatesWithIsoRanges:function(_73){
var _74=[];
if(_73.length>1){
var _75=false,_76=0,_77=null,_78=null,_79=_1.date.stamp.fromISOString(_73[0]);
for(var i=1;i<_73.length+1;i++){
currentDate=_1.date.stamp.fromISOString(_73[i]);
if(_75){
difference=Math.abs(_1.date.difference(_79,currentDate,"day"));
if(difference==1){
_78=currentDate;
}else{
range=_1.date.stamp.toISOString(_77).substring(0,10)+"/"+_1.date.stamp.toISOString(_78).substring(0,10);
_74.push(range);
_75=false;
}
}else{
difference=Math.abs(_1.date.difference(_79,currentDate,"day"));
if(difference==1){
_75=true;
_77=_79;
_78=currentDate;
}else{
_74.push(_1.date.stamp.toISOString(_79).substring(0,10));
}
}
_79=currentDate;
}
return _74;
}else{
return _73;
}
}});
_1.declare("dojox.widget._MonthDropDown",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{months:[],templateString:"<div class='dijitCalendarMonthMenu dijitMenu' "+"dojoAttachEvent='onclick:_onClick,onmouseover:_onMenuHover,onmouseout:_onMenuHover'></div>",_setMonthsAttr:function(_7a){
this.domNode.innerHTML=_1.map(_7a,function(_7b,idx){
return _7b?"<div class='dijitCalendarMonthLabel' month='"+idx+"'>"+_7b+"</div>":"";
}).join("");
},_onClick:function(evt){
this.onChange(_1.attr(evt.target,"month"));
},onChange:function(_7c){
},_onMenuHover:function(evt){
_1.toggleClass(evt.target,"dijitCalendarMonthLabelHover",evt.type=="mouseover");
}});
return dojox.widget.MultiSelectCalendar;
});
