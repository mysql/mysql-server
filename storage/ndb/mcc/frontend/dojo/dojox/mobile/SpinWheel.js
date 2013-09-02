//>>built
define("dojox/mobile/SpinWheel",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/dom-class","dojo/dom-construct","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./SpinWheelSlot"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _2("dojox.mobile.SpinWheel",[_8,_7,_6],{slotClasses:[],slotProps:[],centerPos:0,buildRendering:function(){
this.inherited(arguments);
_4.add(this.domNode,"mblSpinWheel");
this.centerPos=Math.round(this.domNode.offsetHeight/2);
this.slots=[];
for(var i=0;i<this.slotClasses.length;i++){
this.slots.push(((typeof this.slotClasses[i]=="string")?_3.getObject(this.slotClasses[i]):this.slotClasses[i])(this.slotProps[i]));
this.addChild(this.slots[i]);
}
_5.create("DIV",{className:"mblSpinWheelBar"},this.domNode);
},startup:function(){
this.inherited(arguments);
this.reset();
},getValue:function(){
var a=[];
_1.forEach(this.getChildren(),function(w){
if(w instanceof _9){
a.push(w.getValue());
}
},this);
return a;
},setValue:function(a){
var i=0;
_1.forEach(this.getChildren(),function(w){
if(w instanceof _9){
w.setValue(a[i]);
w.setColor(a[i]);
i++;
}
},this);
},reset:function(){
_1.forEach(this.getChildren(),function(w){
if(w instanceof _9){
w.setInitialValue();
}
},this);
}});
});
