//>>built
define("dojox/mobile/common",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/ready","dijit/registry","./sniff","./uacss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
var dm=_4.getObject("dojox.mobile",true);
dm.getScreenSize=function(){
return {h:_5.global.innerHeight||_5.doc.documentElement.clientHeight,w:_5.global.innerWidth||_5.doc.documentElement.clientWidth};
};
dm.updateOrient=function(){
var _b=dm.getScreenSize();
_6.replace(_5.doc.documentElement,_b.h>_b.w?"dj_portrait":"dj_landscape",_b.h>_b.w?"dj_landscape":"dj_portrait");
};
dm.updateOrient();
dm.tabletSize=500;
dm.detectScreenSize=function(_c){
var _d=dm.getScreenSize();
var sz=Math.min(_d.w,_d.h);
var _e,to;
if(sz>=dm.tabletSize&&(_c||(!this._sz||this._sz<dm.tabletSize))){
_e="phone";
to="tablet";
}else{
if(sz<dm.tabletSize&&(_c||(!this._sz||this._sz>=dm.tabletSize))){
_e="tablet";
to="phone";
}
}
if(to){
_6.replace(_5.doc.documentElement,"dj_"+to,"dj_"+_e);
_3.publish("/dojox/mobile/screenSize/"+to,[_d]);
}
this._sz=sz;
};
dm.detectScreenSize();
dm.hideAddressBarWait=typeof (_2["mblHideAddressBarWait"])==="number"?_2["mblHideAddressBarWait"]:1500;
dm.hide_1=function(){
scrollTo(0,1);
dm._hidingTimer=(dm._hidingTimer==0)?200:dm._hidingTimer*2;
setTimeout(function(){
if(dm.isAddressBarHidden()||dm._hidingTimer>dm.hideAddressBarWait){
dm.resizeAll();
dm._hiding=false;
}else{
setTimeout(dm.hide_1,dm._hidingTimer);
}
},50);
};
dm.hideAddressBar=function(_f){
if(dm.disableHideAddressBar||dm._hiding){
return;
}
dm._hiding=true;
dm._hidingTimer=_a("iphone")?200:0;
var _10=screen.availHeight;
if(_a("android")){
_10=outerHeight/devicePixelRatio;
if(_10==0){
dm._hiding=false;
setTimeout(function(){
dm.hideAddressBar();
},200);
}
if(_10<=innerHeight){
_10=outerHeight;
}
if(_a("android")<3){
_5.doc.documentElement.style.overflow=_5.body().style.overflow="visible";
}
}
if(_5.body().offsetHeight<_10){
_5.body().style.minHeight=_10+"px";
dm._resetMinHeight=true;
}
setTimeout(dm.hide_1,dm._hidingTimer);
};
dm.isAddressBarHidden=function(){
return pageYOffset===1;
};
dm.resizeAll=function(evt,_11){
if(dm.disableResizeAll){
return;
}
_3.publish("/dojox/mobile/resizeAll",[evt,_11]);
_3.publish("/dojox/mobile/beforeResizeAll",[evt,_11]);
if(dm._resetMinHeight){
_5.body().style.minHeight=dm.getScreenSize().h+"px";
}
dm.updateOrient();
dm.detectScreenSize();
var _12=function(w){
var _13=w.getParent&&w.getParent();
return !!((!_13||!_13.resize)&&w.resize);
};
var _14=function(w){
_1.forEach(w.getChildren(),function(_15){
if(_12(_15)){
_15.resize();
}
_14(_15);
});
};
if(_11){
if(_11.resize){
_11.resize();
}
_14(_11);
}else{
_1.forEach(_1.filter(_9.toArray(),_12),function(w){
w.resize();
});
}
_3.publish("/dojox/mobile/afterResizeAll",[evt,_11]);
};
dm.openWindow=function(url,_16){
_5.global.open(url,_16||"_blank");
};
if(_2["mblApplyPageStyles"]!==false){
_6.add(_5.doc.documentElement,"mobile");
}
if(_a("chrome")){
_6.add(_5.doc.documentElement,"dj_chrome");
}
if(_5.global._no_dojo_dm){
var _17=_5.global._no_dojo_dm;
for(var i in _17){
dm[i]=_17[i];
}
dm.deviceTheme.setDm(dm);
}
_a.add("mblAndroidWorkaround",_2["mblAndroidWorkaround"]!==false&&_a("android")<3,undefined,true);
_a.add("mblAndroid3Workaround",_2["mblAndroid3Workaround"]!==false&&_a("android")>=3,undefined,true);
_8(function(){
dm.detectScreenSize(true);
if(_2["mblAndroidWorkaroundButtonStyle"]!==false&&_a("android")){
_7.create("style",{innerHTML:"BUTTON,INPUT[type='button'],INPUT[type='submit'],INPUT[type='reset'],INPUT[type='file']::-webkit-file-upload-button{-webkit-appearance:none;}"},_5.doc.head,"first");
}
if(_a("mblAndroidWorkaround")){
_7.create("style",{innerHTML:".mblView.mblAndroidWorkaround{position:absolute;top:-9999px !important;left:-9999px !important;}"},_5.doc.head,"last");
}
var f=dm.resizeAll;
var _18=navigator.appVersion.indexOf("Mobile")!=-1&&!(_a("iphone")>=7);
if((_2.mblHideAddressBar!==false&&_18)||_2.mblForceHideAddressBar===true){
dm.hideAddressBar();
if(_2.mblAlwaysHideAddressBar===true){
f=dm.hideAddressBar;
}
}
var _19=_a("iphone")>=6;
if((_a("android")||_19)&&_5.global.onorientationchange!==undefined){
var _1a=f;
var _1b,_1c,_1d;
if(_19){
_1c=_5.doc.documentElement.clientWidth;
_1d=_5.doc.documentElement.clientHeight;
}else{
f=function(evt){
var _1e=_3.connect(null,"onresize",null,function(e){
_3.disconnect(_1e);
_1a(e);
});
};
_1b=dm.getScreenSize();
}
_3.connect(null,"onresize",null,function(e){
if(_19){
var _1f=_5.doc.documentElement.clientWidth,_20=_5.doc.documentElement.clientHeight;
if(_1f==_1c&&_20!=_1d){
_1a(e);
}
_1c=_1f;
_1d=_20;
}else{
var _21=dm.getScreenSize();
if(_21.w==_1b.w&&Math.abs(_21.h-_1b.h)>=100){
_1a(e);
}
_1b=_21;
}
});
}
_3.connect(null,_5.global.onorientationchange!==undefined?"onorientationchange":"onresize",null,f);
_5.body().style.visibility="visible";
});
return dm;
});
