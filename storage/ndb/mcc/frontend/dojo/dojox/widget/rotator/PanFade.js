//>>built
define("dojox/widget/rotator/PanFade",["dojo/_base/array","dojo/_base/connect","dojo/_base/lang","dojo/_base/fx","dojo/dom-style","dojo/fx"],function(_1,_2,_3,_4,_5,fx){
var _6=0,_7=1,UP=2,_8=3;
function _9(_a,_b){
var j={node:_b.current.node,duration:_b.duration,easing:_b.easing},k={node:_b.next.node,duration:_b.duration,easing:_b.easing},r=_b.rotatorBox,m=_a%2,a=m?"left":"top",s=(m?r.w:r.h)*(_a<2?-1:1),p={},q={};
_5.set(k.node,{display:"",opacity:0});
p[a]={start:0,end:-s};
q[a]={start:s,end:0};
return fx.combine([_4.animateProperty(_3.mixin({properties:p},j)),_4.fadeOut(j),_4.animateProperty(_3.mixin({properties:q},k)),_4.fadeIn(k)]);
};
function _c(n,z){
_5.set(n,"zIndex",z);
};
var _d={panFade:function(_e){
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
var _15=fx.chain(_12);
var h=_2.connect(_15,"onEnd",function(){
_2.disconnect(h);
_1.forEach(_13,function(q){
_5.set(q,{display:"none",left:0,opacity:1,top:0,zIndex:0});
});
});
return _15;
}
return _9(_11,_e);
},panFadeDown:function(_16){
return _9(_6,_16);
},panFadeRight:function(_17){
return _9(_7,_17);
},panFadeUp:function(_18){
return _9(UP,_18);
},panFadeLeft:function(_19){
return _9(_8,_19);
}};
_3.mixin(_3.getObject("dojox.widget.rotator"),_d);
return _d;
});
