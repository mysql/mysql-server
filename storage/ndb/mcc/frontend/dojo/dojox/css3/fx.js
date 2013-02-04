//>>built
define("dojox/css3/fx",["dojo/_base/kernel","dojo/_base/connect","dojo/dom-style","dojo/_base/fx","dojo/fx","dojo/_base/html","dojox/html/ext-dojo/style","dojox/fx/ext-dojo/complex"],function(_1,_2,_3,_4,_5,_6,_7,_8){
var _9=_1.getObject("dojox.css3.fx",true);
return _1.mixin(_9,{puff:function(_a){
return _5.combine([_4.fadeOut(_a),this.expand({node:_a.node,endScale:_a.endScale||2})]);
},expand:function(_b){
return _4.animateProperty({node:_b.node,properties:{transform:{start:"scale(1)",end:"scale("+[_b.endScale||3]+")"}}});
},shrink:function(_c){
return this.expand({node:_c.node,endScale:0.01});
},rotate:function(_d){
return _4.animateProperty({node:_d.node,duration:_d.duration||1000,properties:{transform:{start:"rotate("+(_d.startAngle||"0deg")+")",end:"rotate("+(_d.endAngle||"360deg")+")"}}});
},flip:function(_e){
var _f=[],_10=_e.whichAnims||[0,1,2,3],_11=_e.direction||1,_12=[{start:"scale(1, 1) skew(0deg,0deg)",end:"scale(0, 1) skew(0,"+(_11*30)+"deg)"},{start:"scale(0, 1) skew(0deg,"+(_11*30)+"deg)",end:"scale(-1, 1) skew(0deg,0deg)"},{start:"scale(-1, 1) skew(0deg,0deg)",end:"scale(0, 1) skew(0deg,"+(-_11*30)+"deg)"},{start:"scale(0, 1) skew(0deg,"+(-_11*30)+"deg)",end:"scale(1, 1) skew(0deg,0deg)"}];
for(var i=0;i<_10.length;i++){
_f.push(_4.animateProperty(_1.mixin({node:_e.node,duration:_e.duration||600,properties:{transform:_12[_10[i]]}},_e)));
}
return _5.chain(_f);
},bounce:function(_13){
var _14=[],n=_13.node,_15=_13.duration||1000,_16=_13.scaleX||1.2,_17=_13.scaleY||0.6,ds=_6.style,_18=ds(n,"position"),_19="absolute",_1a=ds(n,"top"),_1b=[],_1c=0,_1d=Math.round,_1e=_13.jumpHeight||70;
if(_18!=="absolute"){
_19="relative";
}
var a1=_4.animateProperty({node:n,duration:_15/6,properties:{transform:{start:"scale(1, 1)",end:"scale("+_16+", "+_17+")"}}});
_2.connect(a1,"onBegin",function(){
ds(n,{transformOrigin:"50% 100%",position:_19});
});
_14.push(a1);
var a2=_4.animateProperty({node:n,duration:_15/6,properties:{transform:{end:"scale(1, 1)",start:"scale("+_16+", "+_17+")"}}});
_1b.push(a2);
_1b.push(new _4.Animation(_1.mixin({curve:[],duration:_15/3,delay:_15/12,onBegin:function(){
_1c=(new Date).getTime();
},onAnimate:function(){
var _1f=(new Date).getTime();
ds(n,{top:parseInt(ds(n,"top"))-_1d(_1e*((_1f-_1c)/this.duration))+"px"});
_1c=_1f;
}},_13)));
_14.push(_5.combine(_1b));
_14.push(_4.animateProperty(_1.mixin({duration:_15/3,onEnd:function(){
ds(n,{position:_18});
},properties:{top:_1a}},_13)));
_14.push(a1);
_14.push(a2);
return _5.chain(_14);
}});
});
