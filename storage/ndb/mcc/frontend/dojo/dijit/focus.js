//>>built
define("dijit/focus",["dojo/aspect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/Evented","dojo/_base/lang","dojo/on","dojo/ready","dojo/_base/sniff","dojo/Stateful","dojo/_base/unload","dojo/_base/window","dojo/window","./a11y","./registry","."],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11=_2([_a,_6],{curNode:null,activeStack:[],constructor:function(){
var _12=_7.hitch(this,function(_13){
if(_3.isDescendant(this.curNode,_13)){
this.set("curNode",null);
}
if(_3.isDescendant(this.prevNode,_13)){
this.set("prevNode",null);
}
});
_1.before(_5,"empty",_12);
_1.before(_5,"destroy",_12);
},registerIframe:function(_14){
return this.registerWin(_14.contentWindow,_14);
},registerWin:function(_15,_16){
var _17=this;
var _18=function(evt){
_17._justMouseDowned=true;
setTimeout(function(){
_17._justMouseDowned=false;
},0);
if(_9("ie")&&evt&&evt.srcElement&&evt.srcElement.parentNode==null){
return;
}
_17._onTouchNode(_16||evt.target||evt.srcElement,"mouse");
};
var doc=_9("ie")?_15.document.documentElement:_15.document;
if(doc){
if(_9("ie")){
_15.document.body.attachEvent("onmousedown",_18);
var _19=function(evt){
var tag=evt.srcElement.tagName.toLowerCase();
if(tag=="#document"||tag=="body"){
return;
}
if(_e.isTabNavigable(evt.srcElement)){
_17._onFocusNode(_16||evt.srcElement);
}else{
_17._onTouchNode(_16||evt.srcElement);
}
};
doc.attachEvent("onactivate",_19);
var _1a=function(evt){
_17._onBlurNode(_16||evt.srcElement);
};
doc.attachEvent("ondeactivate",_1a);
return {remove:function(){
_15.document.detachEvent("onmousedown",_18);
doc.detachEvent("onactivate",_19);
doc.detachEvent("ondeactivate",_1a);
doc=null;
}};
}else{
doc.body.addEventListener("mousedown",_18,true);
doc.body.addEventListener("touchstart",_18,true);
var _1b=function(evt){
_17._onFocusNode(_16||evt.target);
};
doc.addEventListener("focus",_1b,true);
var _1c=function(evt){
_17._onBlurNode(_16||evt.target);
};
doc.addEventListener("blur",_1c,true);
return {remove:function(){
doc.body.removeEventListener("mousedown",_18,true);
doc.body.removeEventListener("touchstart",_18,true);
doc.removeEventListener("focus",_1b,true);
doc.removeEventListener("blur",_1c,true);
doc=null;
}};
}
}
},_onBlurNode:function(){
this.set("prevNode",this.curNode);
this.set("curNode",null);
if(this._justMouseDowned){
return;
}
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
}
this._clearActiveWidgetsTimer=setTimeout(_7.hitch(this,function(){
delete this._clearActiveWidgetsTimer;
this._setStack([]);
this.prevNode=null;
}),100);
},_onTouchNode:function(_1d,by){
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
delete this._clearActiveWidgetsTimer;
}
var _1e=[];
try{
while(_1d){
var _1f=_4.get(_1d,"dijitPopupParent");
if(_1f){
_1d=_f.byId(_1f).domNode;
}else{
if(_1d.tagName&&_1d.tagName.toLowerCase()=="body"){
if(_1d===_c.body()){
break;
}
_1d=_d.get(_1d.ownerDocument).frameElement;
}else{
var id=_1d.getAttribute&&_1d.getAttribute("widgetId"),_20=id&&_f.byId(id);
if(_20&&!(by=="mouse"&&_20.get("disabled"))){
_1e.unshift(id);
}
_1d=_1d.parentNode;
}
}
}
}
catch(e){
}
this._setStack(_1e,by);
},_onFocusNode:function(_21){
if(!_21){
return;
}
if(_21.nodeType==9){
return;
}
this._onTouchNode(_21);
if(_21==this.curNode){
return;
}
this.set("curNode",_21);
},_setStack:function(_22,by){
var _23=this.activeStack;
this.set("activeStack",_22);
for(var _24=0;_24<Math.min(_23.length,_22.length);_24++){
if(_23[_24]!=_22[_24]){
break;
}
}
var _25;
for(var i=_23.length-1;i>=_24;i--){
_25=_f.byId(_23[i]);
if(_25){
_25._hasBeenBlurred=true;
_25.set("focused",false);
if(_25._focusManager==this){
_25._onBlur(by);
}
this.emit("widget-blur",_25,by);
}
}
for(i=_24;i<_22.length;i++){
_25=_f.byId(_22[i]);
if(_25){
_25.set("focused",true);
if(_25._focusManager==this){
_25._onFocus(by);
}
this.emit("widget-focus",_25,by);
}
}
},focus:function(_26){
if(_26){
try{
_26.focus();
}
catch(e){
}
}
}});
var _27=new _11();
_8(function(){
var _28=_27.registerWin(_c.doc.parentWindow||_c.doc.defaultView);
if(_9("ie")){
_b.addOnWindowUnload(function(){
_28.remove();
_28=null;
});
}
});
_10.focus=function(_29){
_27.focus(_29);
};
for(var _2a in _27){
if(!/^_/.test(_2a)){
_10.focus[_2a]=typeof _27[_2a]=="function"?_7.hitch(_27,_2a):_27[_2a];
}
}
_27.watch(function(_2b,_2c,_2d){
_10.focus[_2b]=_2d;
});
return _27;
});
