//>>built
define("dojox/charting/plot3d/Cylinders",["dojox/gfx3d","dojox/gfx3d/matrix","dojo/_base/declare","dojo/_base/Color","dojo/_base/window","./Base"],function(_1,_2,_3,_4,_5,_6){
var _7=function(a,f,o){
a=typeof a=="string"?a.split(""):a;
o=o||_5.global;
var z=a[0];
for(var i=1;i<a.length;z=f.call(o,z,a[i++])){
}
return z;
};
return _3("dojox.charting.plot3d.Cylinders",_6,{constructor:function(_8,_9,_a){
this.depth="auto";
this.gap=0;
this.data=[];
this.material={type:"plastic",finish:"shiny",color:"lime"};
this.outline=null;
if(_a){
if("depth" in _a){
this.depth=_a.depth;
}
if("gap" in _a){
this.gap=_a.gap;
}
if("material" in _a){
var m=_a.material;
if(typeof m=="string"||m instanceof _4){
this.material.color=m;
}else{
this.material=m;
}
}
if("outline" in _a){
this.outline=_a.outline;
}
}
},getDepth:function(){
if(this.depth=="auto"){
var w=this.width;
if(this.data&&this.data.length){
w=w/this.data.length;
}
return w-2*this.gap;
}
return this.depth;
},generate:function(_b,_c){
if(!this.data){
return this;
}
var _d=this.width/this.data.length,_e=0,_f=this.height/_7(this.data,Math.max);
if(!_c){
_c=_b.view;
}
for(var i=0;i<this.data.length;++i,_e+=_d){
_c.createCylinder({center:{x:_e+_d/2,y:0,z:0},radius:_d/2-this.gap,height:this.data[i]*_f}).setTransform(_2.rotateXg(-90)).setFill(this.material).setStroke(this.outline);
}
}});
});
