//>>built
define("dojox/mvc/parserExtension",["require","dojo/_base/kernel","dojo/_base/lang","dojo/has!dojo-parser?:dojo/_base/window","dojo/has","dojo/has!dojo-mobile-parser?:dojo/parser","dojo/has!dojo-parser?:dojox/mobile/parser","dojox/mvc/_atBindingMixin","dojox/mvc/Element"],function(_1,_2,_3,_4,_5,_6,_7,_8){
_5.add("dom-qsa",!!document.createElement("div").querySelectorAll);
try{
_5.add("dojo-parser",!!_1("dojo/parser"));
}
catch(e){
}
try{
_5.add("dojo-mobile-parser",!!_1("dojox/mobile/parser"));
}
catch(e){
}
if(_5("dojo-parser")){
var _9=_6.scan;
_6.scan=function(_a,_b){
return _9.apply(this,_3._toArray(arguments)).then(function(_c){
var _d=(_b.scope||_2._scopeName)+"Type",_e="data-"+(_b.scope||_2._scopeName)+"-",_f=_e+"type";
for(var _10=_5("dom-qsa")?_a.querySelectorAll("["+_8.prototype.dataBindAttr+"]"):_a.getElementsByTagName("*"),i=0,l=_10.length;i<l;i++){
var _11=_10[i],_12=false;
if(!_11.getAttribute(_f)&&!_11.getAttribute(_d)&&_11.getAttribute(_8.prototype.dataBindAttr)){
_c.push({types:["dojox/mvc/Element"],node:_11});
}
}
return _c;
});
};
}
if(_5("dojo-mobile-parser")){
var _13=_7.parse;
_7.parse=function(_14,_15){
var _16=((_15||{}).scope||_2._scopeName)+"Type",_17="data-"+((_15||{}).scope||_2._scopeName)+"-",_18=_17+"type";
nodes=_5("dom-qsa")?(_14||_4.body()).querySelectorAll("["+_8.prototype.dataBindAttr+"]"):(_14||_4.body()).getElementsByTagName("*");
for(var i=0,l=nodes.length;i<l;i++){
var _19=nodes[i],_1a=false,_1b=[];
if(!_19.getAttribute(_18)&&!_19.getAttribute(_16)&&_19.getAttribute(_8.prototype.dataBindAttr)){
_19.setAttribute(_18,"dojox/mvc/Element");
}
}
return _13.apply(this,_3._toArray(arguments));
};
}
return _6||_7;
});
