//>>built
define("dojox/form/YearTextBox",["dojo/_base/kernel","dojo/_base/lang","dojox/widget/YearlyCalendar","dijit/form/TextBox","./DateTextBox","dojo/_base/declare"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox/form/DateTextBox");
return _6("dojox.form.YearTextBox",_5,{popupClass:_3,format:function(_7){
if(typeof _7=="string"){
return _7;
}else{
if(_7.getFullYear){
return _7.getFullYear();
}
}
return _7;
},validator:function(_8){
return _8==""||_8==null||/(^-?\d\d*$)/.test(String(_8));
},_setValueAttr:function(_9,_a,_b){
if(_9){
if(_9.getFullYear){
_9=_9.getFullYear();
}
}
_4.prototype._setValueAttr.call(this,_9,_a,_b);
},openDropDown:function(){
this.inherited(arguments);
this.dropDown.onValueSelected=_2.hitch(this,function(_c){
this.focus();
setTimeout(_2.hitch(this,"closeDropDown"),1);
_4.prototype._setValueAttr.call(this,_c,true,_c);
});
},parse:function(_d,_e){
return _d||(this._isEmpty(_d)?null:undefined);
},filter:function(_f){
if(_f&&_f.getFullYear){
return _f.getFullYear().toString();
}
return this.inherited(arguments);
}});
});
