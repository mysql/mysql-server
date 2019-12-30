//>>built
define("dojox/mobile/_PickerBase",["dojo/_base/array","dojo/_base/declare","dijit/_Contained","dijit/_Container","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5){
return _2("dojox.mobile._PickerBase",[_5,_4,_3],{slotClasses:[],slotProps:[],slotOrder:[],buildRendering:function(){
this.inherited(arguments);
this.slots=[];
for(var i=0;i<this.slotClasses.length;i++){
var _6=this.slotOrder.length?this.slotOrder[i]:i;
var _7=new this.slotClasses[_6](this.slotProps[_6]);
this.addChild(_7);
this.slots[_6]=_7;
}
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
this.reset();
},getSlots:function(){
return this.slots.length?this.slots:_1.filter(this.getChildren(),function(c){
return c.declaredClass.indexOf("Slot")!==-1;
});
},_getValuesAttr:function(){
return _1.map(this.getSlots(),function(w){
return w.get("value");
});
},_setValuesAttr:function(a){
_1.forEach(this.getSlots(),function(w,i){
w.set("value",a[i]);
});
},_setColorsAttr:function(a){
_1.forEach(this.getSlots(),function(w,i){
w.setColor&&w.setColor(a[i]);
});
},reset:function(){
_1.forEach(this.getSlots(),function(w){
w.setInitialValue();
});
}});
});
