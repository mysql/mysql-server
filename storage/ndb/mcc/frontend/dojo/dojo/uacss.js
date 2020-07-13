/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/uacss",["./dom-geometry","./_base/lang","./domReady","./sniff","./_base/window"],function(_1,_2,_3,_4,_5){
var _6=_5.doc.documentElement,ie=_4("ie"),_7=_4("trident"),_8=_4("opera"),_9=Math.floor,ff=_4("ff"),_a=_1.boxModel.replace(/-/,""),_b={"dj_quirks":_4("quirks"),"dj_opera":_8,"dj_khtml":_4("khtml"),"dj_webkit":_4("webkit"),"dj_safari":_4("safari"),"dj_chrome":_4("chrome"),"dj_edge":_4("edge"),"dj_gecko":_4("mozilla"),"dj_ios":_4("ios"),"dj_android":_4("android")};
if(ie){
_b["dj_ie"]=true;
_b["dj_ie"+_9(ie)]=true;
_b["dj_iequirks"]=_4("quirks");
}
if(_7){
_b["dj_trident"]=true;
_b["dj_trident"+_9(_7)]=true;
}
if(ff){
_b["dj_ff"+_9(ff)]=true;
}
_b["dj_"+_a]=true;
var _c="";
for(var _d in _b){
if(_b[_d]){
_c+=_d+" ";
}
}
_6.className=_2.trim(_6.className+" "+_c);
_3(function(){
if(!_1.isBodyLtr()){
var _e="dj_rtl dijitRtl "+_c.replace(/ /g,"-rtl ");
_6.className=_2.trim(_6.className+" "+_e+"dj_rtl dijitRtl "+_c.replace(/ /g,"-rtl "));
}
});
return _4;
});
