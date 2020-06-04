//>>built
require({cache:{"url:dojox/widget/MultiSelectCalendar/MultiSelectCalendar.html":"<table cellspacing=\"0\" cellpadding=\"0\" class=\"dijitCalendarContainer\" role=\"grid\" dojoAttachEvent=\"onkeydown: _onKeyDown\" aria-labelledby=\"${id}_year\">\n\t<thead>\n\t\t<tr class=\"dijitReset dijitCalendarMonthContainer\" valign=\"top\">\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"decrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarDecrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"decreaseArrowNode\" class=\"dijitA11ySideArrow\">-</span>\n\t\t\t</th>\n\t\t\t<th class='dijitReset' colspan=\"5\">\n\t\t\t\t<div dojoType=\"dijit.form.DropDownButton\" dojoAttachPoint=\"monthDropDownButton\"\n\t\t\t\t\tid=\"${id}_mddb\" tabIndex=\"-1\">\n\t\t\t\t</div>\n\t\t\t</th>\n\t\t\t<th class='dijitReset dijitCalendarArrow' dojoAttachPoint=\"incrementMonth\">\n\t\t\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitCalendarIncrementControl dijitCalendarIncrease\" role=\"presentation\"/>\n\t\t\t\t<span dojoAttachPoint=\"increaseArrowNode\" class=\"dijitA11ySideArrow\">+</span>\n\t\t\t</th>\n\t\t</tr>\n\t\t<tr>\n\t\t\t<th class=\"dijitReset dijitCalendarDayLabelTemplate\" role=\"columnheader\"><span class=\"dijitCalendarDayLabel\"></span></th>\n\t\t</tr>\n\t</thead>\n\t<tbody dojoAttachEvent=\"onclick: _onDayClick, onmouseover: _onDayMouseOver, onmouseout: _onDayMouseOut, onmousedown: _onDayMouseDown, onmouseup: _onDayMouseUp\" class=\"dijitReset dijitCalendarBodyContainer\">\n\t\t<tr class=\"dijitReset dijitCalendarWeekTemplate\" role=\"row\">\n\t\t\t<td class=\"dijitReset dijitCalendarDateTemplate\" role=\"gridcell\"><span class=\"dijitCalendarDateLabel\"></span></td>\n\t\t</tr>\n\t</tbody>\n\t<tfoot class=\"dijitReset dijitCalendarYearContainer\">\n\t\t<tr>\n\t\t\t<td class='dijitReset' valign=\"top\" colspan=\"7\">\n\t\t\t\t<h3 class=\"dijitCalendarYearLabel\">\n\t\t\t\t\t<span dojoAttachPoint=\"previousYearLabelNode\" class=\"dijitInline dijitCalendarPreviousYear\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"currentYearLabelNode\" class=\"dijitInline dijitCalendarSelectedYear\" id=\"${id}_year\"></span>\n\t\t\t\t\t<span dojoAttachPoint=\"nextYearLabelNode\" class=\"dijitInline dijitCalendarNextYear\"></span>\n\t\t\t\t</h3>\n\t\t\t</td>\n\t\t</tr>\n\t</tfoot>\n</table>"}});
define("dojox/widget/MultiSelectCalendar",["dojo/main","dijit","dojo/text!./MultiSelectCalendar/MultiSelectCalendar.html","dojo/cldr/supplemental","dojo/date","dojo/date/stamp","dojo/date/locale","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/_CssStateMixin","dijit/form/DropDownButton","dijit/typematic"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
_1.experimental("dojox.widget.MultiSelectCalendar");
var _e=_1.declare("dojox.widget.MultiSelectCalendar",[_8,_9,_a,_b],{templateString:_3,widgetsInTemplate:true,value:{},datePackage:"dojo.date",dayWidth:"narrow",tabIndex:"0",returnIsoRanges:false,currentFocus:new Date(),baseClass:"dijitCalendar",cssStateNodes:{"decrementMonth":"dijitCalendarArrow","incrementMonth":"dijitCalendarArrow","previousYearLabelNode":"dijitCalendarPreviousYear","nextYearLabelNode":"dijitCalendarNextYear"},_areValidDates:function(_f){
for(var _10 in this.value){
valid=(_10&&!isNaN(_10)&&typeof _f=="object"&&_10.toString()!=this.constructor.prototype.value.toString());
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
},_setValueAttr:function(_11,_12){
this.value={};
if(_1.isArray(_11)){
_1.forEach(_11,function(_13,i){
var _14=_13.indexOf("/");
if(_14==-1){
this.value[_13]=1;
}else{
var _15=_6.fromISOString(_13.substr(0,10));
var _16=_6.fromISOString(_13.substr(11,10));
this.toggleDate(_15,[],[]);
if((_15-_16)>0){
this._addToRangeRTL(_15,_16,[],[]);
}else{
this._addToRangeLTR(_15,_16,[],[]);
}
}
},this);
if(_11.length>0){
this.focusOnLastDate(_11[_11.length-1]);
}
}else{
if(_11){
_11=new this.dateClassObj(_11);
}
if(this._isValidDate(_11)){
_11.setHours(1,0,0,0);
if(!this.isDisabledDate(_11,this.lang)){
dateIndex=_6.toISOString(_11).substring(0,10);
this.value[dateIndex]=1;
this.set("currentFocus",_11);
if(_12||typeof _12=="undefined"){
this.onChange(this.get("value"));
this.onValueSelected(this.get("value"));
}
}
}
}
this._populateGrid();
},focusOnLastDate:function(_17){
var _18=_17.indexOf("/");
var _19,_1a;
if(_18==-1){
lastDate=_6.fromISOString(_17);
}else{
_19=_6.fromISOString(_17.substr(0,10));
_1a=_6.fromISOString(_17.substr(11,10));
if((_19-_1a)>0){
lastDate=_19;
}else{
lastDate=_1a;
}
}
this.set("currentFocus",lastDate);
},_isValidDate:function(_1b){
return _1b&&!isNaN(_1b)&&typeof _1b=="object"&&_1b.toString()!=this.constructor.prototype.value.toString();
},_setText:function(_1c,_1d){
while(_1c.firstChild){
_1c.removeChild(_1c.firstChild);
}
_1c.appendChild(_1.doc.createTextNode(_1d));
},_populateGrid:function(){
var _1e=new this.dateClassObj(this.currentFocus);
_1e.setDate(1);
var _1f=_1e.getDay(),_20=this.dateFuncObj.getDaysInMonth(_1e),_21=this.dateFuncObj.getDaysInMonth(this.dateFuncObj.add(_1e,"month",-1)),_22=new this.dateClassObj(),_23=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
if(_23>_1f){
_23-=7;
}
this.listOfNodes=_1.query(".dijitCalendarDateTemplate",this.domNode);
this.listOfNodes.forEach(function(_24,i){
i+=_23;
var _25=new this.dateClassObj(_1e),_26,_27="dijitCalendar",adj=0;
if(i<_1f){
_26=_21-_1f+i+1;
adj=-1;
_27+="Previous";
}else{
if(i>=(_1f+_20)){
_26=i-_1f-_20+1;
adj=1;
_27+="Next";
}else{
_26=i-_1f+1;
_27+="Current";
}
}
if(adj){
_25=this.dateFuncObj.add(_25,"month",adj);
}
_25.setDate(_26);
if(!this.dateFuncObj.compare(_25,_22,"date")){
_27="dijitCalendarCurrentDate "+_27;
}
dateIndex=_6.toISOString(_25).substring(0,10);
if(!this.isDisabledDate(_25,this.lang)){
if(this._isSelectedDate(_25,this.lang)){
if(this.value[dateIndex]){
_27="dijitCalendarSelectedDate "+_27;
}else{
_27=_27.replace("dijitCalendarSelectedDate ","");
}
}
}
if(this._isSelectedDate(_25,this.lang)){
_27="dijitCalendarBrowsingDate "+_27;
}
if(this.isDisabledDate(_25,this.lang)){
_27="dijitCalendarDisabledDate "+_27;
}
var _28=this.getClassForDate(_25,this.lang);
if(_28){
_27=_28+" "+_27;
}
_24.className=_27+"Month dijitCalendarDateTemplate";
_24.dijitDateValue=_25.valueOf();
_1.attr(_24,"dijitDateValue",_25.valueOf());
var _29=_1.query(".dijitCalendarDateLabel",_24)[0],_2a=_25.getDateLocalized?_25.getDateLocalized(this.lang):_25.getDate();
this._setText(_29,_2a);
},this);
var _2b=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_1e);
this.monthDropDownButton.dropDown.set("months",_2b);
this.monthDropDownButton.containerNode.innerHTML=(_1.isIE==6?"":"<div class='dijitSpacer'>"+this.monthDropDownButton.dropDown.domNode.innerHTML+"</div>")+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_2b[_1e.getMonth()]+"</div>";
var y=_1e.getFullYear()-1;
var d=new this.dateClassObj();
_1.forEach(["previous","current","next"],function(_2c){
d.setFullYear(y++);
this._setText(this[_2c+"YearLabelNode"],this.dateLocaleModule.format(d,{selector:"year",locale:this.lang}));
},this);
},goToToday:function(){
this.set("currentFocus",new this.dateClassObj(),false);
},constructor:function(_2d){
var _2e=(_2d.datePackage&&(_2d.datePackage!="dojo.date"))?_2d.datePackage+".Date":"Date";
this.dateClassObj=_1.getObject(_2e,false);
this.datePackage=_2d.datePackage||this.datePackage;
this.dateFuncObj=_1.getObject(this.datePackage,false);
this.dateLocaleModule=_1.getObject(this.datePackage+".locale",false);
},buildRendering:function(){
this.inherited(arguments);
_1.setSelectable(this.domNode,false);
var _2f=_1.hitch(this,function(_30,n){
var _31=_1.query(_30,this.domNode)[0];
for(var i=0;i<n;i++){
_31.parentNode.appendChild(_31.cloneNode(true));
}
});
_2f(".dijitCalendarDayLabelTemplate",6);
_2f(".dijitCalendarDateTemplate",6);
_2f(".dijitCalendarWeekTemplate",5);
var _32=this.dateLocaleModule.getNames("days",this.dayWidth,"standAlone",this.lang);
var _33=_1.cldr.supplemental.getFirstDayOfWeek(this.lang);
_1.query(".dijitCalendarDayLabel",this.domNode).forEach(function(_34,i){
this._setText(_34,_32[(i+_33)%7]);
},this);
var _35=new this.dateClassObj(this.currentFocus);
this.monthDropDownButton.dropDown=new _85({id:this.id+"_mdd",onChange:_1.hitch(this,"_onMonthSelect")});
this.set("currentFocus",_35,false);
var _36=this;
var _37=function(_38,_39,adj){
_36._connects.push(_2.typematic.addMouseListener(_36[_38],_36,function(_3a){
if(_3a>=0){
_36._adjustDisplay(_39,adj);
}
},0.8,500));
};
_37("incrementMonth","month",1);
_37("decrementMonth","month",-1);
_37("nextYearLabelNode","year",1);
_37("previousYearLabelNode","year",-1);
},_adjustDisplay:function(_3b,_3c){
this._setCurrentFocusAttr(this.dateFuncObj.add(this.currentFocus,_3b,_3c));
},_setCurrentFocusAttr:function(_3d,_3e){
var _3f=this.currentFocus,_40=_3f?_1.query("[dijitDateValue="+_3f.valueOf()+"]",this.domNode)[0]:null;
_3d=new this.dateClassObj(_3d);
_3d.setHours(1,0,0,0);
this._set("currentFocus",_3d);
var _41=_6.toISOString(_3d).substring(0,7);
if(_41!=this.previousMonth){
this._populateGrid();
this.previousMonth=_41;
}
var _42=_1.query("[dijitDateValue='"+_3d.valueOf()+"']",this.domNode)[0];
_42.setAttribute("tabIndex",this.tabIndex);
if(this._focused||_3e){
_42.focus();
}
if(_40&&_40!=_42){
if(_1.isWebKit){
_40.setAttribute("tabIndex","-1");
}else{
_40.removeAttribute("tabIndex");
}
}
},focus:function(){
this._setCurrentFocusAttr(this.currentFocus,true);
},_onMonthSelect:function(_43){
this.currentFocus=this.dateFuncObj.add(this.currentFocus,"month",_43-this.currentFocus.getMonth());
this._populateGrid();
},toggleDate:function(_44,_45,_46){
var _47=_6.toISOString(_44).substring(0,10);
if(this.value[_47]){
this.unselectDate(_44,_46);
}else{
this.selectDate(_44,_45);
}
},selectDate:function(_48,_49){
var _4a=this._getNodeByDate(_48);
var _4b=_4a.className;
var _4c=_6.toISOString(_48).substring(0,10);
this.value[_4c]=1;
_49.push(_4c);
_4b="dijitCalendarSelectedDate "+_4b;
_4a.className=_4b;
},unselectDate:function(_4d,_4e){
var _4f=this._getNodeByDate(_4d);
var _50=_4f.className;
var _51=_6.toISOString(_4d).substring(0,10);
delete (this.value[_51]);
_4e.push(_51);
_50=_50.replace("dijitCalendarSelectedDate ","");
_4f.className=_50;
},_getNodeByDate:function(_52){
var _53=new this.dateClassObj(this.listOfNodes[0].dijitDateValue);
var _54=Math.abs(_1.date.difference(_53,_52,"day"));
return this.listOfNodes[_54];
},_onDayClick:function(evt){
_1.stopEvent(evt);
for(var _55=evt.target;_55&&!_55.dijitDateValue;_55=_55.parentNode){
}
if(_55&&!_1.hasClass(_55,"dijitCalendarDisabledDate")){
value=new this.dateClassObj(_55.dijitDateValue);
if(!this.rangeJustSelected){
this.toggleDate(value,[],[]);
this.previouslySelectedDay=value;
this.set("currentFocus",value);
this.onValueSelected([_6.toISOString(value).substring(0,10)]);
}else{
this.rangeJustSelected=false;
this.set("currentFocus",value);
}
}
},_onDayMouseOver:function(evt){
var _56=_1.hasClass(evt.target,"dijitCalendarDateLabel")?evt.target.parentNode:evt.target;
if(_56&&(_56.dijitDateValue||_56==this.previousYearLabelNode||_56==this.nextYearLabelNode)){
_1.addClass(_56,"dijitCalendarHoveredDate");
this._currentNode=_56;
}
},_setEndRangeAttr:function(_57){
_57=new this.dateClassObj(_57);
_57.setHours(1);
this.endRange=_57;
},_getEndRangeAttr:function(){
var _58=new this.dateClassObj(this.endRange);
_58.setHours(0,0,0,0);
if(_58.getDate()<this.endRange.getDate()){
_58=this.dateFuncObj.add(_58,"hour",1);
}
return _58;
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
var _59=evt.target.parentNode;
if(_59&&_59.dijitDateValue){
_1.addClass(_59,"dijitCalendarActiveDate");
this._currentNode=_59;
}
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_59.dijitDateValue);
this._selectRange();
}else{
this.selectingRange=false;
this.previousRangeStart=null;
this.previousRangeEnd=null;
}
},_onDayMouseUp:function(evt){
var _5a=evt.target.parentNode;
if(_5a&&_5a.dijitDateValue){
_1.removeClass(_5a,"dijitCalendarActiveDate");
}
},handleKey:function(evt){
var dk=_1.keys,_5b=-1,_5c,_5d=this.currentFocus;
switch(evt.keyCode){
case dk.RIGHT_ARROW:
_5b=1;
case dk.LEFT_ARROW:
_5c="day";
if(!this.isLeftToRight()){
_5b*=-1;
}
break;
case dk.DOWN_ARROW:
_5b=1;
case dk.UP_ARROW:
_5c="week";
break;
case dk.PAGE_DOWN:
_5b=1;
case dk.PAGE_UP:
_5c=evt.ctrlKey||evt.altKey?"year":"month";
break;
case dk.END:
_5d=this.dateFuncObj.add(_5d,"month",1);
_5c="day";
case dk.HOME:
_5d=new this.dateClassObj(_5d);
_5d.setDate(1);
break;
case dk.ENTER:
case dk.SPACE:
if(evt.shiftKey&&this.previouslySelectedDay){
this.selectingRange=true;
this.set("endRange",_5d);
this._selectRange();
}else{
this.selectingRange=false;
this.toggleDate(_5d,[],[]);
this.previouslySelectedDay=_5d;
this.previousRangeStart=null;
this.previousRangeEnd=null;
this.onValueSelected([_6.toISOString(_5d).substring(0,10)]);
}
break;
default:
return true;
}
if(_5c){
_5d=this.dateFuncObj.add(_5d,_5c,_5b);
}
this.set("currentFocus",_5d);
return false;
},_onKeyDown:function(evt){
if(!this.handleKey(evt)){
_1.stopEvent(evt);
}
},_removeFromRangeLTR:function(_5e,end,_5f,_60){
difference=Math.abs(_1.date.difference(_5e,end,"day"));
for(var i=0;i<=difference;i++){
var _61=_5.add(_5e,"day",i);
this.toggleDate(_61,_5f,_60);
}
if(this.previousRangeEnd===null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=end;
}
}
if(this.previousRangeStart===null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")>0){
this.previousRangeStart=end;
}
}
this.previouslySelectedDay=_5.add(_61,"day",1);
},_removeFromRangeRTL:function(_62,end,_63,_64){
difference=Math.abs(_1.date.difference(_62,end,"day"));
for(var i=0;i<=difference;i++){
var _65=_1.date.add(_62,"day",-i);
this.toggleDate(_65,_63,_64);
}
if(this.previousRangeEnd===null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")<0){
this.previousRangeEnd=end;
}
}
if(this.previousRangeStart===null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")<0){
this.previousRangeStart=end;
}
}
this.previouslySelectedDay=_5.add(_65,"day",-1);
},_addToRangeRTL:function(_66,end,_67,_68){
difference=Math.abs(_5.difference(_66,end,"day"));
for(var i=1;i<=difference;i++){
var _69=_5.add(_66,"day",-i);
this.toggleDate(_69,_67,_68);
}
if(this.previousRangeStart===null){
this.previousRangeStart=end;
}else{
if(_1.date.compare(end,this.previousRangeStart,"date")<0){
this.previousRangeStart=end;
}
}
if(this.previousRangeEnd===null){
this.previousRangeEnd=_66;
}else{
if(_1.date.compare(_66,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=_66;
}
}
this.previouslySelectedDay=_69;
},_addToRangeLTR:function(_6a,end,_6b,_6c){
difference=Math.abs(_1.date.difference(_6a,end,"day"));
for(var i=1;i<=difference;i++){
var _6d=_1.date.add(_6a,"day",i);
this.toggleDate(_6d,_6b,_6c);
}
if(this.previousRangeStart===null){
this.previousRangeStart=_6a;
}else{
if(_1.date.compare(_6a,this.previousRangeStart,"date")<0){
this.previousRangeStart=_6a;
}
}
if(this.previousRangeEnd===null){
this.previousRangeEnd=end;
}else{
if(_1.date.compare(end,this.previousRangeEnd,"date")>0){
this.previousRangeEnd=end;
}
}
this.previouslySelectedDay=_6d;
},_selectRange:function(){
var _6e=[];
var _6f=[];
var _70=this.previouslySelectedDay;
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
if(removingFromRange===true){
if(_1.date.compare(end,_70,"date")<0){
this._removeFromRangeRTL(_70,end,_6e,_6f);
}else{
this._removeFromRangeLTR(_70,end,_6e,_6f);
}
}else{
if(_1.date.compare(end,_70,"date")<0){
this._addToRangeRTL(_70,end,_6e,_6f);
}else{
this._addToRangeLTR(_70,end,_6e,_6f);
}
}
if(_6e.length>0){
this.onValueSelected(_6e);
}
if(_6f.length>0){
this.onValueUnselected(_6f);
}
this.rangeJustSelected=true;
},onValueSelected:function(_71){
},onValueUnselected:function(_72){
},onChange:function(_73){
},_isSelectedDate:function(_74,_75){
dateIndex=_6.toISOString(_74).substring(0,10);
return this.value[dateIndex];
},isDisabledDate:function(_76,_77){
},getClassForDate:function(_78,_79){
},_sort:function(){
if(this.value=={}){
return [];
}
var _7a=[];
for(var _7b in this.value){
_7a.push(_7b);
}
_7a.sort(function(a,b){
var _7c=new Date(a),_7d=new Date(b);
return _7c-_7d;
});
return _7a;
},_returnDatesWithIsoRanges:function(_7e){
var _7f=[];
if(_7e.length>1){
var _80=false,_81=0,_82=null,_83=null,_84=_6.fromISOString(_7e[0]);
for(var i=1;i<_7e.length+1;i++){
currentDate=_6.fromISOString(_7e[i]);
if(_80){
difference=Math.abs(_5.difference(_84,currentDate,"day"));
if(difference==1){
_83=currentDate;
}else{
range=_6.toISOString(_82).substring(0,10)+"/"+_6.toISOString(_83).substring(0,10);
_7f.push(range);
_80=false;
}
}else{
difference=Math.abs(_5.difference(_84,currentDate,"day"));
if(difference==1){
_80=true;
_82=_84;
_83=currentDate;
}else{
_7f.push(_6.toISOString(_84).substring(0,10));
}
}
_84=currentDate;
}
return _7f;
}else{
return _7e;
}
}});
var _85=_e._MonthDropDown=_1.declare("dojox.widget._MonthDropDown",[_8,_9,_a],{months:[],templateString:"<div class='dijitCalendarMonthMenu dijitMenu' "+"dojoAttachEvent='onclick:_onClick,onmouseover:_onMenuHover,onmouseout:_onMenuHover'></div>",_setMonthsAttr:function(_86){
this.domNode.innerHTML=_1.map(_86,function(_87,idx){
return _87?"<div class='dijitCalendarMonthLabel' month='"+idx+"'>"+_87+"</div>":"";
}).join("");
},_onClick:function(evt){
this.onChange(_1.attr(evt.target,"month"));
},onChange:function(_88){
},_onMenuHover:function(evt){
_1.toggleClass(evt.target,"dijitCalendarMonthLabelHover",evt.type=="mouseover");
}});
return _e;
});
