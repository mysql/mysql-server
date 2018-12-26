/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/sniff",["./kernel","../has"],function(_1,_2){
if(!1){
return _2;
}
_1.isBrowser=true,_1._name="browser";
var _3=_2.add,n=navigator,_4=n.userAgent,_5=n.appVersion,tv=parseFloat(_5),_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14;
if(_4.indexOf("AdobeAIR")>=0){
_7=1;
}
_8=(_5.indexOf("Konqueror")>=0)?tv:0;
_9=parseFloat(_4.split("WebKit/")[1])||undefined;
_a=parseFloat(_4.split("Chrome/")[1])||undefined;
_b=_5.indexOf("Macintosh")>=0;
_12=/iPhone|iPod|iPad/.test(_4);
_13=parseFloat(_4.split("Android ")[1])||undefined;
_14=typeof opera!="undefined"&&opera.wiiremote;
var _15=Math.max(_5.indexOf("WebKit"),_5.indexOf("Safari"),0);
if(_15&&!_a){
_c=parseFloat(_5.split("Version/")[1]);
if(!_c||parseFloat(_5.substr(_15+7))<=419.3){
_c=2;
}
}
if(!_2("dojo-webkit")){
if(_4.indexOf("Opera")>=0){
_6=tv;
if(_6>=9.8){
_6=parseFloat(_4.split("Version/")[1])||tv;
}
}
if(_4.indexOf("Gecko")>=0&&!_8&&!_9){
_d=_e=tv;
}
if(_e){
_10=parseFloat(_4.split("Firefox/")[1]||_4.split("Minefield/")[1])||undefined;
}
if(document.all&&!_6){
_f=parseFloat(_5.split("MSIE ")[1])||undefined;
var _16=document.documentMode;
if(_16&&_16!=5&&Math.floor(_f)!=_16){
_f=_16;
}
}
}
_11=document.compatMode=="BackCompat";
_3("opera",_1.isOpera=_6);
_3("air",_1.isAIR=_7);
_3("khtml",_1.isKhtml=_8);
_3("webkit",_1.isWebKit=_9);
_3("chrome",_1.isChrome=_a);
_3("mac",_1.isMac=_b);
_3("safari",_1.isSafari=_c);
_3("mozilla",_1.isMozilla=_1.isMoz=_d);
_3("ie",_1.isIE=_f);
_3("ff",_1.isFF=_10);
_3("quirks",_1.isQuirks=_11);
_3("ios",_1.isIos=_12);
_3("android",_1.isAndroid=_13);
_1.locale=_1.locale||(_f?n.userLanguage:n.language).toLowerCase();
return _2;
});
