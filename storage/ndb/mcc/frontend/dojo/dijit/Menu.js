//>>built
define("dijit/Menu",["require","dojo/_base/array","dojo/_base/declare","dojo/_base/event","dojo/dom","dojo/dom-attr","dojo/dom-geometry","dojo/dom-style","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window","dojo/window","./popup","./DropDownMenu","dojo/ready"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,on,_c,_d,_e,pm,_f,_10){
if(!_9.isAsync){
_10(0,function(){
var _11=["dijit/MenuItem","dijit/PopupMenuItem","dijit/CheckedMenuItem","dijit/MenuSeparator"];
_1(_11);
});
}
return _3("dijit.Menu",_f,{constructor:function(){
this._bindings=[];
},targetNodeIds:[],contextMenuForWindow:false,leftClickToOpen:false,refocus:true,postCreate:function(){
if(this.contextMenuForWindow){
this.bindDomNode(_d.body());
}else{
_2.forEach(this.targetNodeIds,this.bindDomNode,this);
}
this.inherited(arguments);
},_iframeContentWindow:function(_12){
return _e.get(this._iframeContentDocument(_12))||this._iframeContentDocument(_12)["__parent__"]||(_12.name&&_d.doc.frames[_12.name])||null;
},_iframeContentDocument:function(_13){
return _13.contentDocument||(_13.contentWindow&&_13.contentWindow.document)||(_13.name&&_d.doc.frames[_13.name]&&_d.doc.frames[_13.name].document)||null;
},bindDomNode:function(_14){
_14=_5.byId(_14);
var cn;
if(_14.tagName.toLowerCase()=="iframe"){
var _15=_14,_16=this._iframeContentWindow(_15);
cn=_d.withGlobal(_16,_d.body);
}else{
cn=(_14==_d.body()?_d.doc.documentElement:_14);
}
var _17={node:_14,iframe:_15};
_6.set(_14,"_dijitMenu"+this.id,this._bindings.push(_17));
var _18=_b.hitch(this,function(cn){
return [on(cn,this.leftClickToOpen?"click":"contextmenu",_b.hitch(this,function(evt){
_4.stop(evt);
this._scheduleOpen(evt.target,_15,{x:evt.pageX,y:evt.pageY});
})),on(cn,"keydown",_b.hitch(this,function(evt){
if(evt.shiftKey&&evt.keyCode==_a.F10){
_4.stop(evt);
this._scheduleOpen(evt.target,_15);
}
}))];
});
_17.connects=cn?_18(cn):[];
if(_15){
_17.onloadHandler=_b.hitch(this,function(){
var _19=this._iframeContentWindow(_15);
cn=_d.withGlobal(_19,_d.body);
_17.connects=_18(cn);
});
if(_15.addEventListener){
_15.addEventListener("load",_17.onloadHandler,false);
}else{
_15.attachEvent("onload",_17.onloadHandler);
}
}
},unBindDomNode:function(_1a){
var _1b;
try{
_1b=_5.byId(_1a);
}
catch(e){
return;
}
var _1c="_dijitMenu"+this.id;
if(_1b&&_6.has(_1b,_1c)){
var bid=_6.get(_1b,_1c)-1,b=this._bindings[bid],h;
while(h=b.connects.pop()){
h.remove();
}
var _1d=b.iframe;
if(_1d){
if(_1d.removeEventListener){
_1d.removeEventListener("load",b.onloadHandler,false);
}else{
_1d.detachEvent("onload",b.onloadHandler);
}
}
_6.remove(_1b,_1c);
delete this._bindings[bid];
}
},_scheduleOpen:function(_1e,_1f,_20){
if(!this._openTimer){
this._openTimer=setTimeout(_b.hitch(this,function(){
delete this._openTimer;
this._openMyself({target:_1e,iframe:_1f,coords:_20});
}),1);
}
},_openMyself:function(_21){
var _22=_21.target,_23=_21.iframe,_24=_21.coords;
if(_24){
if(_23){
var ifc=_7.position(_23,true),_25=this._iframeContentWindow(_23),_26=_d.withGlobal(_25,"_docScroll",dojo);
var cs=_8.getComputedStyle(_23),tp=_8.toPixelValue,_27=(_c("ie")&&_c("quirks")?0:tp(_23,cs.paddingLeft))+(_c("ie")&&_c("quirks")?tp(_23,cs.borderLeftWidth):0),top=(_c("ie")&&_c("quirks")?0:tp(_23,cs.paddingTop))+(_c("ie")&&_c("quirks")?tp(_23,cs.borderTopWidth):0);
_24.x+=ifc.x+_27-_26.x;
_24.y+=ifc.y+top-_26.y;
}
}else{
_24=_7.position(_22,true);
_24.x+=10;
_24.y+=10;
}
var _28=this;
var _29=this._focusManager.get("prevNode");
var _2a=this._focusManager.get("curNode");
var _2b=!_2a||(_5.isDescendant(_2a,this.domNode))?_29:_2a;
function _2c(){
if(_28.refocus&&_2b){
_2b.focus();
}
pm.close(_28);
};
pm.open({popup:this,x:_24.x,y:_24.y,onExecute:_2c,onCancel:_2c,orient:this.isLeftToRight()?"L":"R"});
this.focus();
this._onBlur=function(){
this.inherited("_onBlur",arguments);
pm.close(this);
};
},uninitialize:function(){
_2.forEach(this._bindings,function(b){
if(b){
this.unBindDomNode(b.node);
}
},this);
this.inherited(arguments);
}});
});
