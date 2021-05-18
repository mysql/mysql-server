//>>built
define("dijit/focus",["dojo/aspect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-construct","dojo/Evented","dojo/_base/lang","dojo/on","dojo/domReady","dojo/sniff","dojo/Stateful","dojo/_base/window","dojo/window","./a11y","./registry","./main"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f,_10){
var _11;
var _12;
var _13=_2([_b,_7],{curNode:null,activeStack:[],constructor:function(){
var _14=_8.hitch(this,function(_15){
if(_3.isDescendant(this.curNode,_15)){
this.set("curNode",null);
}
if(_3.isDescendant(this.prevNode,_15)){
this.set("prevNode",null);
}
});
_1.before(_6,"empty",_14);
_1.before(_6,"destroy",_14);
},registerIframe:function(_16){
return this.registerWin(_16.contentWindow,_16);
},registerWin:function(_17,_18){
var _19=this,_1a=_17.document&&_17.document.body;
if(_1a){
var _1b=_a("pointer-events")?"pointerdown":_a("MSPointer")?"MSPointerDown":_a("touch-events")?"mousedown, touchstart":"mousedown";
var mdh=on(_17.document,_1b,function(evt){
if(evt&&evt.target&&evt.target.parentNode==null){
return;
}
_19._onTouchNode(_18||evt.target,"mouse");
});
var fih=on(_1a,"focusin",function(evt){
if(!evt.target.tagName){
return;
}
var tag=evt.target.tagName.toLowerCase();
if(tag=="#document"||tag=="body"){
return;
}
if(_e.isFocusable(evt.target)){
_19._onFocusNode(_18||evt.target);
}else{
_19._onTouchNode(_18||evt.target);
}
});
var foh=on(_1a,"focusout",function(evt){
_19._onBlurNode(_18||evt.target);
});
return {remove:function(){
mdh.remove();
fih.remove();
foh.remove();
mdh=fih=foh=null;
_1a=null;
}};
}
},_onBlurNode:function(_1c){
var now=(new Date()).getTime();
if(now<_11+100){
return;
}
if(this._clearFocusTimer){
clearTimeout(this._clearFocusTimer);
}
this._clearFocusTimer=setTimeout(_8.hitch(this,function(){
this.set("prevNode",this.curNode);
this.set("curNode",null);
}),0);
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
}
if(now<_12+100){
return;
}
this._clearActiveWidgetsTimer=setTimeout(_8.hitch(this,function(){
delete this._clearActiveWidgetsTimer;
this._setStack([]);
}),0);
},_onTouchNode:function(_1d,by){
_12=(new Date()).getTime();
if(this._clearActiveWidgetsTimer){
clearTimeout(this._clearActiveWidgetsTimer);
delete this._clearActiveWidgetsTimer;
}
if(_5.contains(_1d,"dijitPopup")){
_1d=_1d.firstChild;
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
_11=(new Date()).getTime();
if(this._clearFocusTimer){
clearTimeout(this._clearFocusTimer);
delete this._clearFocusTimer;
}
this._onTouchNode(_21);
if(_21==this.curNode){
return;
}
this.set("prevNode",this.curNode);
this.set("curNode",_21);
},_setStack:function(_22,by){
var _23=this.activeStack,_24=_23.length-1,_25=_22.length-1;
if(_22[_25]==_23[_24]){
return;
}
this.set("activeStack",_22);
var _26,i;
for(i=_24;i>=0&&_23[i]!=_22[i];i--){
_26=_f.byId(_23[i]);
if(_26){
_26._hasBeenBlurred=true;
_26.set("focused",false);
if(_26._focusManager==this){
_26._onBlur(by);
}
this.emit("widget-blur",_26,by);
}
}
for(i++;i<=_25;i++){
_26=_f.byId(_22[i]);
if(_26){
_26.set("focused",true);
if(_26._focusManager==this){
_26._onFocus(by);
}
this.emit("widget-focus",_26,by);
}
}
},focus:function(_27){
if(_27){
try{
_27.focus();
}
catch(e){
}
}
}});
var _28=new _13();
_9(function(){
var _29=_28.registerWin(_d.get(document));
if(_a("ie")){
on(window,"unload",function(){
if(_29){
_29.remove();
_29=null;
}
});
}
});
_10.focus=function(_2a){
_28.focus(_2a);
};
for(var _2b in _28){
if(!/^_/.test(_2b)){
_10.focus[_2b]=typeof _28[_2b]=="function"?_8.hitch(_28,_2b):_28[_2b];
}
}
_28.watch(function(_2c,_2d,_2e){
_10.focus[_2c]=_2e;
});
return _28;
});
