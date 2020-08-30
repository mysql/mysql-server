//>>built
define("dojox/mobile/common",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/lang","dojo/_base/window","dojo/_base/kernel","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/domReady","dojo/ready","dojo/touch","dijit/registry","./sniff","./uacss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e){
var dm=_4.getObject("dojox.mobile",true);
_5.doc.dojoClick=true;
if(_e("touch")){
_e.add("clicks-prevented",!(_e("android")>=4.1||(_e("ie")===10)||(!_e("ie")&&_e("trident")>6)));
if(_e("clicks-prevented")){
dm._sendClick=function(_f,e){
for(var _10=_f;_10;_10=_10.parentNode){
if(_10.dojoClick){
return;
}
}
var ev=_5.doc.createEvent("MouseEvents");
ev.initMouseEvent("click",true,true,_5.global,1,e.screenX,e.screenY,e.clientX,e.clientY);
_f.dispatchEvent(ev);
};
}
}
dm.getScreenSize=function(){
return {h:_5.global.innerHeight||_5.doc.documentElement.clientHeight,w:_5.global.innerWidth||_5.doc.documentElement.clientWidth};
};
dm.updateOrient=function(){
var dim=dm.getScreenSize();
_8.replace(_5.doc.documentElement,dim.h>dim.w?"dj_portrait":"dj_landscape",dim.h>dim.w?"dj_landscape":"dj_portrait");
};
dm.updateOrient();
dm.tabletSize=500;
dm.detectScreenSize=function(_11){
var dim=dm.getScreenSize();
var sz=Math.min(dim.w,dim.h);
var _12,to;
if(sz>=dm.tabletSize&&(_11||(!this._sz||this._sz<dm.tabletSize))){
_12="phone";
to="tablet";
}else{
if(sz<dm.tabletSize&&(_11||(!this._sz||this._sz>=dm.tabletSize))){
_12="tablet";
to="phone";
}
}
if(to){
_8.replace(_5.doc.documentElement,"dj_"+to,"dj_"+_12);
_3.publish("/dojox/mobile/screenSize/"+to,[dim]);
}
this._sz=sz;
};
dm.detectScreenSize();
dm.hideAddressBarWait=typeof (_2.mblHideAddressBarWait)==="number"?_2.mblHideAddressBarWait:1500;
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
dm.hideAddressBar=function(evt){
if(dm.disableHideAddressBar||dm._hiding){
return;
}
dm._hiding=true;
dm._hidingTimer=_e("ios")?200:0;
var _13=screen.availHeight;
if(_e("android")){
_13=outerHeight/devicePixelRatio;
if(_13==0){
dm._hiding=false;
setTimeout(function(){
dm.hideAddressBar();
},200);
}
if(_13<=innerHeight){
_13=outerHeight;
}
if(_e("android")<3){
_5.doc.documentElement.style.overflow=_5.body().style.overflow="visible";
}
}
if(_5.body().offsetHeight<_13){
_5.body().style.minHeight=_13+"px";
dm._resetMinHeight=true;
}
setTimeout(dm.hide_1,dm._hidingTimer);
};
dm.isAddressBarHidden=function(){
return pageYOffset===1;
};
dm.resizeAll=function(evt,_14){
if(dm.disableResizeAll){
return;
}
_3.publish("/dojox/mobile/resizeAll",[evt,_14]);
_3.publish("/dojox/mobile/beforeResizeAll",[evt,_14]);
if(dm._resetMinHeight){
_5.body().style.minHeight=dm.getScreenSize().h+"px";
}
dm.updateOrient();
dm.detectScreenSize();
var _15=function(w){
var _16=w.getParent&&w.getParent();
return !!((!_16||!_16.resize)&&w.resize);
};
var _17=function(w){
_1.forEach(w.getChildren(),function(_18){
if(_15(_18)){
_18.resize();
}
_17(_18);
});
};
if(_14){
if(_14.resize){
_14.resize();
}
_17(_14);
}else{
_1.forEach(_1.filter(_d.toArray(),_15),function(w){
w.resize();
});
}
_3.publish("/dojox/mobile/afterResizeAll",[evt,_14]);
};
dm.openWindow=function(url,_19){
_5.global.open(url,_19||"_blank");
};
dm._detectWindowsTheme=function(){
if(navigator.userAgent.match(/IEMobile\/10\.0/)){
_9.create("style",{innerHTML:"@-ms-viewport {width: auto !important}"},_5.doc.head);
}
var _1a=function(){
_8.add(_5.doc.documentElement,"windows_theme");
_6.experimental("Dojo Mobile Windows theme","Behavior and appearance of the Windows theme are experimental.");
};
var _1b=_e("windows-theme");
if(_1b!==undefined){
if(_1b){
_1a();
}
return;
}
var i,j;
var _1c=function(_1d){
if(_1d&&_1d.indexOf("/windows/")!==-1){
_e.add("windows-theme",true);
_1a();
return true;
}
return false;
};
var s=_5.doc.styleSheets;
for(i=0;i<s.length;i++){
if(s[i].href){
continue;
}
var r=s[i].cssRules||s[i].imports;
if(!r){
continue;
}
for(j=0;j<r.length;j++){
if(_1c(r[j].href)){
return;
}
}
}
var _1e=_5.doc.getElementsByTagName("link");
for(i=0;i<_1e.length;i++){
if(_1c(_1e[i].href)){
return;
}
}
};
if(_2.mblApplyPageStyles!==false){
_8.add(_5.doc.documentElement,"mobile");
}
if(_e("chrome")){
_8.add(_5.doc.documentElement,"dj_chrome");
}
if(_5.global._no_dojo_dm){
var _1f=_5.global._no_dojo_dm;
for(var i in _1f){
dm[i]=_1f[i];
}
dm.deviceTheme.setDm(dm);
}
_e.add("mblAndroidWorkaround",_2.mblAndroidWorkaround!==false&&_e("android")<3,undefined,true);
_e.add("mblAndroid3Workaround",_2.mblAndroid3Workaround!==false&&_e("android")>=3,undefined,true);
dm._detectWindowsTheme();
dm.setSelectable=function(_20,_21){
var _22,i;
_20=_7.byId(_20);
if(_e("ie")<=9){
_22=_20.getElementsByTagName("*");
i=_22.length;
if(_21){
_20.removeAttribute("unselectable");
while(i--){
_22[i].removeAttribute("unselectable");
}
}else{
_20.setAttribute("unselectable","on");
while(i--){
if(_22[i].tagName!=="INPUT"){
_22[i].setAttribute("unselectable","on");
}
}
}
}else{
_8.toggle(_20,"unselectable",!_21);
}
};
var _23=_e("pointer-events")?"touchAction":_e("MSPointer")?"msTouchAction":null;
dm._setTouchAction=_23?function(_24,_25){
_24.style[_23]=_25;
}:function(){
};
_a(function(){
if(_2.mblApplyPageStyles!==false){
_8.add(_5.body(),"mblBackground");
}
});
_b(function(){
dm.detectScreenSize(true);
if(_2.mblAndroidWorkaroundButtonStyle!==false&&_e("android")){
_9.create("style",{innerHTML:"BUTTON,INPUT[type='button'],INPUT[type='submit'],INPUT[type='reset'],INPUT[type='file']::-webkit-file-upload-button{-webkit-appearance:none;} audio::-webkit-media-controls-play-button,video::-webkit-media-controls-play-button{-webkit-appearance:media-play-button;} video::-webkit-media-controls-fullscreen-button{-webkit-appearance:media-fullscreen-button;}"},_5.doc.head,"first");
}
if(_e("mblAndroidWorkaround")){
_9.create("style",{innerHTML:".mblView.mblAndroidWorkaround{position:absolute;top:-9999px !important;left:-9999px !important;}"},_5.doc.head,"last");
}
var f=dm.resizeAll;
var _26=navigator.appVersion.indexOf("Mobile")!=-1&&!(_e("ios")>=7);
if((_2.mblHideAddressBar!==false&&_26)||_2.mblForceHideAddressBar===true){
dm.hideAddressBar();
if(_2.mblAlwaysHideAddressBar===true){
f=dm.hideAddressBar;
}
}
var _27=_e("ios")>=6;
if((_e("android")||_27)&&_5.global.onorientationchange!==undefined){
var _28=f;
var _29,_2a,_2b;
if(_27){
_2a=_5.doc.documentElement.clientWidth;
_2b=_5.doc.documentElement.clientHeight;
}else{
f=function(evt){
var _2c=_3.connect(null,"onresize",null,function(e){
_3.disconnect(_2c);
_28(e);
});
};
_29=dm.getScreenSize();
}
_3.connect(null,"onresize",null,function(e){
if(_27){
var _2d=_5.doc.documentElement.clientWidth,_2e=_5.doc.documentElement.clientHeight;
if(_2d==_2a&&_2e!=_2b){
_28(e);
}
_2a=_2d;
_2b=_2e;
}else{
var _2f=dm.getScreenSize();
if(_2f.w==_29.w&&Math.abs(_2f.h-_29.h)>=100){
_28(e);
}
_29=_2f;
}
});
}
_3.connect(null,_5.global.onorientationchange!==undefined?"onorientationchange":"onresize",null,f);
_5.body().style.visibility="visible";
});
return dm;
});
