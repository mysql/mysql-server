//>>built
define("dijit/_base/wai",["dojo/dom-attr","dojo/_base/lang","..","../hccss"],function(_1,_2,_3){
_2.mixin(_3,{hasWaiRole:function(_4,_5){
var _6=this.getWaiRole(_4);
return _5?(_6.indexOf(_5)>-1):(_6.length>0);
},getWaiRole:function(_7){
return _2.trim((_1.get(_7,"role")||"").replace("wairole:",""));
},setWaiRole:function(_8,_9){
_1.set(_8,"role",_9);
},removeWaiRole:function(_a,_b){
var _c=_1.get(_a,"role");
if(!_c){
return;
}
if(_b){
var t=_2.trim((" "+_c+" ").replace(" "+_b+" "," "));
_1.set(_a,"role",t);
}else{
_a.removeAttribute("role");
}
},hasWaiState:function(_d,_e){
return _d.hasAttribute?_d.hasAttribute("aria-"+_e):!!_d.getAttribute("aria-"+_e);
},getWaiState:function(_f,_10){
return _f.getAttribute("aria-"+_10)||"";
},setWaiState:function(_11,_12,_13){
_11.setAttribute("aria-"+_12,_13);
},removeWaiState:function(_14,_15){
_14.removeAttribute("aria-"+_15);
}});
return _3;
});
