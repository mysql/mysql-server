//>>built
define("dojox/css3/fx",["dojo/_base/lang","dojo/_base/connect","dojo/dom-style","dojo/_base/fx","dojo/fx","dojo/_base/html","dojox/html/ext-dojo/style","dojox/fx/ext-dojo/complex"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1.getObject("dojox.css3.fx",true);
var _a={puff:function(_b){
return _5.combine([_4.fadeOut(_b),this.expand({node:_b.node,endScale:_b.endScale||2})]);
},expand:function(_c){
return _4.animateProperty({node:_c.node,properties:{transform:{start:"scale(1)",end:"scale("+[_c.endScale||3]+")"}}});
},shrink:function(_d){
return this.expand({node:_d.node,endScale:0.01});
},rotate:function(_e){
return _4.animateProperty({node:_e.node,duration:_e.duration||1000,properties:{transform:{start:"rotate("+(_e.startAngle||"0deg")+")",end:"rotate("+(_e.endAngle||"360deg")+")"}}});
},flip:function(_f){
var _10=[],_11=_f.whichAnims||[0,1,2,3],_12=_f.direction||1,_13=[{start:"scale(1, 1) skew(0deg,0deg)",end:"scale(0, 1) skew(0,"+(_12*30)+"deg)"},{start:"scale(0, 1) skew(0deg,"+(_12*30)+"deg)",end:"scale(-1, 1) skew(0deg,0deg)"},{start:"scale(-1, 1) skew(0deg,0deg)",end:"scale(0, 1) skew(0deg,"+(-_12*30)+"deg)"},{start:"scale(0, 1) skew(0deg,"+(-_12*30)+"deg)",end:"scale(1, 1) skew(0deg,0deg)"}];
for(var i=0;i<_11.length;i++){
_10.push(_4.animateProperty(_1.mixin({node:_f.node,duration:_f.duration||600,properties:{transform:_13[_11[i]]}},_f)));
}
return _5.chain(_10);
},bounce:function(_14){
var _15=[],n=_14.node,_16=_14.duration||1000,_17=_14.scaleX||1.2,_18=_14.scaleY||0.6,ds=_6.style,_19=ds(n,"position"),_1a="absolute",_1b=ds(n,"top"),_1c=[],_1d=0,_1e=Math.round,_1f=_14.jumpHeight||70;
if(_19!=="absolute"){
_1a="relative";
}
var a1=_4.animateProperty({node:n,duration:_16/6,properties:{transform:{start:"scale(1, 1)",end:"scale("+_17+", "+_18+")"}}});
_2.connect(a1,"onBegin",function(){
ds(n,{transformOrigin:"50% 100%",position:_1a});
});
_15.push(a1);
var a2=_4.animateProperty({node:n,duration:_16/6,properties:{transform:{end:"scale(1, 1)",start:"scale("+_17+", "+_18+")"}}});
_1c.push(a2);
_1c.push(new _4.Animation(_1.mixin({curve:[],duration:_16/3,delay:_16/12,onBegin:function(){
_1d=(new Date).getTime();
},onAnimate:function(){
var _20=(new Date).getTime();
ds(n,{top:parseInt(ds(n,"top"))-_1e(_1f*((_20-_1d)/this.duration))+"px"});
_1d=_20;
}},_14)));
_15.push(_5.combine(_1c));
_15.push(_4.animateProperty(_1.mixin({duration:_16/3,onEnd:function(){
ds(n,{position:_19});
},properties:{top:_1b}},_14)));
_15.push(a1);
_15.push(a2);
return _5.chain(_15);
}};
return _1.mixin(_9,_a);
});
