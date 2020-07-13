//>>built
define("dijit/registry",["dojo/_base/array","dojo/_base/window","./main"],function(_1,_2,_3){
var _4={},_5={};
var _6={length:0,add:function(_7){
if(_5[_7.id]){
throw new Error("Tried to register widget with id=="+_7.id+" but that id is already registered");
}
_5[_7.id]=_7;
this.length++;
},remove:function(id){
if(_5[id]){
delete _5[id];
this.length--;
}
},byId:function(id){
return typeof id=="string"?_5[id]:id;
},byNode:function(_8){
return _5[_8.getAttribute("widgetId")];
},toArray:function(){
var ar=[];
for(var id in _5){
ar.push(_5[id]);
}
return ar;
},getUniqueId:function(_9){
var id;
do{
id=_9+"_"+(_9 in _4?++_4[_9]:_4[_9]=0);
}while(_5[id]);
return _3._scopeName=="dijit"?id:_3._scopeName+"_"+id;
},findWidgets:function(_a,_b){
var _c=[];
function _d(_e){
for(var _f=_e.firstChild;_f;_f=_f.nextSibling){
if(_f.nodeType==1){
var _10=_f.getAttribute("widgetId");
if(_10){
var _11=_5[_10];
if(_11){
_c.push(_11);
}
}else{
if(_f!==_b){
_d(_f);
}
}
}
}
};
_d(_a);
return _c;
},_destroyAll:function(){
_3._curFocus=null;
_3._prevFocus=null;
_3._activeStack=[];
_1.forEach(_6.findWidgets(_2.body()),function(_12){
if(!_12._destroyed){
if(_12.destroyRecursive){
_12.destroyRecursive();
}else{
if(_12.destroy){
_12.destroy();
}
}
}
});
},getEnclosingWidget:function(_13){
while(_13){
var id=_13.nodeType==1&&_13.getAttribute("widgetId");
if(id){
return _5[id];
}
_13=_13.parentNode;
}
return null;
},_hash:_5};
_3.registry=_6;
return _6;
});
