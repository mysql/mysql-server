//>>built
define("dojox/mobile/dh/SuffixFileTypeMap",["dojo/_base/lang"],function(_1){
var o={};
_1.setObject("dojox.mobile.dh.SuffixFileTypeMap",o);
o.map={"html":"html","json":"json"};
o.add=function(_2,_3){
this.map[_2]=_3;
};
o.getContentType=function(_4){
var _5=(_4||"").replace(/.*\./,"");
return this.map[_5];
};
return o;
});
