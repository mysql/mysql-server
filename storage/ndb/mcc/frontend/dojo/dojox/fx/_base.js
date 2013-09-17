//>>built
define("dojox/fx/_base",["dojo/_base/array","dojo/_base/lang","dojo/_base/fx","dojo/fx","dojo/dom","dojo/dom-style","dojo/dom-geometry","dojo/_base/connect","dojo/_base/html"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_2.getObject("dojox.fx",true);
_a.sizeTo=function(_b){
var _c=_b.node=_5.byId(_b.node),_d="absolute";
var _e=_b.method||"chain";
if(!_b.duration){
_b.duration=500;
}
if(_e=="chain"){
_b.duration=Math.floor(_b.duration/2);
}
var _f,_10,_11,_12,_13,_14=null;
var _15=(function(n){
return function(){
var cs=_6.getComputedStyle(n),pos=cs.position,w=cs.width,h=cs.height;
_f=(pos==_d?n.offsetTop:parseInt(cs.top)||0);
_11=(pos==_d?n.offsetLeft:parseInt(cs.left)||0);
_13=(w=="auto"?0:parseInt(w));
_14=(h=="auto"?0:parseInt(h));
_12=_11-Math.floor((_b.width-_13)/2);
_10=_f-Math.floor((_b.height-_14)/2);
if(pos!=_d&&pos!="relative"){
var ret=_6.coords(n,true);
_f=ret.y;
_11=ret.x;
n.style.position=_d;
n.style.top=_f+"px";
n.style.left=_11+"px";
}
};
})(_c);
var _16=_3.animateProperty(_2.mixin({properties:{height:function(){
_15();
return {end:_b.height||0,start:_14};
},top:function(){
return {start:_f,end:_10};
}}},_b));
var _17=_3.animateProperty(_2.mixin({properties:{width:function(){
return {start:_13,end:_b.width||0};
},left:function(){
return {start:_11,end:_12};
}}},_b));
var _18=_4[(_b.method=="combine"?"combine":"chain")]([_16,_17]);
return _18;
};
_a.slideBy=function(_19){
var _1a=_19.node=_5.byId(_19.node),top,_1b;
var _1c=(function(n){
return function(){
var cs=_6.getComputedStyle(n);
var pos=cs.position;
top=(pos=="absolute"?n.offsetTop:parseInt(cs.top)||0);
_1b=(pos=="absolute"?n.offsetLeft:parseInt(cs.left)||0);
if(pos!="absolute"&&pos!="relative"){
var ret=_7.coords(n,true);
top=ret.y;
_1b=ret.x;
n.style.position="absolute";
n.style.top=top+"px";
n.style.left=_1b+"px";
}
};
})(_1a);
_1c();
var _1d=_3.animateProperty(_2.mixin({properties:{top:top+(_19.top||0),left:_1b+(_19.left||0)}},_19));
_8.connect(_1d,"beforeBegin",_1d,_1c);
return _1d;
};
_a.crossFade=function(_1e){
var _1f=_1e.nodes[0]=_5.byId(_1e.nodes[0]),op1=_9.style(_1f,"opacity"),_20=_1e.nodes[1]=_5.byId(_1e.nodes[1]),op2=_9.style(_20,"opacity");
var _21=_4.combine([_3[(op1==0?"fadeIn":"fadeOut")](_2.mixin({node:_1f},_1e)),_3[(op1==0?"fadeOut":"fadeIn")](_2.mixin({node:_20},_1e))]);
return _21;
};
_a.highlight=function(_22){
var _23=_22.node=_5.byId(_22.node);
_22.duration=_22.duration||400;
var _24=_22.color||"#ffff99",_25=_9.style(_23,"backgroundColor");
if(_25=="rgba(0, 0, 0, 0)"){
_25="transparent";
}
var _26=_3.animateProperty(_2.mixin({properties:{backgroundColor:{start:_24,end:_25}}},_22));
if(_25=="transparent"){
_8.connect(_26,"onEnd",_26,function(){
_23.style.backgroundColor=_25;
});
}
return _26;
};
_a.wipeTo=function(_27){
_27.node=_5.byId(_27.node);
var _28=_27.node,s=_28.style;
var dir=(_27.width?"width":"height"),_29=_27[dir],_2a={};
_2a[dir]={start:function(){
s.overflow="hidden";
if(s.visibility=="hidden"||s.display=="none"){
s[dir]="1px";
s.display="";
s.visibility="";
return 1;
}else{
var now=_9.style(_28,dir);
return Math.max(now,1);
}
},end:_29};
var _2b=_3.animateProperty(_2.mixin({properties:_2a},_27));
return _2b;
};
return _a;
});
