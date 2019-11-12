//>>built
define("dojox/form/DayTextBox",["dojo/_base/kernel","dojo/_base/lang","dojox/widget/DailyCalendar","dijit/form/TextBox","./DateTextBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox/form/DayTextBox");
return _6("dojox.form.DayTextBox",_5,{popupClass:_3,parse:function(_7){
return _7;
},format:function(_8){
return _8.getDate?_8.getDate():_8;
},validator:function(_9){
var _a=Number(_9);
var _b=/(^-?\d\d*$)/.test(String(_9));
return _9==""||_9==null||(_b&&_a>=1&&_a<=31);
},_setValueAttr:function(_c,_d,_e){
if(_c){
if(_c.getDate){
_c=_c.getDate();
}
}
_4.prototype._setValueAttr.call(this,_c,_d,_e);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_f){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_4.prototype._setValueAttr.call(this,String(_f.getDate()),true,String(_f.getDate()));
});
}});
});
