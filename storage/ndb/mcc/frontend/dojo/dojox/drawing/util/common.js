//>>built
define("dojox/drawing/util/common",["dojo","dojox/math/round"],function(_1,_2){
var _3={};
var _4=0;
return {radToDeg:function(n){
return (n*180)/Math.PI;
},degToRad:function(n){
return (n*Math.PI)/180;
},angle:function(_5,_6){
if(_6){
_6=_6/180;
var _7=this.radians(_5),_8=Math.PI*_6,_9=_2(_7/_8),_a=_9*_8;
return _2(this.radToDeg(_a));
}else{
return this.radToDeg(this.radians(_5));
}
},oppAngle:function(_b){
(_b+=180)>360?_b=_b-360:_b;
return _b;
},radians:function(o){
return Math.atan2(o.start.y-o.y,o.x-o.start.x);
},length:function(o){
return Math.sqrt(Math.pow(o.start.x-o.x,2)+Math.pow(o.start.y-o.y,2));
},lineSub:function(x1,y1,x2,y2,_c){
var _d=this.distance(this.argsToObj.apply(this,arguments));
_d=_d<_c?_c:_d;
var pc=(_d-_c)/_d;
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
},pointOnCircle:function(cx,cy,_e,_f){
var _10=_f*Math.PI/180;
var x=_e*Math.cos(_10);
var y=_e*Math.sin(_10);
return {x:cx+x,y:cy-y};
},constrainAngle:function(obj,min,max){
var _11=this.angle(obj);
if(_11>=min&&_11<=max){
return obj;
}
var _12=this.length(obj);
var _13=_11>max?max:min-_11<100?min:max;
return this.pointOnCircle(obj.start.x,obj.start.y,_12,_13);
},snapAngle:function(obj,ca){
var _14=this.radians(obj),_15=this.length(obj),seg=Math.PI*ca,rnd=Math.round(_14/seg),_16=rnd*seg,_17=this.radToDeg(_16),pt=this.pointOnCircle(obj.start.x,obj.start.y,_15,_17);
return pt;
},idSetStart:function(num){
_4=num;
},uid:function(str){
str=str||"shape";
_3[str]=_3[str]===undefined?_4:_3[str]+1;
return str+_3[str];
},abbr:function(_18){
return _18.substring(_18.lastIndexOf(".")+1).charAt(0).toLowerCase()+_18.substring(_18.lastIndexOf(".")+2);
},mixin:function(o1,o2){
},objects:{},register:function(obj){
this.objects[obj.id]=obj;
},byId:function(id){
return this.objects[id];
},attr:function(_19,_1a,_1b,_1c){
if(!_19){
return false;
}
try{
if(_19.shape&&_19.util){
_19=_19.shape;
}
if(!_1b&&_1a=="id"&&_19.target){
var n=_19.target;
while(n&&!_1.attr(n,"id")){
n=n.parentNode;
}
return n&&_1.attr(n,"id");
}
if(_19.rawNode||_19.target){
var _1d=Array.prototype.slice.call(arguments);
_1d[0]=_19.rawNode||_19.target;
return _1.attr.apply(_1,_1d);
}
return _1.attr(_19,"id");
}
catch(e){
if(!_1c){
}
return false;
}
}};
});
