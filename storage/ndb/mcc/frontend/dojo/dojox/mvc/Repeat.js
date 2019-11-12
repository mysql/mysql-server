//>>built
define("dojox/mvc/Repeat",["dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom","dojo/dom-construct","dojo/_base/array","dojo/query","dojo/when","dijit/registry","./_Container"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dojox.mvc.Repeat",_b,{index:0,useParent:"",removeRepeatNode:false,children:null,_relTargetProp:"children",startup:function(){
if(this.removeRepeatNode){
var _c=null;
if(_2.isFunction(this.getParent)){
if(this.getParent()){
this.select=this.getParent().select;
this.onCheckStateChanged=this.getParent().onCheckStateChanged;
}
}
}
this.inherited(arguments);
this._setChildrenAttr(this.children);
},postscript:function(_d,_e){
if(this.useParent&&_5.byId(this.useParent)){
this.srcNodeRef=_5.byId(this.useParent);
}else{
this.srcNodeRef=_5.byId(_e);
}
if(this.srcNodeRef){
var _f=this._attachTemplateNodes?"inlineTemplateString":"templateString";
if(this[_f]==""){
this[_f]=this.srcNodeRef.innerHTML;
}
try{
this.srcNodeRef.innerHTML="";
}
catch(e){
while(this.srcNodeRef.firstChild){
this.srcNodeRef.removeChild(this.srcNodeRef.firstChild);
}
}
}
this.inherited(arguments);
},_setChildrenAttr:function(_10){
var _11=this.children;
this._set("children",_10);
if(this.binding!=_10){
this.set("ref",_10);
}
if(this._started&&(!this._builtOnce||_11!=_10)){
this._builtOnce=true;
this._buildContained(_10);
}
},_buildContained:function(_12){
if(!_12){
return;
}
if(this.useParent&&_5.byId(this.useParent)){
this.srcNodeRef=_5.byId(this.useParent);
}
this._destroyBody();
this._updateAddRemoveWatch(_12);
var _13=[],_14=this._attachTemplateNodes?"inlineTemplateString":"templateString";
for(this.index=0;_2.isFunction(_12.get)?_12.get(this.index):_12[this.index];this.index++){
_13.push(this._exprRepl(this[_14]));
}
var _15=this.containerNode||this.srcNodeRef||this.domNode;
if(_3("ie")&&/^(table|tbody)$/i.test(_15.tagName)){
var div=_4.doc.createElement("div");
div.innerHTML="<table><tbody>"+_13.join("")+"</tbody></table>";
for(var _16=div.getElementsByTagName("tbody")[0];_16.firstChild;){
_15.appendChild(_16.firstChild);
}
}else{
if(_3("ie")&&/^td$/i.test(_15.tagName)){
var div=_4.doc.createElement("div");
div.innerHTML="<table><tbody><tr>"+_13.join("")+"</tr></tbody></table>";
for(var tr=div.getElementsByTagName("tr")[0];tr.firstChild;){
_15.appendChild(tr.firstChild);
}
}else{
_15.innerHTML=_13.join("");
}
}
this.srcNodeRef=_15;
var _17=this;
_9(this._createBody(),function(){
if(!_17.removeRepeatNode){
return;
}
var _18=_17.domNode;
if(!_17.savedParentId&&_17.domNode.parentNode&&_17.domNode.parentNode.id){
_17.savedParentId=_17.domNode.parentNode.id;
}
var _19=_5.byId(_17.savedParentId);
if(_18&&_18.children){
var t3=_a.findWidgets(_18);
var _1a=t3.length;
for(var j=_1a;j>0;j--){
if(t3[j-1].declaredClass=="dojox.mvc.Group"){
var cnt=_18.children[j-1].children.length;
var _1b=_a.byId(_19.id).select;
for(var i=cnt;i>0;i--){
_a.byId(_18.children[j-1].id).select=_1b;
_6.place(_18.children[j-1].removeChild(_18.children[j-1].children[i-1]),_19,"first");
}
}else{
_6.place(_18.removeChild(_18.children[j-1]),_19,"first");
}
}
_6.destroy(_18);
}
});
},_updateAddRemoveWatch:function(_1c){
if(this._addRemoveWatch){
this._addRemoveWatch.unwatch();
}
var _1d=this;
this._addRemoveWatch=_2.isFunction(_1c.watchElements)&&_1c.watchElements(function(idx,_1e,_1f){
if(!_1e||!_1f||_1e.length||_1f.length){
_1d._buildContained(_1d.children);
}
});
}});
});
