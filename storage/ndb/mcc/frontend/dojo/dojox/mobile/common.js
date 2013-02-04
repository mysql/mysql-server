//>>built
define("dojox/mobile/common",["dojo/_base/kernel","dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/ready","dijit/registry","./sniff","./uacss"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
var dm=_5.getObject("dojox.mobile",true);
dm.getScreenSize=function(){
return {h:_6.global.innerHeight||_6.doc.documentElement.clientHeight,w:_6.global.innerWidth||_6.doc.documentElement.clientWidth};
};
dm.updateOrient=function(){
var _e=dm.getScreenSize();
_7.replace(_6.doc.documentElement,_e.h>_e.w?"dj_portrait":"dj_landscape",_e.h>_e.w?"dj_landscape":"dj_portrait");
};
dm.updateOrient();
dm.tabletSize=500;
dm.detectScreenSize=function(_f){
var dim=dm.getScreenSize();
var sz=Math.min(dim.w,dim.h);
var _10,to;
if(sz>=dm.tabletSize&&(_f||(!this._sz||this._sz<dm.tabletSize))){
_10="phone";
to="tablet";
}else{
if(sz<dm.tabletSize&&(_f||(!this._sz||this._sz>=dm.tabletSize))){
_10="tablet";
to="phone";
}
}
if(to){
_7.replace(_6.doc.documentElement,"dj_"+to,"dj_"+_10);
_4.publish("/dojox/mobile/screenSize/"+to,[dim]);
}
this._sz=sz;
};
dm.detectScreenSize();
dm.setupIcon=function(_11,_12){
if(_11&&_12){
var arr=_2.map(_12.split(/[ ,]/),function(_13){
return _13-0;
});
var t=arr[0];
var r=arr[1]+arr[2];
var b=arr[0]+arr[3];
var l=arr[1];
_9.set(_11,{clip:"rect("+t+"px "+r+"px "+b+"px "+l+"px)",top:(_11.parentNode?_9.get(_11,"top"):0)-t+"px",left:-l+"px"});
}
};
dm.hideAddressBarWait=typeof (_3["mblHideAddressBarWait"])==="number"?_3["mblHideAddressBarWait"]:1500;
dm.hide_1=function(_14){
scrollTo(0,1);
var h=dm.getScreenSize().h+"px";
if(_c("android")){
if(_14){
_6.body().style.minHeight=h;
}
dm.resizeAll();
}else{
if(_14||dm._h===h&&h!==_6.body().style.minHeight){
_6.body().style.minHeight=h;
dm.resizeAll();
}
}
dm._h=h;
};
dm.hide_fs=function(){
var t=_6.body().style.minHeight;
_6.body().style.minHeight=(dm.getScreenSize().h*2)+"px";
scrollTo(0,1);
setTimeout(function(){
dm.hide_1(1);
dm._hiding=false;
},1000);
};
dm.hideAddressBar=function(evt){
if(dm.disableHideAddressBar||dm._hiding){
return;
}
dm._hiding=true;
dm._h=0;
_6.body().style.minHeight=(dm.getScreenSize().h*2)+"px";
setTimeout(dm.hide_1,0);
setTimeout(dm.hide_1,200);
setTimeout(dm.hide_1,800);
setTimeout(dm.hide_fs,dm.hideAddressBarWait);
};
dm.resizeAll=function(evt,_15){
if(dm.disableResizeAll){
return;
}
_4.publish("/dojox/mobile/resizeAll",[evt,_15]);
dm.updateOrient();
dm.detectScreenSize();
var _16=function(w){
var _17=w.getParent&&w.getParent();
return !!((!_17||!_17.resize)&&w.resize);
};
var _18=function(w){
_2.forEach(w.getChildren(),function(_19){
if(_16(_19)){
_19.resize();
}
_18(_19);
});
};
if(_15){
if(_15.resize){
_15.resize();
}
_18(_15);
}else{
_2.forEach(_2.filter(_b.toArray(),_16),function(w){
w.resize();
});
}
};
dm.openWindow=function(url,_1a){
_6.global.open(url,_1a||"_blank");
};
dm.createDomButton=function(_1b,_1c,_1d){
if(!dm._domButtons){
if(_c("webkit")){
var _1e=function(_1f,dic){
var i,j;
if(!_1f){
var dic={};
var ss=_1.doc.styleSheets;
for(i=0;i<ss.length;i++){
ss[i]&&_1e(ss[i],dic);
}
return dic;
}
var _20=_1f.cssRules||[];
for(i=0;i<_20.length;i++){
var _21=_20[i];
if(_21.href&&_21.styleSheet){
_1e(_21.styleSheet,dic);
}else{
if(_21.selectorText){
var _22=_21.selectorText.split(/,/);
for(j=0;j<_22.length;j++){
var sel=_22[j];
var n=sel.split(/>/).length-1;
if(sel.match(/(mblDomButton\w+)/)){
var cls=RegExp.$1;
if(!dic[cls]||n>dic[cls]){
dic[cls]=n;
}
}
}
}
}
}
};
dm._domButtons=_1e();
}else{
dm._domButtons={};
}
}
var s=_1b.className;
var _23=_1d||_1b;
if(s.match(/(mblDomButton\w+)/)&&s.indexOf("/")===-1){
var _24=RegExp.$1;
var _25=4;
if(s.match(/(mblDomButton\w+_(\d+))/)){
_25=RegExp.$2-0;
}else{
if(dm._domButtons[_24]!==undefined){
_25=dm._domButtons[_24];
}
}
var _26=null;
if(_c("bb")&&_3["mblBBBoxShadowWorkaround"]!==false){
_26={style:"-webkit-box-shadow:none"};
}
for(var i=0,p=_23;i<_25;i++){
p=p.firstChild||_8.create("DIV",_26,p);
}
if(_1d){
setTimeout(function(){
_7.remove(_1b,_24);
},0);
_7.add(_1d,_24);
}
}else{
if(s.indexOf(".")!==-1){
_8.create("IMG",{src:s},_23);
}else{
return null;
}
}
_7.add(_23,"mblDomButton");
if(_3["mblAndroidWorkaround"]!==false&&_c("android")>=2.2){
_9.set(_23,"webkitTransform","translate3d(0,0,0)");
}
!!_1c&&_9.set(_23,_1c);
return _23;
};
dm.createIcon=function(_27,_28,_29,_2a,_2b){
if(_27&&_27.indexOf("mblDomButton")===0){
if(_29&&_29.className.match(/(mblDomButton\w+)/)){
_7.remove(_29,RegExp.$1);
}else{
_29=_8.create("DIV");
}
_29.title=_2a;
_7.add(_29,_27);
dm.createDomButton(_29);
}else{
if(_27&&_27!=="none"){
if(!_29||_29.nodeName!=="IMG"){
_29=_8.create("IMG",{alt:_2a});
}
_29.src=(_27||"").replace("${theme}",dm.currentTheme);
dm.setupIcon(_29,_28);
if(_2b&&_28){
var arr=_28.split(/[ ,]/);
_9.set(_2b,{width:arr[2]+"px",height:arr[3]+"px"});
}
}
}
if(_2b){
_2b.appendChild(_29);
}
return _29;
};
dm._iw=_3["mblIosWorkaround"]!==false&&_c("iphone");
if(dm._iw){
dm._iwBgCover=_8.create("div");
}
if(_3.parseOnLoad){
_a(90,function(){
var _2c=_6.body().getElementsByTagName("*");
var i,len,s;
len=_2c.length;
for(i=0;i<len;i++){
s=_2c[i].getAttribute("dojoType");
if(s){
if(_2c[i].parentNode.getAttribute("lazy")=="true"){
_2c[i].setAttribute("__dojoType",s);
_2c[i].removeAttribute("dojoType");
}
}
}
});
}
_a(function(){
dm.detectScreenSize(true);
if(_3["mblApplyPageStyles"]!==false){
_7.add(_6.doc.documentElement,"mobile");
}
if(_c("chrome")){
_7.add(_6.doc.documentElement,"dj_chrome");
}
if(_3["mblAndroidWorkaround"]!==false&&_c("android")>=2.2){
if(_3["mblAndroidWorkaroundButtonStyle"]!==false){
_8.create("style",{innerHTML:"BUTTON,INPUT[type='button'],INPUT[type='submit'],INPUT[type='reset'],INPUT[type='file']::-webkit-file-upload-button{-webkit-appearance:none;}"},_6.doc.head,"first");
}
if(_c("android")<3){
_9.set(_6.doc.documentElement,"webkitTransform","translate3d(0,0,0)");
_4.connect(null,"onfocus",null,function(e){
_9.set(_6.doc.documentElement,"webkitTransform","");
});
_4.connect(null,"onblur",null,function(e){
_9.set(_6.doc.documentElement,"webkitTransform","translate3d(0,0,0)");
});
}else{
if(_3["mblAndroid3Workaround"]!==false){
_9.set(_6.doc.documentElement,{webkitBackfaceVisibility:"hidden",webkitPerspective:8000});
}
}
}
var f=dm.resizeAll;
if(_3["mblHideAddressBar"]!==false&&navigator.appVersion.indexOf("Mobile")!=-1||_3["mblForceHideAddressBar"]===true){
dm.hideAddressBar();
if(_3["mblAlwaysHideAddressBar"]===true){
f=dm.hideAddressBar;
}
}
_4.connect(null,(_6.global.onorientationchange!==undefined&&!_c("android"))?"onorientationchange":"onresize",null,f);
var _2d=_6.body().getElementsByTagName("*");
var i,len=_2d.length,s;
for(i=0;i<len;i++){
s=_2d[i].getAttribute("__dojoType");
if(s){
_2d[i].setAttribute("dojoType",s);
_2d[i].removeAttribute("__dojoType");
}
}
if(_1.hash){
var _2e=function(_2f){
if(!_2f){
return [];
}
var arr=_b.findWidgets(_2f);
var _30=arr;
for(var i=0;i<_30.length;i++){
arr=arr.concat(_2e(_30[i].containerNode));
}
return arr;
};
_4.subscribe("/dojo/hashchange",null,function(_31){
var _32=dm.currentView;
if(!_32){
return;
}
var _33=dm._params;
if(!_33){
var _34=_31?_31:dm._defaultView.id;
var _35=_2e(_32.domNode);
var dir=1,_36="slide";
for(i=0;i<_35.length;i++){
var w=_35[i];
if("#"+_34==w.moveTo){
_36=w.transition;
dir=(w instanceof dm.Heading)?-1:1;
break;
}
}
_33=[_34,dir,_36];
}
_32.performTransition.apply(_32,_33);
dm._params=null;
});
}
_6.body().style.visibility="visible";
});
_b.getEnclosingWidget=function(_37){
while(_37){
var id=_37.getAttribute&&_37.getAttribute("widgetId");
if(id){
return _b.byId(id);
}
_37=_37._parentNode||_37.parentNode;
}
return null;
};
return dm;
});
