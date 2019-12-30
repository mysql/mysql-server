//>>built
define("dijit/focus",["dojo/aspect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/Evented","dojo/_base/lang","dojo/on","dojo/ready","dojo/sniff","dojo/Stateful","dojo/_base/unload","dojo/_base/window","dojo/window","./a11y","./registry","./main"],function(_1,_2,_3,_4,_5,_6,_7,on,_8,_9,_a,_b,_c,_d,_e,_f,_10){
var _11;
var _12=_2([_a,_6],{curNode:null,activeStack:[],constructor:function(){
var _13=_7.hitch(this,function(_14){
if(_3.isDescendant(this.curNode,_14)){
this.set("curNode",null);
}
if(_3.isDescendant(this.prevNode,_14)){
this.set("prevNode",null);
}
});
_1.before(_5,"empty",_13);
_1.before(_5,"destroy",_13);
},registerIframe:function(_15){
return this.registerWin(_15.contentWindow,_15);
},registerWin:function(_16,_17){
var _18=this,_19=_16.document&&_16.document.body;
if(_19){
var mdh=on(_19,"mousedown",function(evt){
_18._justMouseDowned=true;
setTimeout(function(){
_18._justMouseDowned=false;
},13);
if(evt&&evt.target&&evt.target.parentNode==null){
return;
}
_18._onTouchNode(_17||evt.target,"mouse");
});
var fih=on(_19,"focusin",function(evt){
_11=(new Date()).getTime();
if(!evt.target.tagName){
return;
}
var tag=evt.target.tagName.toLowerCase();
if(tag=="#document"||tag=="body"){
return;
}
if(_e.isFocusable(evt.target)){
_18._onFocusNode(_17||evt.target);
}else{
_18._onTouchNode(_17||evt.target);
}
});
var foh=on(_19,"focusout",function(evt){
if((new Date()).getTime()<_11+100){
return;
}
_18._onBlurNode(_17||evt.target);
});
return {remove:function(){
mdh.remove();
fih.remove();
foh.remove();
mdh=fih=foh=null;
_19=null;
}};
}
},_onBlurNode:function(_1a){
if(this._clearFocusTimer){
clearTimeout(this._clearFocusTimer);
}
this._clearFocusTimer=setTimeout(_7.hitch(this,function(){
this.set("prevNode",this.curNode);
this.set("curNode",null);
}),0);
if(this._justMouseDowned){
return;
}
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
}
this._clearActiveWidgetsTimer=setTimeout(_7.hitch(this,function(){
delete this._clearActiveWidgetsTimer;
this._setStack([]);
}),0);
},_onTouchNode:function(_1b,by){
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
delete this._clearActiveWidgetsTimer;
}
var _1c=[];
try{
while(_1b){
var _1d=_4.get(_1b,"dijitPopupParent");
if(_1d){
_1b=_f.byId(_1d).domNode;
}else{
if(_1b.tagName&&_1b.tagName.toLowerCase()=="body"){
if(_1b===_c.body()){
break;
}
_1b=_d.get(_1b.ownerDocument).frameElement;
}else{
var id=_1b.getAttribute&&_1b.getAttribute("widgetId"),_1e=id&&_f.byId(id);
if(_1e&&!(by=="mouse"&&_1e.get("disabled"))){
_1c.unshift(id);
}
_1b=_1b.parentNode;
}
}
}
}
catch(e){
}
this._setStack(_1c,by);
},_onFocusNode:function(_1f){
if(!_1f){
return;
}
if(_1f.nodeType==9){
return;
}
if(this._clearFocusTimer){
clearTimeout(this._clearFocusTimer);
delete this._clearFocusTimer;
}
this._onTouchNode(_1f);
if(_1f==this.curNode){
return;
}
this.set("prevNode",this.curNode);
this.set("curNode",_1f);
},_setStack:function(_20,by){
var _21=this.activeStack;
this.set("activeStack",_20);
for(var _22=0;_22<Math.min(_21.length,_20.length);_22++){
if(_21[_22]!=_20[_22]){
break;
}
}
var _23;
for(var i=_21.length-1;i>=_22;i--){
_23=_f.byId(_21[i]);
if(_23){
_23._hasBeenBlurred=true;
_23.set("focused",false);
if(_23._focusManager==this){
_23._onBlur(by);
}
this.emit("widget-blur",_23,by);
}
}
for(i=_22;i<_20.length;i++){
_23=_f.byId(_20[i]);
if(_23){
_23.set("focused",true);
if(_23._focusManager==this){
_23._onFocus(by);
}
this.emit("widget-focus",_23,by);
}
}
},focus:function(_24){
if(_24){
try{
_24.focus();
}
catch(e){
}
}
}});
var _25=new _12();
_8(function(){
var _26=_25.registerWin(_d.get(_c.doc));
if(_9("ie")){
_b.addOnWindowUnload(function(){
if(_26){
_26.remove();
_26=null;
}
});
}
});
_10.focus=function(_27){
_25.focus(_27);
};
for(var _28 in _25){
if(!/^_/.test(_28)){
_10.focus[_28]=typeof _25[_28]=="function"?_7.hitch(_25,_28):_25[_28];
}
}
_25.watch(function(_29,_2a,_2b){
_10.focus[_29]=_2b;
});
return _25;
});
