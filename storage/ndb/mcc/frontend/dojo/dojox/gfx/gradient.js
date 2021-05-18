//>>built
define("dojox/gfx/gradient",["dojo/_base/lang","./matrix","dojo/_base/Color"],function(_1,m,_2){
var _3=_1.getObject("dojox.gfx.gradient",true);
var C=_2;
_3.rescale=function(_4,_5,to){
var _6=_4.length,_7=(to<_5),_8;
if(_7){
var _9=_5;
_5=to;
to=_9;
}
if(!_6){
return [];
}
if(to<=_4[0].offset){
_8=[{offset:0,color:_4[0].color},{offset:1,color:_4[0].color}];
}else{
if(_5>=_4[_6-1].offset){
_8=[{offset:0,color:_4[_6-1].color},{offset:1,color:_4[_6-1].color}];
}else{
var _a=to-_5,_b,_c,i;
_8=[];
if(_5<0){
_8.push({offset:0,color:new C(_4[0].color)});
}
for(i=0;i<_6;++i){
_b=_4[i];
if(_b.offset>=_5){
break;
}
}
if(i){
_c=_4[i-1];
_8.push({offset:0,color:_2.blendColors(new C(_c.color),new C(_b.color),(_5-_c.offset)/(_b.offset-_c.offset))});
}else{
_8.push({offset:0,color:new C(_b.color)});
}
for(;i<_6;++i){
_b=_4[i];
if(_b.offset>=to){
break;
}
_8.push({offset:(_b.offset-_5)/_a,color:new C(_b.color)});
}
if(i<_6){
_c=_4[i-1];
_8.push({offset:1,color:_2.blendColors(new C(_c.color),new C(_b.color),(to-_c.offset)/(_b.offset-_c.offset))});
}else{
_8.push({offset:1,color:new C(_4[_6-1].color)});
}
}
}
if(_7){
_8.reverse();
for(i=0,_6=_8.length;i<_6;++i){
_b=_8[i];
_b.offset=1-_b.offset;
}
}
return _8;
};
function _d(x,y,_e,_f,_10,_11){
var r=m.multiplyPoint(_e,x,y),p=m.multiplyPoint(_f,r);
return {r:r,p:p,o:m.multiplyPoint(_10,p).x/_11};
};
function _12(a,b){
return a.o-b.o;
};
_3.project=function(_13,_14,tl,rb,ttl,trb){
_13=_13||m.identity;
var f1=m.multiplyPoint(_13,_14.x1,_14.y1),f2=m.multiplyPoint(_13,_14.x2,_14.y2),_15=Math.atan2(f2.y-f1.y,f2.x-f1.x),_16=m.project(f2.x-f1.x,f2.y-f1.y),pf1=m.multiplyPoint(_16,f1),pf2=m.multiplyPoint(_16,f2),_17=new m.Matrix2D([m.rotate(-_15),{dx:-pf1.x,dy:-pf1.y}]),_18=m.multiplyPoint(_17,pf2).x,_19=[_d(tl.x,tl.y,_13,_16,_17,_18),_d(rb.x,rb.y,_13,_16,_17,_18),_d(tl.x,rb.y,_13,_16,_17,_18),_d(rb.x,tl.y,_13,_16,_17,_18)].sort(_12),_1a=_19[0].o,to=_19[3].o,_1b=_3.rescale(_14.colors,_1a,to),_1c=Math.atan2(_19[3].r.y-_19[0].r.y,_19[3].r.x-_19[0].r.x);
return {type:"linear",x1:_19[0].p.x,y1:_19[0].p.y,x2:_19[3].p.x,y2:_19[3].p.y,colors:_1b,angle:_15};
};
return _3;
});
