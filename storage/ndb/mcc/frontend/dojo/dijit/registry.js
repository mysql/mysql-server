//>>built
define("dijit/registry",["dojo/_base/array","dojo/sniff","dojo/_base/unload","dojo/_base/window","./main"],function(_1,_2,_3,_4,_5){
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
},findWidgets:function(_c,_d){
var _e=[];
function _f(_10){
for(var _11=_10.firstChild;_11;_11=_11.nextSibling){
if(_11.nodeType==1){
var _12=_11.getAttribute("widgetId");
if(_12){
var _13=_7[_12];
if(_13){
_e.push(_13);
}
}else{
if(_11!==_d){
_f(_11);
}
}
}
}
};
_f(_c);
return _e;
},_destroyAll:function(){
_5._curFocus=null;
_5._prevFocus=null;
_5._activeStack=[];
_1.forEach(_8.findWidgets(_4.body()),function(_14){
if(!_14._destroyed){
if(_14.destroyRecursive){
_14.destroyRecursive();
}else{
if(_14.destroy){
_14.destroy();
}
}
}
});
},getEnclosingWidget:function(_15){
while(_15){
var id=_15.nodeType==1&&_15.getAttribute("widgetId");
if(id){
return _7[id];
}
_15=_15.parentNode;
}
return null;
},_hash:_7};
_5.registry=_8;
return _8;
});
