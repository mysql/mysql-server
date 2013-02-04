//>>built
define("dojox/form/DateTextBox",["dojo/_base/kernel","dojo/_base/lang","dojo/dom-style","dojox/widget/Calendar","dojox/widget/CalendarViews","dijit/form/_DateTimeTextBox","dijit/form/TextBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_1.experimental("dojox.form.DateTextBox");
var _9=_8("dojox.form.DateTextBox",_6,{popupClass:"dojox.widget.Calendar",_selector:"date",openDropDown:function(){
this.inherited(arguments);
_3.set(this.dropDown.domNode.parentNode,"position","absolute");
}});
_8("dojox.form.DayTextBox",_9,{popupClass:"dojox.widget.DailyCalendar",parse:function(_a){
return _a;
},format:function(_b){
return _b.getDate?_b.getDate():_b;
},validator:function(_c){
var _d=Number(_c);
var _e=/(^-?\d\d*$)/.test(String(_c));
return _c==""||_c==null||(_e&&_d>=1&&_d<=31);
},_setValueAttr:function(_f,_10,_11){
if(_f){
if(_f.getDate){
_f=_f.getDate();
}
}
_7.prototype._setValueAttr.call(this,_f,_10,_11);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_12){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_7.prototype._setValueAttr.call(this,String(_12.getDate()),true,String(_12.getDate()));
});
}});
_8("dojox.form.MonthTextBox",_9,{popupClass:"dojox.widget.MonthlyCalendar",selector:"date",postMixInProperties:function(){
this.inherited(arguments);
this.constraints.datePattern="MM";
},format:function(_13){
if(!_13&&_13!==0){
return 1;
}
if(_13.getMonth){
return _13.getMonth()+1;
}
return Number(_13)+1;
},parse:function(_14,_15){
return Number(_14)-1;
},serialize:function(_16,_17){
return String(_16);
},validator:function(_18){
var num=Number(_18);
var _19=/(^-?\d\d*$)/.test(String(_18));
return _18==""||_18==null||(_19&&num>=1&&num<=12);
},_setValueAttr:function(_1a,_1b,_1c){
if(_1a){
if(_1a.getMonth){
_1a=_1a.getMonth();
}
}
_7.prototype._setValueAttr.call(this,_1a,_1b,_1c);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_1d){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_7.prototype._setValueAttr.call(this,_1d,true,_1d);
});
}});
_8("dojox.form.YearTextBox",_9,{popupClass:"dojox.widget.YearlyCalendar",format:function(_1e){
if(typeof _1e=="string"){
return _1e;
}else{
if(_1e.getFullYear){
return _1e.getFullYear();
}
}
return _1e;
},validator:function(_1f){
return _1f==""||_1f==null||/(^-?\d\d*$)/.test(String(_1f));
},_setValueAttr:function(_20,_21,_22){
if(_20){
if(_20.getFullYear){
_20=_20.getFullYear();
}
}
_7.prototype._setValueAttr.call(this,_20,_21,_22);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_23){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_7.prototype._setValueAttr.call(this,_23,true,_23);
});
},parse:function(_24,_25){
return _24||(this._isEmpty(_24)?null:undefined);
},filter:function(val){
if(val&&val.getFullYear){
return val.getFullYear().toString();
}
return this.inherited(arguments);
}});
return _9;
});
