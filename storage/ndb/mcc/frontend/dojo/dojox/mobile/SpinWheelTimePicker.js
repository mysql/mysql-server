//>>built
define("dojox/mobile/SpinWheelTimePicker",["dojo/_base/declare","dojo/dom-class","./_TimePickerMixin","./SpinWheel","./SpinWheelSlot"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.SpinWheelTimePicker",[_4,_3],{slotClasses:[_5,_5],slotProps:[{labelFrom:0,labelTo:23,style:{width:"50px",textAlign:"right"}},{labelFrom:0,labelTo:59,zeroPad:2,style:{width:"40px",textAlign:"right"}}],buildRendering:function(){
this.inherited(arguments);
_2.add(this.domNode,"mblSpinWheelTimePicker");
}});
});
