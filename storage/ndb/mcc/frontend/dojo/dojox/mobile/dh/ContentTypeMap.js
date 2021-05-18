//>>built
define("dojox/mobile/dh/ContentTypeMap",["dojo/_base/lang"],function(_1){
var o={};
_1.setObject("dojox.mobile.dh.ContentTypeMap",o);
o.map={"html":"dojox/mobile/dh/HtmlContentHandler","json":"dojox/mobile/dh/JsonContentHandler"};
o.add=function(_2,_3){
this.map[_2]=_3;
};
o.getHandlerClass=function(_4){
return this.map[_4];
};
return o;
});
