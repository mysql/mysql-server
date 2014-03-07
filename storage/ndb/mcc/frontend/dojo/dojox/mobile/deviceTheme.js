//>>built
define("dojox/mobile/deviceTheme",["dojo/_base/array","dojo/_base/config","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/dom-construct","require"],function(_1,_2,_3,_4,_5,_6,_7){
var dm=_3.getObject("dojox.mobile",true);
dm.loadCssFile=function(_8){
dm.loadedCssFiles.push(_6.create("LINK",{href:_8,type:"text/css",rel:"stylesheet"},_4.doc.getElementsByTagName("head")[0]));
};
dm.themeMap=dm.themeMap||[["Android","android",[]],["BlackBerry","blackberry",[]],["iPad","iphone",[_7.toUrl("dojox/mobile/themes/iphone/ipad.css")]],["Custom","custom",[]],[".*","iphone",[]]];
dm.loadDeviceTheme=function(_9){
var t=_2["mblThemeFiles"]||dm.themeFiles||["@theme"];
if(!_3.isArray(t)){
}
var i,j;
var m=dm.themeMap;
var ua=_9||_2["mblUserAgent"]||(location.search.match(/theme=(\w+)/)?RegExp.$1:navigator.userAgent);
for(i=0;i<m.length;i++){
if(ua.match(new RegExp(m[i][0]))){
var _a=m[i][1];
_5.replace(_4.doc.documentElement,_a+"_theme",dm.currentTheme?dm.currentTheme+"_theme":"");
dm.currentTheme=_a;
var _b=[].concat(m[i][2]);
for(j=t.length-1;j>=0;j--){
var _c=_3.isArray(t[j])?(t[j][0]||"").replace(/\./g,"/"):"dojox/mobile";
var _d=_3.isArray(t[j])?t[j][1]:t[j];
var f="themes/"+_a+"/"+(_d==="@theme"?_a:_d)+".css";
_b.unshift(_7.toUrl(_c+"/"+f));
}
_1.forEach(dm.loadedCssFiles,function(n){
n.parentNode.removeChild(n);
});
dm.loadedCssFiles=[];
for(j=0;j<_b.length;j++){
dm.loadCssFile(_b[j].toString());
}
if(_9&&dm.loadCompatCssFiles){
dm.loadCompatCssFiles();
}
break;
}
}
};
if(dm.configDeviceTheme){
dm.configDeviceTheme();
}
dm.loadDeviceTheme();
return dm;
});
