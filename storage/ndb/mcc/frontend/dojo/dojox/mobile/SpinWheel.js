//>>built
define("dojox/mobile/SpinWheel",["dojo/_base/declare","dojo/dom-construct","./_PickerBase","./SpinWheelSlot"],function(_1,_2,_3){
return _1("dojox.mobile.SpinWheel",_3,{baseClass:"mblSpinWheel",buildRendering:function(){
this.inherited(arguments);
_2.create("div",{className:"mblSpinWheelBar"},this.domNode);
},startup:function(){
if(this._started){
return;
}
this.centerPos=Math.round(this.domNode.offsetHeight/2);
this.inherited(arguments);
},addChild:function(_4,_5){
this.inherited(arguments);
if(this._started){
_4.setInitialValue();
}
}});
});
