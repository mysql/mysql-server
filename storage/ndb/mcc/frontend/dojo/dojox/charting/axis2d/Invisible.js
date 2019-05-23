//>>built
define("dojox/charting/axis2d/Invisible",["dojo/_base/lang","dojo/_base/declare","./Base","../scaler/linear","dojox/gfx","dojox/lang/utils"],function(_1,_2,_3,_4,g,du){
return _2("dojox.charting.axis2d.Invisible",_3,{defaultParams:{vertical:false,fixUpper:"none",fixLower:"none",natural:false,leftBottom:true,includeZero:false,fixed:true},optionalParams:{min:0,max:1,from:0,to:1,majorTickStep:4,minorTickStep:2,microTickStep:1},constructor:function(_5,_6){
this.opt=_1.clone(this.defaultParams);
du.updateWithObject(this.opt,_6);
du.updateWithPattern(this.opt,_6,this.optionalParams);
},dependOnData:function(){
return !("min" in this.opt)||!("max" in this.opt);
},clear:function(){
delete this.scaler;
delete this.ticks;
this.dirty=true;
return this;
},initialized:function(){
return "scaler" in this&&!(this.dirty&&this.dependOnData());
},setWindow:function(_7,_8){
this.scale=_7;
this.offset=_8;
return this.clear();
},getWindowScale:function(){
return "scale" in this?this.scale:1;
},getWindowOffset:function(){
return "offset" in this?this.offset:0;
},calculate:function(_9,_a,_b){
if(this.initialized()){
return this;
}
var o=this.opt;
this.labels=o.labels;
this.scaler=_4.buildScaler(_9,_a,_b,o);
var _c=this.scaler.bounds;
if("scale" in this){
o.from=_c.lower+this.offset;
o.to=(_c.upper-_c.lower)/this.scale+o.from;
if(!isFinite(o.from)||isNaN(o.from)||!isFinite(o.to)||isNaN(o.to)||o.to-o.from>=_c.upper-_c.lower){
delete o.from;
delete o.to;
delete this.scale;
delete this.offset;
}else{
if(o.from<_c.lower){
o.to+=_c.lower-o.from;
o.from=_c.lower;
}else{
if(o.to>_c.upper){
o.from+=_c.upper-o.to;
o.to=_c.upper;
}
}
this.offset=o.from-_c.lower;
}
this.scaler=_4.buildScaler(_9,_a,_b,o);
_c=this.scaler.bounds;
if(this.scale==1&&this.offset==0){
delete this.scale;
delete this.offset;
}
}
return this;
},getScaler:function(){
return this.scaler;
},getTicks:function(){
return this.ticks;
}});
});
