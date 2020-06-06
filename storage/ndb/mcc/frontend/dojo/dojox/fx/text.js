//>>built
define("dojox/fx/text",["dojo/_base/lang","./_base","dojo/_base/fx","dojo/fx","dojo/fx/easing","dojo/dom","dojo/dom-style","dojo/_base/html","dojo/_base/connect"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
var _a=_1.getObject("dojox.fx.text",true);
_a._split=function(_b){
var _c=_b.node=_6.byId(_b.node),s=_c.style,cs=_7.getComputedStyle(_c),_d=_8.coords(_c,true);
_b.duration=_b.duration||1000;
_b.words=_b.words||false;
var _e=(_b.text&&typeof (_b.text)=="string")?_b.text:_c.innerHTML,_f=s.height,_10=s.width,_11=[];
_7.set(_c,{height:cs.height,width:cs.width});
var _12=/(<\/?\w+((\s+\w+(\s*=\s*(?:".*?"|'.*?'|[^'">\s]+))?)+\s*|\s*)\/?>)/g;
var reg=(_b.words?/(<\/?\w+((\s+\w+(\s*=\s*(?:".*?"|'.*?'|[^'">\s]+))?)+\s*|\s*)\/?>)\s*|([^\s<]+\s*)/g:/(<\/?\w+((\s+\w+(\s*=\s*(?:".*?"|'.*?'|[^'">\s]+))?)+\s*|\s*)\/?>)\s*|([^\s<]\s*)/g);
var _13=(typeof _b.text=="string")?_b.text.match(reg):_c.innerHTML.match(reg);
var _14="";
var _15=0;
var _16=0;
for(var i=0;i<_13.length;i++){
var _17=_13[i];
if(!_17.match(_12)){
_14+="<span>"+_17+"</span>";
_15++;
}else{
_14+=_17;
}
}
_c.innerHTML=_14;
function _18(_19){
var _1a=_19.nextSibling;
if(_19.tagName=="SPAN"&&_19.childNodes.length==1&&_19.firstChild.nodeType==3){
var _1b=_8.coords(_19,true);
_16++;
_7.set(_19,{padding:0,margin:0,top:(_b.crop?"0px":_1b.t+"px"),left:(_b.crop?"0px":_1b.l+"px"),display:"inline"});
var _1c=_b.pieceAnimation(_19,_1b,_d,_16,_15);
if(_1.isArray(_1c)){
_11=_11.concat(_1c);
}else{
_11[_11.length]=_1c;
}
}else{
if(_19.firstChild){
_18(_19.firstChild);
}
}
if(_1a){
_18(_1a);
}
};
_18(_c.firstChild);
var _1d=_4.combine(_11);
_9.connect(_1d,"onEnd",_1d,function(){
_c.innerHTML=_e;
_7.set(_c,{height:_f,width:_10});
});
if(_b.onPlay){
_9.connect(_1d,"onPlay",_1d,_b.onPlay);
}
if(_b.onEnd){
_9.connect(_1d,"onEnd",_1d,_b.onEnd);
}
return _1d;
};
_a.explode=function(_1e){
var _1f=_1e.node=_6.byId(_1e.node);
var s=_1f.style;
_1e.distance=_1e.distance||1;
_1e.duration=_1e.duration||1000;
_1e.random=_1e.random||0;
if(typeof (_1e.fade)=="undefined"){
_1e.fade=true;
}
if(typeof (_1e.sync)=="undefined"){
_1e.sync=true;
}
_1e.random=Math.abs(_1e.random);
_1e.pieceAnimation=function(_20,_21,_22,_23,_24){
var _25=_21.h;
var _26=_21.w;
var _27=_1e.distance*2;
var _28=_1e.duration;
var _29=parseFloat(_20.style.top);
var _2a=parseFloat(_20.style.left);
var _2b=0;
var _2c=0;
var _2d=0;
if(_1e.random){
var _2e=(Math.random()*_1e.random)+Math.max(1-_1e.random,0);
_27*=_2e;
_28*=_2e;
_2b=((_1e.unhide&&_1e.sync)||(!_1e.unhide&&!_1e.sync))?(_1e.duration-_28):0;
_2c=Math.random()-0.5;
_2d=Math.random()-0.5;
}
var _2f=((_22.h-_25)/2-(_21.y-_22.y));
var _30=((_22.w-_26)/2-(_21.x-_22.x));
var _31=Math.sqrt(Math.pow(_30,2)+Math.pow(_2f,2));
var _32=_29-_2f*_27+_31*_2d;
var _33=_2a-_30*_27+_31*_2c;
var _34=_3.animateProperty({node:_20,duration:_28,delay:_2b,easing:(_1e.easing||(_1e.unhide?_5.sinOut:_5.circOut)),beforeBegin:(_1e.unhide?function(){
if(_1e.fade){
_7.set(_20,"opacity",0);
}
_20.style.position=_1e.crop?"relative":"absolute";
_20.style.top=_32+"px";
_20.style.left=_33+"px";
}:function(){
_20.style.position=_1e.crop?"relative":"absolute";
}),properties:{top:(_1e.unhide?{start:_32,end:_29}:{start:_29,end:_32}),left:(_1e.unhide?{start:_33,end:_2a}:{start:_2a,end:_33})}});
if(_1e.fade){
var _35=_3.animateProperty({node:_20,duration:_28,delay:_2b,easing:(_1e.fadeEasing||_5.quadOut),properties:{opacity:(_1e.unhide?{start:0,end:1}:{end:0})}});
return (_1e.unhide?[_35,_34]:[_34,_35]);
}else{
return _34;
}
};
var _36=_a._split(_1e);
return _36;
};
_a.converge=function(_37){
_37.unhide=true;
return _a.explode(_37);
};
_a.disintegrate=function(_38){
var _39=_38.node=_6.byId(_38.node);
var s=_39.style;
_38.duration=_38.duration||1500;
_38.distance=_38.distance||1.5;
_38.random=_38.random||0;
if(!_38.fade){
_38.fade=true;
}
var _3a=Math.abs(_38.random);
_38.pieceAnimation=function(_3b,_3c,_3d,_3e,_3f){
var _40=_3c.h;
var _41=_3c.w;
var _42=_38.interval||(_38.duration/(1.5*_3f));
var _43=(_38.duration-_3f*_42);
var _44=Math.random()*_3f*_42;
var _45=(_38.reverseOrder||_38.distance<0)?(_3e*_42):((_3f-_3e)*_42);
var _46=_44*_3a+Math.max(1-_3a,0)*_45;
var _47={};
if(_38.unhide){
_47.top={start:(parseFloat(_3b.style.top)-_3d.h*_38.distance),end:parseFloat(_3b.style.top)};
if(_38.fade){
_47.opacity={start:0,end:1};
}
}else{
_47.top={end:(parseFloat(_3b.style.top)+_3d.h*_38.distance)};
if(_38.fade){
_47.opacity={end:0};
}
}
var _48=_3.animateProperty({node:_3b,duration:_43,delay:_46,easing:(_38.easing||(_38.unhide?_5.sinIn:_5.circIn)),properties:_47,beforeBegin:(_38.unhide?function(){
if(_38.fade){
_7.set(_3b,"opacity",0);
}
_3b.style.position=_38.crop?"relative":"absolute";
_3b.style.top=_47.top.start+"px";
}:function(){
_3b.style.position=_38.crop?"relative":"absolute";
})});
return _48;
};
var _49=_a._split(_38);
return _49;
};
_a.build=function(_4a){
_4a.unhide=true;
return _a.disintegrate(_4a);
};
_a.blockFadeOut=function(_4b){
var _4c=_4b.node=_6.byId(_4b.node);
var s=_4c.style;
_4b.duration=_4b.duration||1000;
_4b.random=_4b.random||0;
var _4d=Math.abs(_4b.random);
_4b.pieceAnimation=function(_4e,_4f,_50,_51,_52){
var _53=_4b.interval||(_4b.duration/(1.5*_52));
var _54=(_4b.duration-_52*_53);
var _55=Math.random()*_52*_53;
var _56=(_4b.reverseOrder)?((_52-_51)*_53):(_51*_53);
var _57=_55*_4d+Math.max(1-_4d,0)*_56;
var _58=_3.animateProperty({node:_4e,duration:_54,delay:_57,easing:(_4b.easing||_5.sinInOut),properties:{opacity:(_4b.unhide?{start:0,end:1}:{end:0})},beforeBegin:(_4b.unhide?function(){
_7.set(_4e,"opacity",0);
}:undefined)});
return _58;
};
var _59=_a._split(_4b);
return _59;
};
_a.blockFadeIn=function(_5a){
_5a.unhide=true;
return _a.blockFadeOut(_5a);
};
_a.backspace=function(_5b){
var _5c=_5b.node=_6.byId(_5b.node);
var s=_5c.style;
_5b.words=false;
_5b.duration=_5b.duration||2000;
_5b.random=_5b.random||0;
var _5d=Math.abs(_5b.random);
var _5e=10;
_5b.pieceAnimation=function(_5f,_60,_61,_62,_63){
var _64=_5b.interval||(_5b.duration/(1.5*_63)),_65=("textContent" in _5f)?_5f.textContent:_5f.innerText,_66=_65.match(/\s/g);
if(typeof (_5b.wordDelay)=="undefined"){
_5b.wordDelay=_64*2;
}
if(!_5b.unhide){
_5e=(_63-_62-1)*_64;
}
var _67,_68;
if(_5b.fixed){
if(_5b.unhide){
var _67=function(){
_7.set(_5f,"opacity",0);
};
}
}else{
if(_5b.unhide){
var _67=function(){
_5f.style.display="none";
};
var _68=function(){
_5f.style.display="inline";
};
}else{
var _68=function(){
_5f.style.display="none";
};
}
}
var _69=_3.animateProperty({node:_5f,duration:1,delay:_5e,easing:(_5b.easing||_5.sinInOut),properties:{opacity:(_5b.unhide?{start:0,end:1}:{end:0})},beforeBegin:_67,onEnd:_68});
if(_5b.unhide){
var _6a=Math.random()*_65.length*_64;
var _6b=_6a*_5d/2+Math.max(1-_5d/2,0)*_5b.wordDelay;
_5e+=_6a*_5d+Math.max(1-_5d,0)*_64*_65.length+(_6b*(_66&&_65.lastIndexOf(_66[_66.length-1])==_65.length-1));
}
return _69;
};
var _6c=_a._split(_5b);
return _6c;
};
_a.type=function(_6d){
_6d.unhide=true;
return _a.backspace(_6d);
};
return _a;
});
