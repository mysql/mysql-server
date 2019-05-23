//>>built
(typeof define==="undefined"?function(_1,_2){
_2();
}:define)(["dojo/_base/config","dojo/_base/lang","dojo/_base/window","require"],function(_3,_4,_5,_6){
var dm=_4&&_4.getObject("dojox.mobile",true)||{};
var _7=function(){
if(!_5){
_5=window;
_5.doc=document;
_5._no_dojo_dm=dm;
}
_3=_3||_5.mblConfig||{};
var _8=_5.doc.getElementsByTagName("script");
for(var i=0;i<_8.length;i++){
var n=_8[i];
var _9=n.getAttribute("src")||"";
if(_9.match(/\/deviceTheme\.js/i)){
_3.baseUrl=_9.replace("deviceTheme.js","../../dojo/");
var _a=(n.getAttribute("data-dojo-config")||n.getAttribute("djConfig"));
if(_a){
var _b=eval("({ "+_a+" })");
for(var _c in _b){
_3[_c]=_b[_c];
}
}
break;
}else{
if(_9.match(/\/dojo\.js/i)){
_3.baseUrl=_9.replace("dojo.js","");
break;
}
}
}
this.loadCssFile=function(_d){
var _e=_5.doc.createElement("link");
_e.href=_d;
_e.type="text/css";
_e.rel="stylesheet";
var _f=_5.doc.getElementsByTagName("head")[0];
_f.insertBefore(_e,_f.firstChild);
dm.loadedCssFiles.push(_e);
};
this.toUrl=function(_10){
return _6?_6.toUrl(_10):_3.baseUrl+"../"+_10;
};
this.setDm=function(_11){
dm=_11;
};
this.themeMap=_3.themeMap||[["Android","android",[]],["BlackBerry","blackberry",[]],["BB10","blackberry",[]],["iPhone","iphone",[]],["iPad","iphone",[this.toUrl("dojox/mobile/themes/iphone/ipad.css")]],["Custom","custom",[]],[".*","iphone",[]]];
dm.loadedCssFiles=[];
this.loadDeviceTheme=function(_12){
var t=_3.mblThemeFiles||dm.themeFiles||["@theme"];
var i,j;
var m=this.themeMap;
var ua=_12||_3.mblUserAgent||(location.search.match(/theme=(\w+)/)?RegExp.$1:navigator.userAgent);
for(i=0;i<m.length;i++){
if(ua.match(new RegExp(m[i][0]))){
var _13=m[i][1];
var cls=_5.doc.documentElement.className;
cls=cls.replace(new RegExp(" *"+dm.currentTheme+"_theme"),"")+" "+_13+"_theme";
_5.doc.documentElement.className=cls;
dm.currentTheme=_13;
var _14=[].concat(m[i][2]);
for(j=0;j<t.length;j++){
var _15=(t[j] instanceof Array||typeof t[j]=="array");
var _16;
if(!_15&&t[j].indexOf("/")!==-1){
_16=t[j];
}else{
var pkg=_15?(t[j][0]||"").replace(/\./g,"/"):"dojox/mobile";
var _17=(_15?t[j][1]:t[j]).replace(/\./g,"/");
var f="themes/"+_13+"/"+(_17==="@theme"?_13:_17)+".css";
_16=pkg+"/"+f;
}
_14.unshift(this.toUrl(_16));
}
for(var k=0;k<dm.loadedCssFiles.length;k++){
var n=dm.loadedCssFiles[k];
n.parentNode.removeChild(n);
}
dm.loadedCssFiles=[];
for(j=0;j<_14.length;j++){
this.loadCssFile(_14[j].toString());
}
if(_12&&dm.loadCompatCssFiles){
dm.loadCompatCssFiles();
}
break;
}
}
};
};
var _18=new _7();
_18.loadDeviceTheme();
window.deviceTheme=dm.deviceTheme=_18;
return _18;
});
