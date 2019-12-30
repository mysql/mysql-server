//>>built
require({cache:{"url:dojox/widget/MultiSelectCalendar/MultiSelectCalendar.html":"<table cellspacing=\"0\" cellpadding=\"0\" class=\"dijitCalendarContainer\" role=\"grid\" dojoAttachEvent=\"onkeydown: _onKeyDown\" aria-labelledby=\"${id}_year\">\n\t<thead>\n\t\t<tr class=\"dijitReset dijitCalendarMonthContainer\" valign=\"top\">\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"decrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarDecrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"decreaseArrowNode\" class=\"dijitA11ySideArrow\">-</span>\n\t\t\t</th>\n\t\t\t<th class='dijitReset' colspan=\"5\">\n\t\t\t\t<div dojoType=\"dijit.form.DropDownButton\" dojoAttachPoint=\"monthDropDownButton\"\n\t\t\t\t\tid=\"${id}_mddb\" tabIndex=\"-1\">\n\t\t\t\t</div>\n\t\t\t</th>\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"incrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarIncrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"increaseArrowNode\" class=\"dijitA11ySideArrow\">+</span>\n\t\t\t</th>\n\t\t</tr>\n\t\t<tr>\n\t\t\t<th class=\"dijitReset dijitCalendarDayLabelTemplate\" role=\"columnheader\"><span class=\"dijitCalendarDayLabel\"></span></th>\n\t\t</tr>\n\t</thead>\n\t<tbody dojoAttachEvent=\"onclick: _onDayClick, onmouseover: _onDayMouseOver, onmouseout: _onDayMouseOut, onmousedown: _onDayMouseDown, onmouseup: _onDayMouseUp\" class=\"dijitReset dijitCalendarBodyContainer\">\n\t\t<tr class=\"dijitReset dijitCalendarWeekTemplate\" role=\"row\">\n\t\t\t<td class=\"dijitReset dijitCalendarDateTemplate\" role=\"gridcell\"><span class=\"dijitCalendarDateLabel\"></span></td>\n\t\t</tr>\n\t</tbody>\n\t<tfoot class=\"dijitReset dijitCalendarYearContainer\">\n\t\t<tr>\n\t\t\t<td class='dijitReset' valign=\"top\" colspan=\"7\">\n\t\t\t\t<h3 class=\"dijitCalendarYearLabel\">\n\t\t\t\t\t<span dojoAttachPoint=\"previousYearLabelNode\" class=\"dijitInline dijitCalendarPreviousYear\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"currentYearLabelNode\" class=\"dijitInline dijitCalendarSelectedYear\" id=\"${id}_year\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"nextYearLabelNode\" class=\"dijitInline dijitCalendarNextYear\"></span>\n\t\t\t\t</h3>\n\t\t\t</td>\n\t\t</tr>\n\t</tfoot>\n</table>"}});
define("dojox/widget/MultiSelectCalendar",["dojo/main","dijit","dojo/text!./MultiSelectCalendar/MultiSelectCalendar.html","dojo/cldr/supplemental","dojo/date","dojo/date/locale","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/_CssStateMixin","dijit/form/DropDownButton","dijit/typematic"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
_1.experimental("dojox.widget.MultiSelectCalendar");
_1.declare("dojox.widget.MultiSelectCalendar",[_7,_8,_9,_a],{templateString:_3,widgetsInTemplate:true,value:{},datePackage:"dojo.date",dayWidth:"narrow",tabIndex:"0",returnIsoRanges:false,currentFocus:new Date(),baseClass:"dijitCalendar",cssStateNodes:{"decrementMonth":"dijitCalendarArrow","incrementMonth":"dijitCalendarArrow","previousYearLabelNode":"dijitCalendarPreviousYear","nextYearLabelNode":"dijitCalendarNextYear"},_areValidDates:function(_d){
for(var _e in this.value){
valid=(_e&&!isNaN(_e)&&typeof _d=="object"&&_e.toString()!=this.constructor.prototype.value.toString());
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
},_setValueAttr:function(_f,_10){
this.value={};
if(_1.isArray(_f)){
_1.forEach(_f,function(_11,i){
var _12=_11.indexOf("/");
if(_12==-1){
this.value[_11]=1;
}else{
var _13=new _1.date.stamp.fromISOString(_11.substr(0,10));
var _14=new _1.date.stamp.fromISOString(_11.substr(11,10));
this.toggleDate(_13,[],[]);
if((_13-_14)>0){
this._addToRangeRTL(_13,_14,[],[]);
}else{
this._addToRangeLTR(_13,_14,[],[]);
}
}
},this);
if(_f.length>0){
this.focusOnLastDate(_f[_f.length-1]);
}
}else{
if(_f){
_f=new this.dateClassObj(_f);
}
if(this._isValidDate(_f)){
_f.setHours(1,0,0,0);
if(!this.isDisabledDate(_f,this.lang)){
dateIndex=_1.date.stamp.toISOString(_f).substring(0,10);
this.value[dateIndex]=1;
this.set("currentFocus",_f);
if(_10||typeof _10=="undefined"){
this.onChange(this.get("value"));
this.onValueSelected(this.get("value"));
}
}
}
}
this._populateGrid();
},focusOnLastDate:function(_15){
var _16=_15.indexOf("/");
var _17,_18;
if(_16==-1){
lastDate=_15;
}else{
_17=new _1.date.stamp.fromISOString(_15.substr(0,10));
_18=new _1.date.stamp.fromISOString(_15.substr(11,10));
if((_17-_18)>0){
lastDate=_17;
}else{
lastDate=_18;
}
}
this.set("currentFocus",lastDate);
},_isValidDate:function(_19){
return _19&&!isNaN(_19)&&typeof _19=="object"&&_19.toString()!=this.constructor.prototype.value.toString();
},_setText:function(_1a,_1b){
while(_1a.firstChild){
_1a.removeChild(_1a.firstChild);
}
_1a.appendChild(_1.doc.createTextNode(_1b));
},_populateGrid:function(){
var _1c=new this.dateClassObj(this.currentFocus);
_1c.setDate(1);
var _1d=_1c.getDay(),_1e=this.dateFuncObj.getDaysInMonth(_1c),_1f=this.dateFuncObj.getDaysInMonth(this.dateFuncObj.add(_1c,"month",-1)),_20=new this.dateClassObj(),_21=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
if(_21>_1d){
_21-=7;
}
this.listOfNodes=_1.query(".dijitCalendarDateTemplate",this.domNode);
this.listOfNodes.forEach(function(_22,i){
i+=_21;
var _23=new this.dateClassObj(_1c),_24,_25="dijitCalendar",adj=0;
if(i<_1d){
_24=_1f-_1d+i+1;
adj=-1;
_25+="Previous";
}else{
if(i>=(_1d+_1e)){
_24=i-_1d-_1e+1;
adj=1;
_25+="Next";
}else{
_24=i-_1d+1;
_25+="Current";
}
}
if(adj){
_23=this.dateFuncObj.add(_23,"month",adj);
}
_23.setDate(_24);
if(!this.dateFuncObj.compare(_23,_20,"date")){
_25="dijitCalendarCurrentDate "+_25;
}
dateIndex=_1.date.stamp.toISOString(_23).substring(0,10);
if(!this.isDisabledDate(_23,this.lang)){
if(this._isSelectedDate(_23,this.lang)){
if(this.value[dateIndex]){
_25="dijitCalendarSelectedDate "+_25;
}else{
_25=_25.replace("dijitCalendarSelectedDate ","");
}
}
}
if(this._isSelectedDate(_23,this.lang)){
_25="dijitCalendarBrowsingDate "+_25;
}
if(this.isDisabledDate(_23,this.lang)){
_25="dijitCalendarDisabledDate "+_25;
}
var _26=this.getClassForDate(_23,this.lang);
if(_26){
_25=_26+" "+_25;
}
_22.className=_25+"Month dijitCalendarDateTemplate";
_22.dijitDateValue=_23.valueOf();
_1.attr(_22,"dijitDateValue",_23.valueOf());
var _27=_1.query(".dijitCalendarDateLabel",_22)[0],_28=_23.getDateLocalized?_23.getDateLocalized(this.lang):_23.getDate();
this._setText(_27,_28);
},this);
var _29=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_1c);
this.monthDropDownButton.dropDown.set("months",_29);
this.monthDropDownButton.containerNode.innerHTML=(_1.isIE==6?"":"<div class='dijitSpacer'>"+this.monthDropDownButton.dropDown.domNode.innerHTML+"</div>")+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_29[_1c.getMonth()]+"</div>";
var y=_1c.getFullYear()-1;
var d=new this.dateClassObj();
_1.forEach(["previous","current","next"],function(_2a){
d.setFullYear(y++);
this._setText(this[_2a+"YearLabelNode"],this.dateLocaleModule.format(d,{selector:"year",locale:this.lang}));
},this);
},goToToday:function(){
this.set("currentFocus",new this.dateClassObj(),false);
},constructor:function(_2b){
var _2c=(_2b.datePackage&&(_2b.datePackage!="dojo.date"))?_2b.datePackage+".Date":"Date";
this.dateClassObj=_1.getObject(_2c,false);
this.datePackage=_2b.datePackage||this.datePackage;
this.dateFuncObj=_1.getObject(this.datePackage,false);
this.dateLocaleModule=_1.getObject(this.datePackage+".locale",false);
},buildRendering:function(){
this.inherited(arguments);
_1.setSelectable(this.domNode,false);
var _2d=_1.hitch(this,function(_2e,n){
var _2f=_1.query(_2e,this.domNode)[0];
for(var i=0;i<n;i++){
_2f.parentNode.appendChild(_2f.cloneNode(true));
}
});
_2d(".dijitCalendarDayLabelTemplate",6);
_2d(".dijitCalendarDateTemplate",6);
_2d(".dijitCalendarWeekTemplate",5);
var _30=this.dateLocaleModule.getNames("days",this.dayWidth,"standAlone",this.lang);
var _31=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
_1.query(".dijitCalendarDayLabel",this.domNode).forEach(function(_32,i){
this._setText(_32,_30[(i+_31)%7]);
},this);
var _33=new this.dateClassObj(this.currentFocus);
this.monthDropDownButton.dropDown=new dojox.widget._MonthDropDown({id:this.id+"_mdd",onChange:_1.hitch(this,"_onMonthSelect")});
this.set("currentFocus",_33,false);
var _34=this;
var _35=function(_36,_37,adj){
_34._connects.push(_2.typematic.addMouseListener(_34[_36],_34,function(_38){
if(_38>=0){
_34._adjustDisplay(_37,adj);
}
},0.8,500));
};
_35("incrementMonth","month",1);
_35("decrementMonth","month",-1);
_35("nextYearLabelNode","year",1);
_35("previousYearLabelNode","year",-1);
},_adjustDisplay:function(_39,_3a){
this._setCurrentFocusAttr(this.dateFuncObj.add(this.currentFocus,_39,_3a));
},_setCurrentFocusAttr:function(_3b,_3c){
var _3d=this.currentFocus,_3e=_3d?_1.query("[dijitDateValue="+_3d.valueOf()+"]",this.domNode)[0]:null;
_3b=new this.dateClassObj(_3b);
_3b.setHours(1,0,0,0);
this._set("currentFocus",_3b);
var _3f=_1.date.stamp.toISOString(_3b).substring(0,7);
if(_3f!=this.previousMonth){
this._populateGrid();
this.previousMonth=_3f;
}
var _40=_1.query("[dijitDateValue="+_3b.valueOf()+"]",this.domNode)[0];
_40.setAttribute("tabIndex",this.tabIndex);
if(this._focused||_3c){
_40.focus();
}
if(_3e&&_3e!=_40){
if(_1.isWebKit){
_3e.setAttribute("tabIndex","-1");
}else{
_3e.removeAttribute("tabIndex");
}
}
},focus:function(){
this._setCurrentFocusAttr(this.currentFocus,true);
},_onMonthSelect:function(_41){
this.currentFocus=this.dateFuncObj.add(this.currentFocus,"month",_41-this.currentFocus.getMonth());
this._populateGrid();
},toggleDate:function(_42,_43,_44){
var _45=_1.date.stamp.toISOString(_42).substring(0,10);
if(this.value[_45]){
this.unselectDate(_42,_44);
}else{
this.selectDate(_42,_43);
}
},selectDate:function(_46,_47){
var _48=this._getNodeByDate(_46);
var _49=_48.className;
var _4a=_1.date.stamp.toISOString(_46).substring(0,10);
this.value[_4a]=1;
_47.push(_4a);
_49="dijitCalendarSelectedDate "+_49;
_48.className=_49;
},unselectDate:function(_4b,_4c){
var _4d=this._getNodeByDate(_4b);
var _4e=_4d.className;
var _4f=_1.date.stamp.toISOString(_4b).substring(0,10);
delete (this.value[_4f]);
_4c.push(_4f);
_4e=_4e.replace("dijitCalendarSelectedDate ","");
_4d.className=_4e;
},_getNodeByDate:function(_50){
var _51=new this.dateClassObj(this.listOfNodes[0].dijitDateValue);
var _52=Math.abs(_1.date.difference(_51,_50,"day"));
return this.listOfNodes[_52];
},_onDayClick:function(evt){
_1.stopEvent(evt);
for(var _53=evt.target;_53&&!_53.dijitDateValue;_53=_53.parentNode){
}
if(_53&&!_1.hasClass(_53,"dijitCalendarDisabledDate")){
value=new this.dateClassObj(_53.dijitDateValue);
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
var _54=_1.hasClass(evt.target,"dijitCalendarDateLabel")?evt.target.parentNode:evt.target;
if(_54&&(_54.dijitDateValue||_54==this.previousYearLabelNode||_54==this.nextYearLabelNode)){
_1.addClass(_54,"dijitCalendarHoveredDate");
this._currentNode=_54;
}
},_setEndRangeAttr:function(_55){
_55=new this.dateClassObj(_55);
_55.setHours(1);
this.endRange=_55;
},_getEndRangeAttr:function(){
var _56=new this.dateClassObj(this.endRange);
_56.setHours(0,0,0,0);
if(_56.getDate()<this.endRange.getDate()){
_56=this.dateFuncObj.add(_56,"hour",1);
}
return _56;
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
var _57=evt.target.parentNode;
if(_57&&_57.dijitDateValue){
_1.addClass(_57,"dijitCalendarActiveDate");
this._currentNode=_57;
}
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_57.dijitDateValue);
this._selectRange();
}else{
this.selectingRange=false;
this.previousRangeStart=null;
this.previousRangeEnd=null;
}
},_onDayMouseUp:function(evt){
var _58=evt.target.parentNode;
if(_58&&_58.dijitDateValue){
_1.removeClass(_58,"dijitCalendarActiveDate");
}
},handleKey:function(evt){
var dk=_1.keys,_59=-1,_5a,_5b=this.currentFocus;
switch(evt.keyCode){
case dk.RIGHT_ARROW:
_59=1;
case dk.LEFT_ARROW:
_5a="day";
if(!this.isLeftToRight()){
_59*=-1;
}
break;
case dk.DOWN_ARROW:
_59=1;
case dk.UP_ARROW:
_5a="week";
break;
case dk.PAGE_DOWN:
_59=1;
case dk.PAGE_UP:
_5a=evt.ctrlKey||evt.altKey?"year":"month";
break;
case dk.END:
_5b=this.dateFuncObj.add(_5b,"month",1);
_5a="day";
case dk.HOME:
_5b=new this.dateClassObj(_5b);
_5b.setDate(1);
break;
case dk.ENTER:
case dk.SPACE:
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_5b);
this._selectRange();
}else{
this.selectingRange=false;
this.toggleDate(_5b,[],[]);
this.previouslySelectedDay=_5b;
this.previousRangeStart=null;
this.previousRangeEnd=null;
this.onValueSelected([_1.date.stamp.toISOString(_5b).substring(0,10)]);
}
break;
default:
return true;
}
if(_5a){
_5b=this.dateFuncObj.add(_5b,_5a,_59);
}
this.set("currentFocus",_5b);
return false;
},_onKeyDown:function(evt){
if(!this.handleKey(evt)){
_1.stopEvent(evt);
}
},_removeFromRangeLTR:function(_5c,end,_5d,_5e){
difference=Math.abs(_1.date.difference(_5c,end,"day"));
for(var i=0;i<=difference;i++){
var _5f=_1.date.add(_5c,"day",i);
this.toggleDate(_5f,_5d,_5e);
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
this.previouslySelectedDay=_1.date.add(_5f,"day",1);
},_removeFromRangeRTL:function(_60,end,_61,_62){
difference=Math.abs(_1.date.difference(_60,end,"day"));
for(var i=0;i<=difference;i++){
var _63=_1.date.add(_60,"day",-i);
this.toggleDate(_63,_61,_62);
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
this.previouslySelectedDay=_1.date.add(_63,"day",-1);
},_addToRangeRTL:function(_64,end,_65,_66){
difference=Math.abs(_1.date.difference(_64,end,"day"));
for(var i=1;i<=difference;i++){
var _67=_1.date.add(_64,"day",-i);
this.toggleDate(_67,_65,_66);
}
if(this.previousRangeStart==null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")<0){
this.previousRangeStart=end;
}
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=_64;
}else{
if(_1.date.compare(_64,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=_64;
}
}
this.previouslySelectedDay=_67;
},_addToRangeLTR:function(_68,end,_69,_6a){
difference=Math.abs(_1.date.difference(_68,end,"day"));
for(var i=1;i<=difference;i++){
var _6b=_1.date.add(_68,"day",i);
this.toggleDate(_6b,_69,_6a);
}
if(this.previousRangeStart==null){
this.previousRangeStart=_68;
}else{
if(_1.date.compare(_68,this.previousRangeStart,"date")<0){
this.previousRangeStart=_68;
}
}
if(this.previousRangeEnd==null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=end;
}
}
this.previouslySelectedDay=_6b;
},_selectRange:function(){
var _6c=[];
var _6d=[];
var _6e=this.previouslySelectedDay;
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
if(_1.date.compare(end,_6e,"date")<0){
this._removeFromRangeRTL(_6e,end,_6c,_6d);
}else{
this._removeFromRangeLTR(_6e,end,_6c,_6d);
}
}else{
if(_1.date.compare(end,_6e,"date")<0){
this._addToRangeRTL(_6e,end,_6c,_6d);
}else{
this._addToRangeLTR(_6e,end,_6c,_6d);
}
}
if(_6c.length>0){
this.onValueSelected(_6c);
}
if(_6d.length>0){
this.onValueUnselected(_6d);
}
this.rangeJustSelected=true;
},onValueSelected:function(_6f){
},onValueUnselected:function(_70){
},onChange:function(_71){
},_isSelectedDate:function(_72,_73){
dateIndex=_1.date.stamp.toISOString(_72).substring(0,10);
return this.value[dateIndex];
},isDisabledDate:function(_74,_75){
},getClassForDate:function(_76,_77){
},_sort:function(){
if(this.value=={}){
return [];
}
var _78=[];
for(var _79 in this.value){
_78.push(_79);
}
_78.sort(function(a,b){
var _7a=new Date(a),_7b=new Date(b);
return _7a-_7b;
});
return _78;
},_returnDatesWithIsoRanges:function(_7c){
var _7d=[];
if(_7c.length>1){
var _7e=false,_7f=0,_80=null,_81=null,_82=_1.date.stamp.fromISOString(_7c[0]);
for(var i=1;i<_7c.length+1;i++){
currentDate=_1.date.stamp.fromISOString(_7c[i]);
if(_7e){
difference=Math.abs(_1.date.difference(_82,currentDate,"day"));
if(difference==1){
_81=currentDate;
}else{
range=_1.date.stamp.toISOString(_80).substring(0,10)+"/"+_1.date.stamp.toISOString(_81).substring(0,10);
_7d.push(range);
_7e=false;
}
}else{
difference=Math.abs(_1.date.difference(_82,currentDate,"day"));
if(difference==1){
_7e=true;
_80=_82;
_81=currentDate;
}else{
_7d.push(_1.date.stamp.toISOString(_82).substring(0,10));
}
}
_82=currentDate;
}
return _7d;
}else{
return _7c;
}
}});
_1.declare("dojox.widget._MonthDropDown",[_7,_8,_9],{months:[],templateString:"<div class='dijitCalendarMonthMenu dijitMenu' "+"dojoAttachEvent='onclick:_onClick,onmouseover:_onMenuHover,onmouseout:_onMenuHover'></div>",_setMonthsAttr:function(_83){
this.domNode.innerHTML=_1.map(_83,function(_84,idx){
return _84?"<div class='dijitCalendarMonthLabel' month='"+idx+"'>"+_84+"</div>":"";
}).join("");
},_onClick:function(evt){
this.onChange(_1.attr(evt.target,"month"));
},onChange:function(_85){
},_onMenuHover:function(evt){
_1.toggleClass(evt.target,"dijitCalendarMonthLabelHover",evt.type=="mouseover");
}});
return dojox.widget.MultiSelectCalendar;
});
