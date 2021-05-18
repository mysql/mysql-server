//>>built
define("dojox/form/MonthTextBox",["dojo/_base/kernel","dojo/_base/lang","dojox/widget/MonthlyCalendar","dijit/form/TextBox","./DateTextBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox/form/DateTextBox");
return _6("dojox.form.MonthTextBox",_5,{popupClass:_3,selector:"date",postMixInProperties:function(){
this.inherited(arguments);
this.constraints.datePattern="MM";
},format:function(_7){
if(!_7&&_7!==0){
return 1;
}
if(_7.getMonth){
return _7.getMonth()+1;
}
return Number(_7)+1;
},parse:function(_8,_9){
return Number(_8)-1;
},serialize:function(_a,_b){
return String(_a);
},validator:function(_c){
var _d=Number(_c);
var _e=/(^-?\d\d*$)/.test(String(_c));
return _c==""||_c==null||(_e&&_d>=1&&_d<=12);
},_setValueAttr:function(_f,_10,_11){
if(_f){
if(_f.getMonth){
_f=_f.getMonth();
}
}
_4.prototype._setValueAttr.call(this,_f,_10,_11);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_12){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_4.prototype._setValueAttr.call(this,_12,true,_12);
});
}});
});
