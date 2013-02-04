//>>built
define("dijit/registry",["dojo/_base/array","dojo/_base/sniff","dojo/_base/unload","dojo/_base/window","."],function(_1,_2,_3,_4,_5){
var _6={},_7={};
var _8={length:0,add:function(_9){
if(_7[_9.id]){
throw new Error("Tried to register widget with id=="+_9.id+" but that id is already registered");
}
_7[_9.id]=_9;
this.length++;
},remove:function(id){
if(_7[id]){
delete _7[id];
this.length--;
}
},byId:function(id){
return typeof id=="string"?_7[id]:id;
},byNode:function(_a){
return _7[_a.getAttribute("widgetId")];
},toArray:function(){
var ar=[];
for(var id in _7){
ar.push(_7[id]);
}
return ar;
},getUniqueId:function(_b){
var id;
do{
id=_b+"_"+(_b in _6?++_6[_b]:_6[_b]=0);
}while(_7[id]);
return _5._scopeName=="dijit"?id:_5._scopeName+"_"+id;
},findWidgets:function(_c){
var _d=[];
function _e(_f){
for(var _10=_f.firstChild;_10;_10=_10.nextSibling){
if(_10.nodeType==1){
var _11=_10.getAttribute("widgetId");
if(_11){
var _12=_7[_11];
if(_12){
_d.push(_12);
}
}else{
_e(_10);
}
}
}
};
_e(_c);
return _d;
},_destroyAll:function(){
_5._curFocus=null;
_5._prevFocus=null;
_5._activeStack=[];
_1.forEach(_8.findWidgets(_4.body()),function(_13){
if(!_13._destroyed){
if(_13.destroyRecursive){
_13.destroyRecursive();
}else{
if(_13.destroy){
_13.destroy();
}
}
}
});
},getEnclosingWidget:function(_14){
while(_14){
var id=_14.getAttribute&&_14.getAttribute("widgetId");
if(id){
return _7[id];
}
_14=_14.parentNode;
}
return null;
},_hash:_7};
if(_2("ie")){
_3.addOnWindowUnload(function(){
_8._destroyAll();
});
}
_5.registry=_8;
return _8;
});
