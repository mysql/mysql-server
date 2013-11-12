//>>built
define("dijit/Calendar",["dojo/_base/array","dojo/date","dojo/date/locale","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/_base/event","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/_base/sniff","./CalendarLite","./_Widget","./_CssStateMixin","./_TemplatedMixin","./form/DropDownButton","./hccss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_4("dijit.Calendar",[_c,_d,_e],{cssStateNodes:{"decrementMonth":"dijitCalendarArrow","incrementMonth":"dijitCalendarArrow","previousYearLabelNode":"dijitCalendarPreviousYear","nextYearLabelNode":"dijitCalendarNextYear"},setValue:function(_12){
_8.deprecated("dijit.Calendar:setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
this.set("value",_12);
},_createMonthWidget:function(){
return new _11._MonthDropDownButton({id:this.id+"_mddb",tabIndex:-1,onMonthSelect:_a.hitch(this,"_onMonthSelect"),lang:this.lang,dateLocaleModule:this.dateLocaleModule},this.monthNode);
},buildRendering:function(){
this.inherited(arguments);
this.connect(this.domNode,"onkeypress","_onKeyPress");
this.connect(this.dateRowsNode,"onmouseover","_onDayMouseOver");
this.connect(this.dateRowsNode,"onmouseout","_onDayMouseOut");
this.connect(this.dateRowsNode,"onmousedown","_onDayMouseDown");
this.connect(this.dateRowsNode,"onmouseup","_onDayMouseUp");
},_onMonthSelect:function(_13){
this._setCurrentFocusAttr(this.dateFuncObj.add(this.currentFocus,"month",_13-this.currentFocus.getMonth()));
},_onDayMouseOver:function(evt){
var _14=_6.contains(evt.target,"dijitCalendarDateLabel")?evt.target.parentNode:evt.target;
if(_14&&((_14.dijitDateValue&&!_6.contains(_14,"dijitCalendarDisabledDate"))||_14==this.previousYearLabelNode||_14==this.nextYearLabelNode)){
_6.add(_14,"dijitCalendarHoveredDate");
this._currentNode=_14;
}
},_onDayMouseOut:function(evt){
if(!this._currentNode){
return;
}
if(evt.relatedTarget&&evt.relatedTarget.parentNode==this._currentNode){
return;
}
var cls="dijitCalendarHoveredDate";
if(_6.contains(this._currentNode,"dijitCalendarActiveDate")){
cls+=" dijitCalendarActiveDate";
}
_6.remove(this._currentNode,cls);
this._currentNode=null;
},_onDayMouseDown:function(evt){
var _15=evt.target.parentNode;
if(_15&&_15.dijitDateValue&&!_6.contains(_15,"dijitCalendarDisabledDate")){
_6.add(_15,"dijitCalendarActiveDate");
this._currentNode=_15;
}
},_onDayMouseUp:function(evt){
var _16=evt.target.parentNode;
if(_16&&_16.dijitDateValue){
_6.remove(_16,"dijitCalendarActiveDate");
}
},handleKey:function(evt){
var _17=-1,_18,_19=this.currentFocus;
switch(evt.charOrCode){
case _9.RIGHT_ARROW:
_17=1;
case _9.LEFT_ARROW:
_18="day";
if(!this.isLeftToRight()){
_17*=-1;
}
break;
case _9.DOWN_ARROW:
_17=1;
case _9.UP_ARROW:
_18="week";
break;
case _9.PAGE_DOWN:
_17=1;
case _9.PAGE_UP:
_18=evt.ctrlKey||evt.altKey?"year":"month";
break;
case _9.END:
_19=this.dateFuncObj.add(_19,"month",1);
_18="day";
case _9.HOME:
_19=new this.dateClassObj(_19);
_19.setDate(1);
break;
case _9.ENTER:
case " ":
this.set("value",this.currentFocus);
break;
default:
return true;
}
if(_18){
_19=this.dateFuncObj.add(_19,_18,_17);
}
this._setCurrentFocusAttr(_19);
return false;
},_onKeyPress:function(evt){
if(!this.handleKey(evt)){
_7.stop(evt);
}
},onValueSelected:function(){
},onChange:function(_1a){
this.onValueSelected(_1a);
},getClassForDate:function(){
}});
_11._MonthDropDownButton=_4("dijit.Calendar._MonthDropDownButton",_10,{onMonthSelect:function(){
},postCreate:function(){
this.inherited(arguments);
this.dropDown=new _11._MonthDropDown({id:this.id+"_mdd",onChange:this.onMonthSelect});
},_setMonthAttr:function(_1b){
var _1c=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_1b);
this.dropDown.set("months",_1c);
this.containerNode.innerHTML=(_b("ie")==6?"":"<div class='dijitSpacer'>"+this.dropDown.domNode.innerHTML+"</div>")+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_1c[_1b.getMonth()]+"</div>";
}});
_11._MonthDropDown=_4("dijit.Calendar._MonthDropDown",[_d,_f],{months:[],templateString:"<div class='dijitCalendarMonthMenu dijitMenu' "+"data-dojo-attach-event='onclick:_onClick,onmouseover:_onMenuHover,onmouseout:_onMenuHover'></div>",_setMonthsAttr:function(_1d){
this.domNode.innerHTML=_1.map(_1d,function(_1e,idx){
return _1e?"<div class='dijitCalendarMonthLabel' month='"+idx+"'>"+_1e+"</div>":"";
}).join("");
},_onClick:function(evt){
this.onChange(_5.get(evt.target,"month"));
},onChange:function(){
},_onMenuHover:function(evt){
_6.toggle(evt.target,"dijitCalendarMonthLabelHover",evt.type=="mouseover");
}});
return _11;
});
