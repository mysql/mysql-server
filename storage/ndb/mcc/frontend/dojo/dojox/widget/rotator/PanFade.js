//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/fx"],function(_1,_2,_3){
_2.provide("dojox.widget.rotator.PanFade");
_2.require("dojo.fx");
(function(d){
var _4=0,_5=1,UP=2,_6=3;
function _7(_8,_9){
var j={node:_9.current.node,duration:_9.duration,easing:_9.easing},k={node:_9.next.node,duration:_9.duration,easing:_9.easing},r=_9.rotatorBox,m=_8%2,a=m?"left":"top",s=(m?r.w:r.h)*(_8<2?-1:1),p={},q={};
d.style(k.node,{display:"",opacity:0});
p[a]={start:0,end:-s};
q[a]={start:s,end:0};
return d.fx.combine([d.animateProperty(d.mixin({properties:p},j)),d.fadeOut(j),d.animateProperty(d.mixin({properties:q},k)),d.fadeIn(k)]);
};
function _a(n,z){
d.style(n,"zIndex",z);
};
d.mixin(_3.widget.rotator,{panFade:function(_b){
var w=_b.wrap,p=_b.rotator.panes,_c=p.length,z=_c,j=_b.current.idx,k=_b.next.idx,nw=Math.abs(k-j),ww=Math.abs((_c-Math.max(j,k))+Math.min(j,k))%_c,_d=j<k,_e=_6,_f=[],_10=[],_11=_b.duration;
if((!w&&!_d)||(w&&(_d&&nw>ww||!_d&&nw<ww))){
_e=_5;
}
if(_b.continuous){
if(_b.quick){
_11=Math.round(_11/(w?Math.min(ww,nw):nw));
}
_a(p[j].node,z--);
var f=(_e==_6);
while(1){
var i=j;
if(f){
if(++j>=_c){
j=0;
}
}else{
if(--j<0){
j=_c-1;
}
}
var x=p[i],y=p[j];
_a(y.node,z--);
_f.push(_7(_e,d.mixin({easing:function(m){
return m;
}},_b,{current:x,next:y,duration:_11})));
if((f&&j==k)||(!f&&j==k)){
break;
}
_10.push(y.node);
}
var _12=d.fx.chain(_f),h=d.connect(_12,"onEnd",function(){
d.disconnect(h);
d.forEach(_10,function(q){
d.style(q,{display:"none",left:0,opacity:1,top:0,zIndex:0});
});
});
return _12;
}
return _7(_e,_b);
},panFadeDown:function(_13){
return _7(_4,_13);
},panFadeRight:function(_14){
return _7(_5,_14);
},panFadeUp:function(_15){
return _7(UP,_15);
},panFadeLeft:function(_16){
return _7(_6,_16);
}});
})(_2);
});
