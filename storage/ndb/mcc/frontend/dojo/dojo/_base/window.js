/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/_base/window",["./kernel","../has","./sniff"],function(_1,_2){
_1.doc=this["document"]||null;
_1.body=function(){
return _1.doc.body||_1.doc.getElementsByTagName("body")[0];
};
_1.setContext=function(_3,_4){
_1.global=_5.global=_3;
_1.doc=_5.doc=_4;
};
_1.withGlobal=function(_6,_7,_8,_9){
var _a=_1.global;
try{
_1.global=_5.global=_6;
return _1.withDoc.call(null,_6.document,_7,_8,_9);
}
finally{
_1.global=_5.global=_a;
}
};
_1.withDoc=function(_b,_c,_d,_e){
var _f=_1.doc,_10=_1.isQuirks,_11=_1.isIE,_12,_13,_14;
try{
_1.doc=_5.doc=_b;
_1.isQuirks=_2.add("quirks",_1.doc.compatMode=="BackCompat",true,true);
if(_2("ie")){
if((_14=_b.parentWindow)&&_14.navigator){
_12=parseFloat(_14.navigator.appVersion.split("MSIE ")[1])||undefined;
_13=_b.documentMode;
if(_13&&_13!=5&&Math.floor(_12)!=_13){
_12=_13;
}
_1.isIE=_2.add("ie",_12,true,true);
}
}
if(_d&&typeof _c=="string"){
_c=_d[_c];
}
return _c.apply(_d,_e||[]);
}
finally{
_1.doc=_5.doc=_f;
_1.isQuirks=_2.add("quirks",_10,true,true);
_1.isIE=_2.add("ie",_11,true,true);
}
};
var _5={global:_1.global,doc:_1.doc,body:_1.body,setContext:_1.setContext,withGlobal:_1.withGlobal,withDoc:_1.withDoc};
return _5;
});
