/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/currency",["./_base/kernel","./_base/lang","./_base/array","./number","./i18n","./i18n!./cldr/nls/currency","./cldr/monetary"],function(_1,_2,_3,_4,_5,_6,_7){
_2.getObject("currency",true,_1);
_1.currency._mixInDefaults=function(_8){
_8=_8||{};
_8.type="currency";
var _9=_5.getLocalization("dojo.cldr","currency",_8.locale)||{};
var _a=_8.currency;
var _b=_7.getData(_a);
_3.forEach(["displayName","symbol","group","decimal"],function(_c){
_b[_c]=_9[_a+"_"+_c];
});
_b.fractional=[true,false];
return _2.mixin(_b,_8);
};
_1.currency.format=function(_d,_e){
return _4.format(_d,_1.currency._mixInDefaults(_e));
};
_1.currency.regexp=function(_f){
return _4.regexp(_1.currency._mixInDefaults(_f));
};
_1.currency.parse=function(_10,_11){
return _4.parse(_10,_1.currency._mixInDefaults(_11));
};
return _1.currency;
});
