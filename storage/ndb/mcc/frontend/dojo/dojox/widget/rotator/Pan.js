//>>built
define("dojox/widget/rotator/Pan",["dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/dom-style","dojo/_base/fx","dojo/fx"],function(_1,_2,_3,_4,_5,fx){
var _6=0,_7=1,UP=2,_8=3;
function _9(_a,_b){
var n=_b.next.node,r=_b.rotatorBox,m=_a%2,a=m?"left":"top",s=(m?r.w:r.h)*(_a<2?-1:1),p={},q={};
_4.set(n,"display","");
p[a]={start:0,end:-s};
q[a]={start:s,end:0};
return fx.combine([_5.animateProperty({node:_b.current.node,duration:_b.duration,properties:p,easing:_b.easing}),_5.animateProperty({node:n,duration:_b.duration,properties:q,easing:_b.easing})]);
};
function _c(n,z){
_4.set(n,"zIndex",z);
};
var _d={pan:function(_e){
var w=_e.wrap,p=_e.rotator.panes,_f=p.length,z=_f,j=_e.current.idx,k=_e.next.idx,nw=Math.abs(k-j),ww=Math.abs((_f-Math.max(j,k))+Math.min(j,k))%_f,_10=j<k,_11=_8,_12=[],_13=[],_14=_e.duration;
if((!w&&!_10)||(w&&(_10&&nw>ww||!_10&&nw<ww))){
_11=_7;
}
if(_e.continuous){
if(_e.quick){
_14=Math.round(_14/(w?Math.min(ww,nw):nw));
}
_c(p[j].node,z--);
var f=(_11==_8);
while(1){
var i=j;
if(f){
if(++j>=_f){
j=0;
}
}else{
if(--j<0){
j=_f-1;
}
}
var x=p[i],y=p[j];
_c(y.node,z--);
_12.push(_9(_11,_3.mixin({easing:function(m){
return m;
}},_e,{current:x,next:y,duration:_14})));
if((f&&j==k)||(!f&&j==k)){
break;
}
_13.push(y.node);
}
var _15=fx.chain(_12),h=_2.connect(_15,"onEnd",function(){
_2.disconnect(h);
_1.forEach(_13,function(q){
_4.set(q,{display:"none",left:0,opacity:1,top:0,zIndex:0});
});
});
return _15;
}
return _9(_11,_e);
},panDown:function(_16){
return _9(_6,_16);
},panRight:function(_17){
return _9(_7,_17);
},panUp:function(_18){
return _9(UP,_18);
},panLeft:function(_19){
return _9(_8,_19);
}};
_3.mixin(_3.getObject("dojox.widget.rotator"),_d);
return _d;
});
