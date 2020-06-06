//>>built
define("dojox/gfx/svgext",["dojo/dom","dojo/_base/array","dojo/_base/window","./_base","./svg"],function(_1,_2,_3,_4,_5){
var _6=_4.svgext={};
var _7={primitives:null,tag:null,children:null};
function _8(_9,_a){
var _b=_a.ownerDocument.createElementNS(_5.xmlns.svg,_9.tag);
_a.appendChild(_b);
for(var p in _9){
if(!(p in _7)){
_b.setAttribute(p,_9[p]);
}
}
if(_9.children){
_2.forEach(_9.children,function(f){
_8(f,_b);
});
}
return _b;
};
_5.Shape.extend({addRenderingOption:function(_c,_d){
this.rawNode.setAttribute(_c,_d);
return this;
},setFilter:function(_e){
if(!_e){
this.rawNode.removeAttribute("filter");
this.filter=null;
return this;
}
this.filter=_e;
_e.id=_e.id||_4._base._getUniqueId();
var _f=_1.byId(_e.id);
if(!_f){
_f=this.rawNode.ownerDocument.createElementNS(_5.xmlns.svg,"filter");
_f.setAttribute("filterUnits","userSpaceOnUse");
for(var p in _e){
if(!(p in _7)){
_f.setAttribute(p,_e[p]);
}
}
_2.forEach(_e.primitives,function(p){
_8(p,_f);
});
var _10=this._getParentSurface();
if(_10){
var _11=_10.defNode;
_11.appendChild(_f);
}
}
this.rawNode.setAttribute("filter","url(#"+_e.id+")");
return this;
},getFilter:function(){
return this.filter;
}});
return _6;
});
