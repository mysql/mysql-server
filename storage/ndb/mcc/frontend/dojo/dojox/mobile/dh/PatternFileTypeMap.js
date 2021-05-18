//>>built
define("dojox/mobile/dh/PatternFileTypeMap",["dojo/_base/lang"],function(_1){
var o={};
_1.setObject("dojox.mobile.dh.PatternFileTypeMap",o);
o.map={".*.html":"html",".*.json":"json"};
o.add=function(_2,_3){
this.map[_2]=_3;
};
o.getContentType=function(_4){
for(var _5 in this.map){
if((new RegExp(_5)).test(_4)){
return this.map[_5];
}
}
return null;
};
return o;
});
