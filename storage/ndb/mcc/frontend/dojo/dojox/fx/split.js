//>>built
define("dojox/fx/split",["dojo/_base/lang","dojo/dom","dojo/_base/window","dojo/_base/html","dojo/dom-geometry","dojo/dom-construct","dojo/dom-attr","dojo/_base/fx","dojo/fx","./_base","dojo/fx/easing","dojo/_base/connect"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
var _d=_1.getObject("dojox.fx");
_1.mixin(_d,{_split:function(_e){
_e.rows=_e.rows||3;
_e.columns=_e.columns||3;
_e.duration=_e.duration||1000;
var _f=_e.node=_2.byId(_e.node),_10=_f.parentNode,_11=_10,_12=_3.body(),_13="position";
while(_11&&_11!=_12&&_4.style(_11,_13)=="static"){
_11=_11.parentNode;
}
var _14=_11!=_12?_5.position(_11,true):{x:0,y:0},_15=_5.position(_f,true),_16=_4.style(_f,"height"),_17=_4.style(_f,"width"),_18=_4.style(_f,"borderLeftWidth")+_4.style(_f,"borderRightWidth"),_19=_4.style(_f,"borderTopWidth")+_4.style(_f,"borderBottomWidth"),_1a=Math.ceil(_16/_e.rows),_1b=Math.ceil(_17/_e.columns),_1c=_6.create(_f.tagName,{style:{position:"absolute",padding:0,margin:0,border:"none",top:_15.y-_14.y+"px",left:_15.x-_14.x+"px",height:_16+_19+"px",width:_17+_18+"px",background:"none",overflow:_e.crop?"hidden":"visible",zIndex:_4.style(_f,"zIndex")}},_f,"after"),_1d=[],_1e=_6.create(_f.tagName,{style:{position:"absolute",border:"none",padding:0,margin:0,height:_1a+_18+"px",width:_1b+_19+"px",overflow:"hidden"}});
for(var y=0,ly=_e.rows;y<ly;y++){
for(var x=0,lx=_e.columns;x<lx;x++){
var _1f=_1.clone(_1e),_20=_1.clone(_f),_21=y*_1a,_22=x*_1b;
_20.style.filter="";
_7.remove(_20,"id");
_4.style(_1f,{border:"none",overflow:"hidden",top:_21+"px",left:_22+"px"});
_4.style(_20,{position:"static",opacity:"1",marginTop:-_21+"px",marginLeft:-_22+"px"});
_1f.appendChild(_20);
_1c.appendChild(_1f);
var _23=_e.pieceAnimation(_1f,x,y,_15);
if(_1.isArray(_23)){
_1d=_1d.concat(_23);
}else{
_1d.push(_23);
}
}
}
var _24=_9.combine(_1d);
_c.connect(_24,"onEnd",_24,function(){
_1c.parentNode.removeChild(_1c);
});
if(_e.onPlay){
_c.connect(_24,"onPlay",_24,_e.onPlay);
}
if(_e.onEnd){
_c.connect(_24,"onEnd",_24,_e.onEnd);
}
return _24;
},explode:function(_25){
var _26=_25.node=_2.byId(_25.node);
_25.rows=_25.rows||3;
_25.columns=_25.columns||3;
_25.distance=_25.distance||1;
_25.duration=_25.duration||1000;
_25.random=_25.random||0;
if(!_25.fade){
_25.fade=true;
}
if(typeof _25.sync=="undefined"){
_25.sync=true;
}
_25.random=Math.abs(_25.random);
_25.pieceAnimation=function(_27,x,y,_28){
var _29=_28.h/_25.rows,_2a=_28.w/_25.columns,_2b=_25.distance*2,_2c=_25.duration,ps=_27.style,_2d=parseInt(ps.top),_2e=parseInt(ps.left),_2f=0,_30=0,_31=0;
if(_25.random){
var _32=(Math.random()*_25.random)+Math.max(1-_25.random,0);
_2b*=_32;
_2c*=_32;
_2f=((_25.unhide&&_25.sync)||(!_25.unhide&&!_25.sync))?(_25.duration-_2c):0;
_30=Math.random()-0.5;
_31=Math.random()-0.5;
}
var _33=((_28.h-_29)/2-_29*y),_34=((_28.w-_2a)/2-_2a*x),_35=Math.sqrt(Math.pow(_34,2)+Math.pow(_33,2)),_36=parseInt(_2d-_33*_2b+_35*_31),_37=parseInt(_2e-_34*_2b+_35*_30);
var _38=_8.animateProperty({node:_27,duration:_2c,delay:_2f,easing:(_25.easing||(_25.unhide?_b.sinOut:_b.circOut)),beforeBegin:(_25.unhide?function(){
if(_25.fade){
_4.style(_27,{opacity:"0"});
}
ps.top=_36+"px";
ps.left=_37+"px";
}:undefined),properties:{top:(_25.unhide?{start:_36,end:_2d}:{start:_2d,end:_36}),left:(_25.unhide?{start:_37,end:_2e}:{start:_2e,end:_37})}});
if(_25.fade){
var _39=_8.animateProperty({node:_27,duration:_2c,delay:_2f,easing:(_25.fadeEasing||_b.quadOut),properties:{opacity:(_25.unhide?{start:"0",end:"1"}:{start:"1",end:"0"})}});
return (_25.unhide?[_39,_38]:[_38,_39]);
}else{
return _38;
}
};
var _3a=_d._split(_25);
if(_25.unhide){
_c.connect(_3a,"onEnd",null,function(){
_4.style(_26,{opacity:"1"});
});
}else{
_c.connect(_3a,"onPlay",null,function(){
_4.style(_26,{opacity:"0"});
});
}
return _3a;
},converge:function(_3b){
_3b.unhide=true;
return _d.explode(_3b);
},disintegrate:function(_3c){
var _3d=_3c.node=_2.byId(_3c.node);
_3c.rows=_3c.rows||5;
_3c.columns=_3c.columns||5;
_3c.duration=_3c.duration||1500;
_3c.interval=_3c.interval||_3c.duration/(_3c.rows+_3c.columns*2);
_3c.distance=_3c.distance||1.5;
_3c.random=_3c.random||0;
if(typeof _3c.fade=="undefined"){
_3c.fade=true;
}
var _3e=Math.abs(_3c.random),_3f=_3c.duration-(_3c.rows+_3c.columns)*_3c.interval;
_3c.pieceAnimation=function(_40,x,y,_41){
var _42=Math.random()*(_3c.rows+_3c.columns)*_3c.interval,ps=_40.style,_43=(_3c.reverseOrder||_3c.distance<0)?((x+y)*_3c.interval):(((_3c.rows+_3c.columns)-(x+y))*_3c.interval),_44=_42*_3e+Math.max(1-_3e,0)*_43,_45={};
if(_3c.unhide){
_45.top={start:(parseInt(ps.top)-_41.h*_3c.distance),end:parseInt(ps.top)};
if(_3c.fade){
_45.opacity={start:"0",end:"1"};
}
}else{
_45.top={end:(parseInt(ps.top)+_41.h*_3c.distance)};
if(_3c.fade){
_45.opacity={end:"0"};
}
}
var _46=_8.animateProperty({node:_40,duration:_3f,delay:_44,easing:(_3c.easing||(_3c.unhide?_b.sinIn:_b.circIn)),properties:_45,beforeBegin:(_3c.unhide?function(){
if(_3c.fade){
_4.style(_40,{opacity:"0"});
}
ps.top=_45.top.start+"px";
}:undefined)});
return _46;
};
var _47=_d._split(_3c);
if(_3c.unhide){
_c.connect(_47,"onEnd",_47,function(){
_4.style(_3d,{opacity:"1"});
});
}else{
_c.connect(_47,"onPlay",_47,function(){
_4.style(_3d,{opacity:"0"});
});
}
return _47;
},build:function(_48){
_48.unhide=true;
return _d.disintegrate(_48);
},shear:function(_49){
var _4a=_49.node=_2.byId(_49.node);
_49.rows=_49.rows||6;
_49.columns=_49.columns||6;
_49.duration=_49.duration||1000;
_49.interval=_49.interval||0;
_49.distance=_49.distance||1;
_49.random=_49.random||0;
if(typeof (_49.fade)=="undefined"){
_49.fade=true;
}
var _4b=Math.abs(_49.random),_4c=(_49.duration-(_49.rows+_49.columns)*Math.abs(_49.interval));
_49.pieceAnimation=function(_4d,x,y,_4e){
var _4f=!(x%2),_50=!(y%2),_51=Math.random()*_4c,_52=(_49.reverseOrder)?(((_49.rows+_49.columns)-(x+y))*_49.interval):((x+y)*_49.interval),_53=_51*_4b+Math.max(1-_4b,0)*_52,_54={},ps=_4d.style;
if(_49.fade){
_54.opacity=(_49.unhide?{start:"0",end:"1"}:{end:"0"});
}
if(_49.columns==1){
_4f=_50;
}else{
if(_49.rows==1){
_50=!_4f;
}
}
var _55=parseInt(ps.left),top=parseInt(ps.top),_56=_49.distance*_4e.w,_57=_49.distance*_4e.h;
if(_49.unhide){
if(_4f==_50){
_54.left=_4f?{start:(_55-_56),end:_55}:{start:(_55+_56),end:_55};
}else{
_54.top=_4f?{start:(top+_57),end:top}:{start:(top-_57),end:top};
}
}else{
if(_4f==_50){
_54.left=_4f?{end:(_55-_56)}:{end:(_55+_56)};
}else{
_54.top=_4f?{end:(top+_57)}:{end:(top-_57)};
}
}
var _58=_8.animateProperty({node:_4d,duration:_4c,delay:_53,easing:(_49.easing||_b.sinInOut),properties:_54,beforeBegin:(_49.unhide?function(){
if(_49.fade){
ps.opacity="0";
}
if(_4f==_50){
ps.left=_54.left.start+"px";
}else{
ps.top=_54.top.start+"px";
}
}:undefined)});
return _58;
};
var _59=_d._split(_49);
if(_49.unhide){
_c.connect(_59,"onEnd",_59,function(){
_4.style(_4a,{opacity:"1"});
});
}else{
_c.connect(_59,"onPlay",_59,function(){
_4.style(_4a,{opacity:"0"});
});
}
return _59;
},unShear:function(_5a){
_5a.unhide=true;
return _d.shear(_5a);
},pinwheel:function(_5b){
var _5c=_5b.node=_2.byId(_5b.node);
_5b.rows=_5b.rows||4;
_5b.columns=_5b.columns||4;
_5b.duration=_5b.duration||1000;
_5b.interval=_5b.interval||0;
_5b.distance=_5b.distance||1;
_5b.random=_5b.random||0;
if(typeof _5b.fade=="undefined"){
_5b.fade=true;
}
var _5d=(_5b.duration-(_5b.rows+_5b.columns)*Math.abs(_5b.interval));
_5b.pieceAnimation=function(_5e,x,y,_5f){
var _60=_5f.h/_5b.rows,_61=_5f.w/_5b.columns,_62=!(x%2),_63=!(y%2),_64=Math.random()*_5d,_65=(_5b.interval<0)?(((_5b.rows+_5b.columns)-(x+y))*_5b.interval*-1):((x+y)*_5b.interval),_66=_64*_5b.random+Math.max(1-_5b.random,0)*_65,_67={},ps=_5e.style;
if(_5b.fade){
_67.opacity=(_5b.unhide?{start:0,end:1}:{end:0});
}
if(_5b.columns==1){
_62=!_63;
}else{
if(_5b.rows==1){
_63=_62;
}
}
var _68=parseInt(ps.left),top=parseInt(ps.top);
if(_62){
if(_63){
_67.top=_5b.unhide?{start:top+_60*_5b.distance,end:top}:{start:top,end:top+_60*_5b.distance};
}else{
_67.left=_5b.unhide?{start:_68+_61*_5b.distance,end:_68}:{start:_68,end:_68+_61*_5b.distance};
}
}
if(_62!=_63){
_67.width=_5b.unhide?{start:_61*(1-_5b.distance),end:_61}:{start:_61,end:_61*(1-_5b.distance)};
}else{
_67.height=_5b.unhide?{start:_60*(1-_5b.distance),end:_60}:{start:_60,end:_60*(1-_5b.distance)};
}
var _69=_8.animateProperty({node:_5e,duration:_5d,delay:_66,easing:(_5b.easing||_b.sinInOut),properties:_67,beforeBegin:(_5b.unhide?function(){
if(_5b.fade){
_4.style(_5e,"opacity",0);
}
if(_62){
if(_63){
ps.top=(top+_60*(1-_5b.distance))+"px";
}else{
ps.left=(_68+_61*(1-_5b.distance))+"px";
}
}else{
ps.left=_68+"px";
ps.top=top+"px";
}
if(_62!=_63){
ps.width=(_61*(1-_5b.distance))+"px";
}else{
ps.height=(_60*(1-_5b.distance))+"px";
}
}:undefined)});
return _69;
};
var _6a=_d._split(_5b);
if(_5b.unhide){
_c.connect(_6a,"onEnd",_6a,function(){
_4.style(_5c,{opacity:"1"});
});
}else{
_c.connect(_6a,"play",_6a,function(){
_4.style(_5c,{opacity:"0"});
});
}
return _6a;
},unPinwheel:function(_6b){
_6b.unhide=true;
return _d.pinwheel(_6b);
},blockFadeOut:function(_6c){
var _6d=_6c.node=_2.byId(_6c.node);
_6c.rows=_6c.rows||5;
_6c.columns=_6c.columns||5;
_6c.duration=_6c.duration||1000;
_6c.interval=_6c.interval||_6c.duration/(_6c.rows+_6c.columns*2);
_6c.random=_6c.random||0;
var _6e=Math.abs(_6c.random),_6f=_6c.duration-(_6c.rows+_6c.columns)*_6c.interval;
_6c.pieceAnimation=function(_70,x,y,_71){
var _72=Math.random()*_6c.duration,_73=(_6c.reverseOrder)?(((_6c.rows+_6c.columns)-(x+y))*Math.abs(_6c.interval)):((x+y)*_6c.interval),_74=_72*_6e+Math.max(1-_6e,0)*_73,_75=_8.animateProperty({node:_70,duration:_6f,delay:_74,easing:(_6c.easing||_b.sinInOut),properties:{opacity:(_6c.unhide?{start:"0",end:"1"}:{start:"1",end:"0"})},beforeBegin:(_6c.unhide?function(){
_4.style(_70,{opacity:"0"});
}:function(){
_70.style.filter="";
})});
return _75;
};
var _76=_d._split(_6c);
if(_6c.unhide){
_c.connect(_76,"onEnd",_76,function(){
_4.style(_6d,{opacity:"1"});
});
}else{
_c.connect(_76,"onPlay",_76,function(){
_4.style(_6d,{opacity:"0"});
});
}
return _76;
},blockFadeIn:function(_77){
_77.unhide=true;
return _d.blockFadeOut(_77);
}});
return _a;
});
