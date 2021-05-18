//>>built
define("dojox/mobile/dh/HtmlContentHandler",["dojo/_base/kernel","dojo/_base/array","dojo/_base/declare","dojo/_base/Deferred","dojo/dom-class","dojo/dom-construct","dijit/registry","../lazyLoadUtils"],function(_1,_2,_3,_4,_5,_6,_7,_8){
return _3("dojox.mobile.dh.HtmlContentHandler",null,{parse:function(_9,_a,_b){
if(this.execScript){
_9=this.execScript(_9);
}
var _c=_6.create("div",{innerHTML:_9,style:{visibility:"hidden"}});
_a.insertBefore(_c,_b);
return _4.when(_8.instantiateLazyWidgets(_c),function(){
var _d,i,_e;
for(i=0,_e=_c.childNodes.length;i<_e;i++){
var n=_c.firstChild;
if(!_d&&n.nodeType===1){
_d=_7.byNode(n);
}
_a.insertBefore(_c.firstChild,_b);
}
_a.removeChild(_c);
if(!_d||!_5.contains(_d.domNode,"mblView")){
return null;
}
return _d.id;
});
}});
});
