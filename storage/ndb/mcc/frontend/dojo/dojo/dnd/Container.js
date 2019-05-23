/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/dnd/Container",["../_base/array","../_base/declare","../_base/event","../_base/kernel","../_base/lang","../_base/window","../dom","../dom-class","../dom-construct","../Evented","../has","../on","../query","../ready","../touch","./common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,on,_c,_d,_e,_f){
var _10=_2("dojo.dnd.Container",_a,{skipForm:false,allowNested:false,constructor:function(_11,_12){
this.node=_7.byId(_11);
if(!_12){
_12={};
}
this.creator=_12.creator||null;
this.skipForm=_12.skipForm;
this.parent=_12.dropParent&&_7.byId(_12.dropParent);
this.map={};
this.current=null;
this.containerState="";
_8.add(this.node,"dojoDndContainer");
if(!(_12&&_12._skipStartup)){
this.startup();
}
this.events=[on(this.node,_e.over,_5.hitch(this,"onMouseOver")),on(this.node,_e.out,_5.hitch(this,"onMouseOut")),on(this.node,"dragstart",_5.hitch(this,"onSelectStart")),on(this.node,"selectstart",_5.hitch(this,"onSelectStart"))];
},creator:function(){
},getItem:function(key){
return this.map[key];
},setItem:function(key,_13){
this.map[key]=_13;
},delItem:function(key){
delete this.map[key];
},forInItems:function(f,o){
o=o||_4.global;
var m=this.map,e=_f._empty;
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
return _c((this.allowNested?"":"> ")+".dojoDndItem",this.parent);
},sync:function(){
var map={};
this.getAllNodes().forEach(function(_14){
if(_14.id){
var _15=this.getItem(_14.id);
if(_15){
map[_14.id]=_15;
return;
}
}else{
_14.id=_f.getUniqueId();
}
var _16=_14.getAttribute("dndType"),_17=_14.getAttribute("dndData");
map[_14.id]={data:_17||_14.innerHTML,type:_16?_16.split(/\s*,\s*/):["text"]};
},this);
this.map=map;
return this;
},insertNodes:function(_18,_19,_1a){
if(!this.parent.firstChild){
_1a=null;
}else{
if(_19){
if(!_1a){
_1a=this.parent.firstChild;
}
}else{
if(_1a){
_1a=_1a.nextSibling;
}
}
}
var i,t;
if(_1a){
for(i=0;i<_18.length;++i){
t=this._normalizedCreator(_18[i]);
this.setItem(t.node.id,{data:t.data,type:t.type});
_1a.parentNode.insertBefore(t.node,_1a);
}
}else{
for(i=0;i<_18.length;++i){
t=this._normalizedCreator(_18[i]);
this.setItem(t.node.id,{data:t.data,type:t.type});
this.parent.appendChild(t.node);
}
}
return this;
},destroy:function(){
_1.forEach(this.events,function(_1b){
_1b.remove();
});
this.clearItems();
this.node=this.parent=this.current=null;
},markupFactory:function(_1c,_1d,_1e){
_1c._skipStartup=true;
return new _1e(_1d,_1c);
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
this.defaultCreator=_f._defaultCreator(this.parent);
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
if(!this.skipForm||!_f.isFormElement(e)){
_3.stop(e);
}
},onOverEvent:function(){
},onOutEvent:function(){
},_changeState:function(_1f,_20){
var _21="dojoDnd"+_1f;
var _22=_1f.toLowerCase()+"State";
_8.replace(this.node,_21+_20,_21+this[_22]);
this[_22]=_20;
},_addItemClass:function(_23,_24){
_8.add(_23,"dojoDndItem"+_24);
},_removeItemClass:function(_25,_26){
_8.remove(_25,"dojoDndItem"+_26);
},_getChildByEvent:function(e){
var _27=e.target;
if(_27){
for(var _28=_27.parentNode;_28;_27=_28,_28=_27.parentNode){
if((_28==this.parent||this.allowNested)&&_8.contains(_27,"dojoDndItem")){
return _27;
}
}
}
return null;
},_normalizedCreator:function(_29,_2a){
var t=(this.creator||this.defaultCreator).call(this,_29,_2a);
if(!_5.isArray(t.type)){
t.type=["text"];
}
if(!t.node.id){
t.node.id=_f.getUniqueId();
}
_8.add(t.node,"dojoDndItem");
return t;
}});
_f._createNode=function(tag){
if(!tag){
return _f._createSpan;
}
return function(_2b){
return _9.create(tag,{innerHTML:_2b});
};
};
_f._createTrTd=function(_2c){
var tr=_9.create("tr");
_9.create("td",{innerHTML:_2c},tr);
return tr;
};
_f._createSpan=function(_2d){
return _9.create("span",{innerHTML:_2d});
};
_f._defaultCreatorNodes={ul:"li",ol:"li",div:"div",p:"div"};
_f._defaultCreator=function(_2e){
var tag=_2e.tagName.toLowerCase();
var c=tag=="tbody"||tag=="thead"?_f._createTrTd:_f._createNode(_f._defaultCreatorNodes[tag]);
return function(_2f,_30){
var _31=_2f&&_5.isObject(_2f),_32,_33,n;
if(_31&&_2f.tagName&&_2f.nodeType&&_2f.getAttribute){
_32=_2f.getAttribute("dndData")||_2f.innerHTML;
_33=_2f.getAttribute("dndType");
_33=_33?_33.split(/\s*,\s*/):["text"];
n=_2f;
}else{
_32=(_31&&_2f.data)?_2f.data:_2f;
_33=(_31&&_2f.type)?_2f.type:["text"];
n=(_30=="avatar"?_f._createSpan:c)(String(_32));
}
if(!n.id){
n.id=_f.getUniqueId();
}
return {node:n,data:_32,type:_33};
};
};
return _10;
});
