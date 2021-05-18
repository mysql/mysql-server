//>>built
define("dojox/dgauges/LinearScaler",["dojo/_base/lang","dojo/_base/declare","dojo/Stateful"],function(_1,_2,_3){
return _2("dojox.dgauges.LinearScaler",_3,{minimum:0,maximum:100,snapInterval:1,majorTickInterval:NaN,minorTickInterval:NaN,minorTicksEnabled:true,majorTicks:null,minorTicks:null,_computedMajorTickInterval:NaN,_computedMinorTickInterval:NaN,constructor:function(){
this.watchedProperties=["minimum","maximum","majorTickInterval","minorTickInterval","snapInterval","minorTicksEnabled"];
},_buildMinorTickItems:function(){
var mt=this.majorTicks;
var _4=[];
if(this.maximum>this.minimum){
var _5=Math.floor((this.maximum-this.minimum)/this.getComputedMajorTickInterval())+1;
var _6=Math.floor(this.getComputedMajorTickInterval()/this.getComputedMinorTickInterval());
var _7;
for(var i=0;i<_5-1;i++){
for(var j=1;j<_6;j++){
_7={scaler:this};
_7.isMinor=true;
_7.value=mt[i].value+j*this.getComputedMinorTickInterval();
_7.position=(Number(_7.value)-this.minimum)/(this.maximum-this.minimum);
_4.push(_7);
}
}
}
return _4;
},_buildMajorTickItems:function(){
var _8=[];
if(this.maximum>this.minimum){
var _9=Math.floor((this.maximum-this.minimum)/this.getComputedMajorTickInterval())+1;
var _a;
for(var i=0;i<_9;i++){
_a={scaler:this};
_a.isMinor=false;
_a.value=this.minimum+i*this.getComputedMajorTickInterval();
_a.position=(Number(_a.value)-this.minimum)/(this.maximum-this.minimum);
_8.push(_a);
}
}
return _8;
},getComputedMajorTickInterval:function(){
if(!isNaN(this.majorTickInterval)){
return this.majorTickInterval;
}
if(isNaN(this._computedMajorTickInterval)){
this._computedMajorTickInterval=(this.maximum-this.minimum)/10;
}
return this._computedMajorTickInterval;
},getComputedMinorTickInterval:function(){
if(!isNaN(this.minorTickInterval)){
return this.minorTickInterval;
}
if(isNaN(this._computedMinorTickInterval)){
this._computedMinorTickInterval=this.getComputedMajorTickInterval()/5;
}
return this._computedMinorTickInterval;
},computeTicks:function(){
this.majorTicks=this._buildMajorTickItems();
this.minorTicks=this.minorTicksEnabled?this._buildMinorTickItems():[];
return this.majorTicks.concat(this.minorTicks);
},positionForValue:function(_b){
var _c;
if(_b==null||isNaN(_b)||_b<=this.minimum){
_c=0;
}
if(_b>=this.maximum){
_c=1;
}
if(isNaN(_c)){
_c=(_b-this.minimum)/(this.maximum-this.minimum);
}
return _c;
},valueForPosition:function(_d){
var _e=Math.abs(this.minimum-this.maximum);
var _f=this.minimum+_e*_d;
if(!isNaN(this.snapInterval)&&this.snapInterval>0){
_f=Math.round((_f-this.minimum)/this.snapInterval)*this.snapInterval+this.minimum;
}
return _f;
}});
});
