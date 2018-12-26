//>>built
define("dojox/mobile/sniff",["dojo/_base/window","dojo/_base/sniff"],function(_1,_2){
var ua=navigator.userAgent;
_2.add("bb",ua.indexOf("BlackBerry")>=0&&parseFloat(ua.split("Version/")[1])||undefined,undefined,true);
_2.add("android",parseFloat(ua.split("Android ")[1])||undefined,undefined,true);
if(ua.match(/(iPhone|iPod|iPad)/)){
var p=RegExp.$1.replace(/P/,"p");
var v=ua.match(/OS ([\d_]+)/)?RegExp.$1:"1";
var os=parseFloat(v.replace(/_/,".").replace(/_/g,""));
_2.add(p,os,undefined,true);
_2.add("iphone",os,undefined,true);
}
if(_2("webkit")){
_2.add("touch",(typeof _1.doc.documentElement.ontouchstart!="undefined"&&navigator.appVersion.indexOf("Mobile")!=-1)||!!_2("android"),undefined,true);
}
return _2;
});
