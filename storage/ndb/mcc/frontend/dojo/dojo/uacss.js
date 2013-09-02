/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/uacss",["./dom-geometry","./_base/lang","./ready","./_base/sniff","./_base/window"],function(_1,_2,_3,_4,_5){
var _6=_5.doc.documentElement,ie=_4("ie"),_7=_4("opera"),_8=Math.floor,ff=_4("ff"),_9=_1.boxModel.replace(/-/,""),_a={"dj_ie":ie,"dj_ie6":_8(ie)==6,"dj_ie7":_8(ie)==7,"dj_ie8":_8(ie)==8,"dj_ie9":_8(ie)==9,"dj_quirks":_4("quirks"),"dj_iequirks":ie&&_4("quirks"),"dj_opera":_7,"dj_khtml":_4("khtml"),"dj_webkit":_4("webkit"),"dj_safari":_4("safari"),"dj_chrome":_4("chrome"),"dj_gecko":_4("mozilla"),"dj_ff3":_8(ff)==3};
_a["dj_"+_9]=true;
var _b="";
for(var _c in _a){
if(_a[_c]){
_b+=_c+" ";
}
}
_6.className=_2.trim(_6.className+" "+_b);
_3(90,function(){
if(!_1.isBodyLtr()){
var _d="dj_rtl dijitRtl "+_b.replace(/ /g,"-rtl ");
_6.className=_2.trim(_6.className+" "+_d+"dj_rtl dijitRtl "+_b.replace(/ /g,"-rtl "));
}
});
return _4;
});
