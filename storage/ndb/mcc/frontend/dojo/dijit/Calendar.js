//>>built
define("dijit/Calendar",["dojo/_base/array","dojo/date","dojo/date/locale","dojo/_base/declare","dojo/dom-attr","dojo/dom-class","dojo/_base/event","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/sniff","./CalendarLite","./_Widget","./_CssStateMixin","./_TemplatedMixin","./form/DropDownButton"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_4("dijit.Calendar",[_c,_d,_e],{cssStateNodes:{"decrementMonth":"dijitCalendarArrow","incrementMonth":"dijitCalendarArrow","previousYearLabelNode":"dijitCalendarPreviousYear","nextYearLabelNode":"dijitCalendarNextYear"},setValue:function(_12){
_8.deprecated("dijit.Calendar:setValue() is deprecated.  Use set('value', ...) instead.","","2.0");
this.set("value",_12);
},_createMonthWidget:function(){
return new _11._MonthDropDownButton({id:this.id+"_mddb",tabIndex:-1,onMonthSelect:_a.hitch(this,"_onMonthSelect"),lang:this.lang,dateLocaleModule:this.dateLocaleModule},this.monthNode);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.domNode,"onkeydown","_onKeyDown");
this.connect(this.dateRowsNode,"onmouseover","_onDayMouseOver");
this.connect(this.dateRowsNode,"onmouseout","_onDayMouseOut");
this.connect(this.dateRowsNode,"onmousedown","_onDayMouseDown");
this.connect(this.dateRowsNode,"onmouseup","_onDayMouseUp");
},_onMonthSelect:function(_13){
var _14=new this.dateClassObj(this.currentFocus);
_14.setDate(1);
_14.setMonth(_13);
var _15=this.dateModule.getDaysInMonth(_14);
var _16=this.currentFocus.getDate();
_14.setDate(Math.min(_16,_15));
this._setCurrentFocusAttr(_14);
},_onDayMouseOver:function(evt){
var _17=_6.contains(evt.target,"dijitCalendarDateLabel")?evt.target.parentNode:evt.target;
if(_17&&((_17.dijitDateValue&&!_6.contains(_17,"dijitCalendarDisabledDate"))||_17==this.previousYearLabelNode||_17==this.nextYearLabelNode)){
_6.add(_17,"dijitCalendarHoveredDate");
this._currentNode=_17;
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
var _18=evt.target.parentNode;
if(_18&&_18.dijitDateValue&&!_6.contains(_18,"dijitCalendarDisabledDate")){
_6.add(_18,"dijitCalendarActiveDate");
this._currentNode=_18;
}
},_onDayMouseUp:function(evt){
var _19=evt.target.parentNode;
if(_19&&_19.dijitDateValue){
_6.remove(_19,"dijitCalendarActiveDate");
}
},handleKey:function(evt){
var _1a=-1,_1b,_1c=this.currentFocus;
switch(evt.keyCode){
case _9.RIGHT_ARROW:
_1a=1;
case _9.LEFT_ARROW:
_1b="day";
if(!this.isLeftToRight()){
_1a*=-1;
}
break;
case _9.DOWN_ARROW:
_1a=1;
case _9.UP_ARROW:
_1b="week";
break;
case _9.PAGE_DOWN:
_1a=1;
case _9.PAGE_UP:
_1b=evt.ctrlKey||evt.altKey?"year":"month";
break;
case _9.END:
_1c=this.dateModule.add(_1c,"month",1);
_1b="day";
case _9.HOME:
_1c=new this.dateClassObj(_1c);
_1c.setDate(1);
break;
case _9.ENTER:
case _9.SPACE:
this.set("value",this.currentFocus);
break;
default:
return true;
}
if(_1b){
_1c=this.dateModule.add(_1c,_1b,_1a);
}
this._setCurrentFocusAttr(_1c);
return false;
},_onKeyDown:function(evt){
if(!this.handleKey(evt)){
_7.stop(evt);
}
},onValueSelected:function(){
},onChange:function(_1d){
this.onValueSelected(_1d);
},getClassForDate:function(){
}});
_11._MonthDropDownButton=_4("dijit.Calendar._MonthDropDownButton",_10,{onMonthSelect:function(){
},postCreate:function(){
this.inherited(arguments);
this.dropDown=new _11._MonthDropDown({id:this.id+"_mdd",onChange:this.onMonthSelect});
},_setMonthAttr:function(_1e){
var _1f=this.dateLocaleModule.getNames("months","wide","standAlone",this.lang,_1e);
this.dropDown.set("months",_1f);
this.containerNode.innerHTML=(_b("ie")==6?"":"<div class='dijitSpacer'>"+this.dropDown.domNode.innerHTML+"</div>")+"<div class='dijitCalendarMonthLabel dijitCalendarCurrentMonthLabel'>"+_1f[_1e.getMonth()]+"</div>";
}});
_11._MonthDropDown=_4("dijit.Calendar._MonthDropDown",[_d,_f],{months:[],templateString:"<div class='dijitCalendarMonthMenu dijitMenu' "+"data-dojo-attach-event='onclick:_onClick,onmouseover:_onMenuHover,onmouseout:_onMenuHover'></div>",_setMonthsAttr:function(_20){
this.domNode.innerHTML=_1.map(_20,function(_21,idx){
return _21?"<div class='dijitCalendarMonthLabel' month='"+idx+"'>"+_21+"</div>":"";
}).join("");
},_onClick:function(evt){
this.onChange(_5.get(evt.target,"month"));
},onChange:function(){
},_onMenuHover:function(evt){
_6.toggle(evt.target,"dijitCalendarMonthLabelHover",evt.type=="mouseover");
}});
return _11;
});
