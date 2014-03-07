//>>built
define("dojox/app/bind",["dojo/_base/kernel","dojo/query","dojo/_base/array","dijit","dojo/_base/json"],function(_1,_2,_3,_4,_5){
return function(_6,_7){
_3.forEach(_6,function(_8){
var _9=_2("div[dojoType^=\"dojox.mvc\"],div[data-dojo-type^=\"dojox.mvc\"]",_8.domNode);
_3.forEach(_9,function(_a){
var _b=_a.getAttribute("ref");
if(_b===null){
var _c=_a.getAttribute("data-dojo-props");
if(_c){
try{
_c=_5.fromJson("{"+_c+"}");
}
catch(e){
throw new Error(e.toString()+" in data-dojo-props='"+extra+"'");
}
_b=_c.ref.replace(/^\s*rel\s*:\s*/,"");
}
}
if(_b){
if(_b[0]==="'"){
_b=_b.substring(1,_b.length-1);
}
var _d=_1.getObject(_b,false,_7);
if(_d){
_4.byNode(_a).set("ref",_d);
}
}
},this);
},this);
};
});
