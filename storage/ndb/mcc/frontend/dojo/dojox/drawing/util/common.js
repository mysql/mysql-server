//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/math/round"],function(_1,_2,_3){
_2.provide("dojox.drawing.util.common");
_2.require("dojox.math.round");
(function(){
var _4={};
var _5=0;
_3.drawing.util.common={radToDeg:function(n){
return (n*180)/Math.PI;
},degToRad:function(n){
return (n*Math.PI)/180;
},angle:function(_6,_7){
if(_7){
_7=_7/180;
var _8=this.radians(_6),_9=Math.PI*_7,_a=_3.math.round(_8/_9),_b=_a*_9;
return _3.math.round(this.radToDeg(_b));
}else{
return this.radToDeg(this.radians(_6));
}
},oppAngle:function(_c){
(_c+=180)>360?_c=_c-360:_c;
return _c;
},radians:function(o){
return Math.atan2(o.start.y-o.y,o.x-o.start.x);
},length:function(o){
return Math.sqrt(Math.pow(o.start.x-o.x,2)+Math.pow(o.start.y-o.y,2));
},lineSub:function(x1,y1,x2,y2,_d){
var _e=this.distance(this.argsToObj.apply(this,arguments));
_e=_e<_d?_d:_e;
var pc=(_e-_d)/_e;
var x=x1-(x1-x2)*pc;
var y=y1-(y1-y2)*pc;
return {x:x,y:y};
},argsToObj:function(){
var a=arguments;
if(a.length<4){
return a[0];
}
return {start:{x:a[0],y:a[1]},x:a[2],y:a[3]};
},distance:function(){
var o=this.argsToObj.apply(this,arguments);
return Math.abs(Math.sqrt(Math.pow(o.start.x-o.x,2)+Math.pow(o.start.y-o.y,2)));
},slope:function(p1,p2){
if(!(p1.x-p2.x)){
return 0;
}
return ((p1.y-p2.y)/(p1.x-p2.x));
},pointOnCircle:function(cx,cy,_f,_10){
var _11=_10*Math.PI/180;
var x=_f*Math.cos(_11);
var y=_f*Math.sin(_11);
return {x:cx+x,y:cy-y};
},constrainAngle:function(obj,min,max){
var _12=this.angle(obj);
if(_12>=min&&_12<=max){
return obj;
}
var _13=this.length(obj);
var _14=_12>max?max:min-_12<100?min:max;
return this.pointOnCircle(obj.start.x,obj.start.y,_13,_14);
},snapAngle:function(obj,ca){
var _15=this.radians(obj),_16=this.length(obj),seg=Math.PI*ca,rnd=Math.round(_15/seg),_17=rnd*seg,_18=this.radToDeg(_17),pt=this.pointOnCircle(obj.start.x,obj.start.y,_16,_18);
return pt;
},idSetStart:function(num){
_5=num;
},uid:function(str){
str=str||"shape";
_4[str]=_4[str]===undefined?_5:_4[str]+1;
return str+_4[str];
},abbr:function(_19){
return _19.substring(_19.lastIndexOf(".")+1).charAt(0).toLowerCase()+_19.substring(_19.lastIndexOf(".")+2);
},mixin:function(o1,o2){
},objects:{},register:function(obj){
this.objects[obj.id]=obj;
},byId:function(id){
return this.objects[id];
},attr:function(_1a,_1b,_1c,_1d){
if(!_1a){
return false;
}
try{
if(_1a.shape&&_1a.util){
_1a=_1a.shape;
}
if(!_1c&&_1b=="id"&&_1a.target){
var n=_1a.target;
while(!_2.attr(n,"id")){
n=n.parentNode;
}
return _2.attr(n,"id");
}
if(_1a.rawNode||_1a.target){
var _1e=Array.prototype.slice.call(arguments);
_1e[0]=_1a.rawNode||_1a.target;
return _2.attr.apply(_2,_1e);
}
return _2.attr(_1a,"id");
}
catch(e){
if(!_1d){
}
return false;
}
}};
})();
});
