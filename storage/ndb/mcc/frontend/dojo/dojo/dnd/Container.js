/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Container",["../main","../Evented","./common","../parser"],function(_1,_2){
_1.declare("dojo.dnd.Container",_2,{skipForm:false,constructor:function(_3,_4){
this.node=_1.byId(_3);
if(!_4){
_4={};
}
this.creator=_4.creator||null;
this.skipForm=_4.skipForm;
this.parent=_4.dropParent&&_1.byId(_4.dropParent);
this.map={};
this.current=null;
this.containerState="";
_1.addClass(this.node,"dojoDndContainer");
if(!(_4&&_4._skipStartup)){
this.startup();
}
this.events=[_1.connect(this.node,"onmouseover",this,"onMouseOver"),_1.connect(this.node,"onmouseout",this,"onMouseOut"),_1.connect(this.node,"ondragstart",this,"onSelectStart"),_1.connect(this.node,"onselectstart",this,"onSelectStart")];
},creator:function(){
},getItem:function(_5){
return this.map[_5];
},setItem:function(_6,_7){
this.map[_6]=_7;
},delItem:function(_8){
delete this.map[_8];
},forInItems:function(f,o){
o=o||_1.global;
var m=this.map,e=_1.dnd._empty;
for(var i in m){
if(i in e){
continue;
}
f.call(o,m[i],i,this);
}
return o;
},clearItems:function(){
this.map={};
},getAllNodes:function(){
return _1.query("> .dojoDndItem",this.parent);
},sync:function(){
var _9={};
this.getAllNodes().forEach(function(_a){
if(_a.id){
var _b=this.getItem(_a.id);
if(_b){
_9[_a.id]=_b;
return;
}
}else{
_a.id=_1.dnd.getUniqueId();
}
var _c=_a.getAttribute("dndType"),_d=_a.getAttribute("dndData");
_9[_a.id]={data:_d||_a.innerHTML,type:_c?_c.split(/\s*,\s*/):["text"]};
},this);
this.map=_9;
return this;
},insertNodes:function(_e,_f,_10){
if(!this.parent.firstChild){
_10=null;
}else{
if(_f){
if(!_10){
_10=this.parent.firstChild;
}
}else{
if(_10){
_10=_10.nextSibling;
}
}
}
if(_10){
for(var i=0;i<_e.length;++i){
var t=this._normalizedCreator(_e[i]);
this.setItem(t.node.id,{data:t.data,type:t.type});
this.parent.insertBefore(t.node,_10);
}
}else{
for(var i=0;i<_e.length;++i){
var t=this._normalizedCreator(_e[i]);
this.setItem(t.node.id,{data:t.data,type:t.type});
this.parent.appendChild(t.node);
}
}
return this;
},destroy:function(){
_1.forEach(this.events,_1.disconnect);
this.clearItems();
this.node=this.parent=this.current=null;
},markupFactory:function(_11,_12,_13){
_11._skipStartup=true;
return new _13(_12,_11);
},startup:function(){
if(!this.parent){
this.parent=this.node;
if(this.parent.tagName.toLowerCase()=="table"){
var c=this.parent.getElementsByTagName("tbody");
if(c&&c.length){
this.parent=c[0];
}
}
}
this.defaultCreator=_1.dnd._defaultCreator(this.parent);
this.sync();
},onMouseOver:function(e){
var n=e.relatedTarget;
while(n){
if(n==this.node){
break;
}
try{
n=n.parentNode;
}
catch(x){
n=null;
}
}
if(!n){
this._changeState("Container","Over");
this.onOverEvent();
}
n=this._getChildByEvent(e);
if(this.current==n){
return;
}
if(this.current){
this._removeItemClass(this.current,"Over");
}
if(n){
this._addItemClass(n,"Over");
}
this.current=n;
},onMouseOut:function(e){
for(var n=e.relatedTarget;n;){
if(n==this.node){
return;
}
try{
n=n.parentNode;
}
catch(x){
n=null;
}
}
if(this.current){
this._removeItemClass(this.current,"Over");
this.current=null;
}
this._changeState("Container","");
this.onOutEvent();
},onSelectStart:function(e){
if(!this.skipForm||!_1.dnd.isFormElement(e)){
_1.stopEvent(e);
}
},onOverEvent:function(){
},onOutEvent:function(){
},_changeState:function(_14,_15){
var _16="dojoDnd"+_14;
var _17=_14.toLowerCase()+"State";
_1.replaceClass(this.node,_16+_15,_16+this[_17]);
this[_17]=_15;
},_addItemClass:function(_18,_19){
_1.addClass(_18,"dojoDndItem"+_19);
},_removeItemClass:function(_1a,_1b){
_1.removeClass(_1a,"dojoDndItem"+_1b);
},_getChildByEvent:function(e){
var _1c=e.target;
if(_1c){
for(var _1d=_1c.parentNode;_1d;_1c=_1d,_1d=_1c.parentNode){
if(_1d==this.parent&&_1.hasClass(_1c,"dojoDndItem")){
return _1c;
}
}
}
return null;
},_normalizedCreator:function(_1e,_1f){
var t=(this.creator||this.defaultCreator).call(this,_1e,_1f);
if(!_1.isArray(t.type)){
t.type=["text"];
}
if(!t.node.id){
t.node.id=_1.dnd.getUniqueId();
}
_1.addClass(t.node,"dojoDndItem");
return t;
}});
_1.dnd._createNode=function(tag){
if(!tag){
return _1.dnd._createSpan;
}
return function(_20){
return _1.create(tag,{innerHTML:_20});
};
};
_1.dnd._createTrTd=function(_21){
var tr=_1.create("tr");
_1.create("td",{innerHTML:_21},tr);
return tr;
};
_1.dnd._createSpan=function(_22){
return _1.create("span",{innerHTML:_22});
};
_1.dnd._defaultCreatorNodes={ul:"li",ol:"li",div:"div",p:"div"};
_1.dnd._defaultCreator=function(_23){
var tag=_23.tagName.toLowerCase();
var c=tag=="tbody"||tag=="thead"?_1.dnd._createTrTd:_1.dnd._createNode(_1.dnd._defaultCreatorNodes[tag]);
return function(_24,_25){
var _26=_24&&_1.isObject(_24),_27,_28,n;
if(_26&&_24.tagName&&_24.nodeType&&_24.getAttribute){
_27=_24.getAttribute("dndData")||_24.innerHTML;
_28=_24.getAttribute("dndType");
_28=_28?_28.split(/\s*,\s*/):["text"];
n=_24;
}else{
_27=(_26&&_24.data)?_24.data:_24;
_28=(_26&&_24.type)?_24.type:["text"];
n=(_25=="avatar"?_1.dnd._createSpan:c)(String(_27));
}
if(!n.id){
n.id=_1.dnd.getUniqueId();
}
return {node:n,data:_27,type:_28};
};
};
return _1.dnd.Container;
});
