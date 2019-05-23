//>>built
define("dojox/mobile/viewRegistry",["dojo/_base/array","dojo/dom-class","dijit/registry"],function(_1,_2,_3){
var _4={length:0,hash:{},initialView:null,add:function(_5){
this.hash[_5.id]=_5;
this.length++;
},remove:function(id){
if(this.hash[id]){
delete this.hash[id];
this.length--;
}
},getViews:function(){
var _6=[];
for(var i in this.hash){
_6.push(this.hash[i]);
}
return _6;
},getParentView:function(_7){
for(var v=_7.getParent();v;v=v.getParent()){
if(_2.contains(v.domNode,"mblView")){
return v;
}
}
return null;
},getChildViews:function(_8){
return _1.filter(this.getViews(),function(v){
return this.getParentView(v)===_8;
},this);
},getEnclosingView:function(_9){
for(var n=_9;n&&n.tagName!=="BODY";n=n.parentNode){
if(n.nodeType===1&&_2.contains(n,"mblView")){
return _3.byNode(n);
}
}
return null;
},getEnclosingScrollable:function(_a){
for(var w=_3.getEnclosingWidget(_a);w;w=w.getParent()){
if(w.scrollableParams&&w._v){
return w;
}
}
return null;
}};
return _4;
});
