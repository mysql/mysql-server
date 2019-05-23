//>>built
define("dojox/mobile/ValuePickerDatePicker",["dojo/_base/declare","dojo/dom-class","./_DatePickerMixin","./ValuePicker","./ValuePickerSlot"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.ValuePickerDatePicker",[_4,_3],{readOnly:false,slotClasses:[_5,_5,_5],slotProps:[{labelFrom:1970,labelTo:2038,style:{width:"87px"}},{style:{width:"72px"}},{style:{width:"72px"}}],buildRendering:function(){
var p=this.slotProps;
p[0].readOnly=p[1].readOnly=p[2].readOnly=this.readOnly;
this.initSlots();
this.inherited(arguments);
_2.add(this.domNode,"mblValuePickerDatePicker");
this._conn=[this.connect(this.slots[0],"_setValueAttr","onYearSet"),this.connect(this.slots[1],"_setValueAttr","onMonthSet"),this.connect(this.slots[2],"_setValueAttr","onDaySet")];
},disableValues:function(_6){
var _7=this.slots[2].items;
if(this._tail){
this.slots[2].items=_7=_7.concat(this._tail);
}
this._tail=_7.slice(_6);
_7.splice(_6);
}});
});
