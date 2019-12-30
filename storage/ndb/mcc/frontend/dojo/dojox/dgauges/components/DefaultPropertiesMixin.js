//>>built
define("dojox/dgauges/components/DefaultPropertiesMixin",["dojo/_base/declare","dojo/_base/Color"],function(_1,_2){
return _1("dojox.dgauges.components.DefaultPropertiesMixin",null,{minimum:0,maximum:100,snapInterval:1,majorTickInterval:NaN,minorTickInterval:NaN,minorTicksEnabled:true,value:0,interactionArea:"gauge",interactionMode:"mouse",animationDuration:0,_setMinimumAttr:function(v){
this.getElement("scale").scaler.set("minimum",v);
},_setMaximumAttr:function(v){
this.getElement("scale").scaler.set("maximum",v);
},_setSnapIntervalAttr:function(v){
this.getElement("scale").scaler.set("snapInterval",v);
},_setMajorTickIntervalAttr:function(v){
this.getElement("scale").scaler.set("majorTickInterval",v);
},_setMinorTickIntervalAttr:function(v){
this.getElement("scale").scaler.set("minorTickInterval",v);
},_setMinorTicksEnabledAttr:function(v){
this.getElement("scale").scaler.set("minorTicksEnabled",v);
},_setInteractionAreaAttr:function(v){
this.getElement("scale").getIndicator("indicator").set("interactionArea",v);
},_setInteractionModeAttr:function(v){
this.getElement("scale").getIndicator("indicator").set("interactionMode",v);
},_setAnimationDurationAttr:function(v){
this.getElement("scale").getIndicator("indicator").set("animationDuration",v);
},_setBorderColorAttr:function(v){
this.borderColor=new _2(v);
this.invalidateRendering();
},_setFillColorAttr:function(v){
this.fillColor=new _2(v);
this.invalidateRendering();
},_setIndicatorColorAttr:function(v){
this.indicatorColor=new _2(v);
this.invalidateRendering();
}});
});
