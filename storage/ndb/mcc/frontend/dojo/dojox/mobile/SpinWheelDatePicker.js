//>>built
define("dojox/mobile/SpinWheelDatePicker",["dojo/_base/array","dojo/_base/declare","dojo/dom-class","./_DatePickerMixin","./SpinWheel","./SpinWheelSlot"],function(_1,_2,_3,_4,_5,_6){
return _2("dojox.mobile.SpinWheelDatePicker",[_5,_4],{slotClasses:[_6,_6,_6],slotProps:[{labelFrom:1970,labelTo:2038},{},{}],buildRendering:function(){
this.initSlots();
this.inherited(arguments);
_3.add(this.domNode,"mblSpinWheelDatePicker");
this._conn=[this.connect(this.slots[0],"onFlickAnimationEnd","_onYearSet"),this.connect(this.slots[1],"onFlickAnimationEnd","_onMonthSet"),this.connect(this.slots[2],"onFlickAnimationEnd","_onDaySet")];
},disableValues:function(_7){
_1.forEach(this.slots[2].panelNodes,function(_8){
for(var i=27;i<31;i++){
_3.toggle(_8.childNodes[i],"mblSpinWheelSlotLabelGray",i>=_7);
}
});
}});
});
