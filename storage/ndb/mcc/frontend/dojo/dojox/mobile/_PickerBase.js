//>>built
define("dojox/mobile/_PickerBase",["dojo/_base/array","dojo/_base/declare","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","dojo/has","dojo/has!dojo-bidi?dojox/mobile/bidi/_PickerBase"],function(_1,_2,_3,_4,_5,_6,_7){
var _8=_2(_6("dojo-bidi")?"dojox.mobile.NonBidi_PickerBase":"dojox.mobile._PickerBase",[_5,_4,_3],{slotClasses:[],slotProps:[],slotOrder:[],buildRendering:function(){
this.inherited(arguments);
this.slots=[];
for(var i=0;i<this.slotClasses.length;i++){
var _9=this.slotOrder.length?this.slotOrder[i]:i;
var _a=new this.slotClasses[_9](this.slotProps[_9]);
this.addChild(_a);
this.slots[_9]=_a;
}
},startup:function(){
if(this._started){
return;
}
this._duringStartup=true;
this.inherited(arguments);
this.reset();
delete this._duringStartup;
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
return _6("dojo-bidi")?_2("dojox.mobile._PickerBase",[_8,_7]):_8;
});
