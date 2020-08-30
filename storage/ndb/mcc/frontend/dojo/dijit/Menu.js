//>>built
define("dijit/Menu",["require","dojo/_base/array","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-geometry","dojo/dom-style","dojo/keys","dojo/_base/lang","dojo/on","dojo/sniff","dojo/_base/window","dojo/window","./popup","./DropDownMenu","dojo/ready"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,on,_a,_b,_c,pm,_d,_e){
if(_a("dijit-legacy-requires")){
_e(0,function(){
var _f=["dijit/MenuItem","dijit/PopupMenuItem","dijit/CheckedMenuItem","dijit/MenuSeparator"];
_1(_f);
});
}
return _3("dijit.Menu",_d,{constructor:function(){
this._bindings=[];
},targetNodeIds:[],selector:"",contextMenuForWindow:false,leftClickToOpen:false,refocus:true,postCreate:function(){
if(this.contextMenuForWindow){
this.bindDomNode(this.ownerDocumentBody);
}else{
_2.forEach(this.targetNodeIds,this.bindDomNode,this);
}
this.inherited(arguments);
},_iframeContentWindow:function(_10){
return _c.get(this._iframeContentDocument(_10))||this._iframeContentDocument(_10)["__parent__"]||(_10.name&&document.frames[_10.name])||null;
},_iframeContentDocument:function(_11){
return _11.contentDocument||(_11.contentWindow&&_11.contentWindow.document)||(_11.name&&document.frames[_11.name]&&document.frames[_11.name].document)||null;
},bindDomNode:function(_12){
_12=_4.byId(_12,this.ownerDocument);
var cn;
if(_12.tagName.toLowerCase()=="iframe"){
var _13=_12,_14=this._iframeContentWindow(_13);
cn=_b.body(_14.document);
}else{
cn=(_12==_b.body(this.ownerDocument)?this.ownerDocument.documentElement:_12);
}
var _15={node:_12,iframe:_13};
_5.set(_12,"_dijitMenu"+this.id,this._bindings.push(_15));
var _16=_9.hitch(this,function(cn){
var _17=this.selector,_18=_17?function(_19){
return on.selector(_17,_19);
}:function(_1a){
return _1a;
},_1b=this;
return [on(cn,_18(this.leftClickToOpen?"click":"contextmenu"),function(evt){
evt.stopPropagation();
evt.preventDefault();
if((new Date()).getTime()<_1b._lastKeyDown+500){
return;
}
_1b._scheduleOpen(this,_13,{x:evt.pageX,y:evt.pageY},evt.target);
}),on(cn,_18("keydown"),function(evt){
if(evt.keyCode==93||(evt.shiftKey&&evt.keyCode==_8.F10)||(_1b.leftClickToOpen&&evt.keyCode==_8.SPACE)){
evt.stopPropagation();
evt.preventDefault();
_1b._scheduleOpen(this,_13,null,evt.target);
_1b._lastKeyDown=(new Date()).getTime();
}
})];
});
_15.connects=cn?_16(cn):[];
if(_13){
_15.onloadHandler=_9.hitch(this,function(){
var _1c=this._iframeContentWindow(_13),cn=_b.body(_1c.document);
_15.connects=_16(cn);
});
if(_13.addEventListener){
_13.addEventListener("load",_15.onloadHandler,false);
}else{
_13.attachEvent("onload",_15.onloadHandler);
}
}
},unBindDomNode:function(_1d){
var _1e;
try{
_1e=_4.byId(_1d,this.ownerDocument);
}
catch(e){
return;
}
var _1f="_dijitMenu"+this.id;
if(_1e&&_5.has(_1e,_1f)){
var bid=_5.get(_1e,_1f)-1,b=this._bindings[bid],h;
while((h=b.connects.pop())){
h.remove();
}
var _20=b.iframe;
if(_20){
if(_20.removeEventListener){
_20.removeEventListener("load",b.onloadHandler,false);
}else{
_20.detachEvent("onload",b.onloadHandler);
}
}
_5.remove(_1e,_1f);
delete this._bindings[bid];
}
},_scheduleOpen:function(_21,_22,_23,_24){
if(!this._openTimer){
this._openTimer=this.defer(function(){
delete this._openTimer;
this._openMyself({target:_24,delegatedTarget:_21,iframe:_22,coords:_23});
},1);
}
},_openMyself:function(_25){
var _26=_25.target,_27=_25.iframe,_28=_25.coords,_29=!_28;
this.currentTarget=_25.delegatedTarget;
if(_28){
if(_27){
var ifc=_6.position(_27,true),_2a=this._iframeContentWindow(_27),_2b=_6.docScroll(_2a.document);
var cs=_7.getComputedStyle(_27),tp=_7.toPixelValue,_2c=(_a("ie")&&_a("quirks")?0:tp(_27,cs.paddingLeft))+(_a("ie")&&_a("quirks")?tp(_27,cs.borderLeftWidth):0),top=(_a("ie")&&_a("quirks")?0:tp(_27,cs.paddingTop))+(_a("ie")&&_a("quirks")?tp(_27,cs.borderTopWidth):0);
_28.x+=ifc.x+_2c-_2b.x;
_28.y+=ifc.y+top-_2b.y;
}
}else{
_28=_6.position(_26,true);
_28.x+=10;
_28.y+=10;
}
var _2d=this;
var _2e=this._focusManager.get("prevNode");
var _2f=this._focusManager.get("curNode");
var _30=!_2f||(_4.isDescendant(_2f,this.domNode))?_2e:_2f;
function _31(){
if(_2d.refocus&&_30){
_30.focus();
}
pm.close(_2d);
};
pm.open({popup:this,x:_28.x,y:_28.y,onExecute:_31,onCancel:_31,orient:this.isLeftToRight()?"L":"R"});
this.focus();
if(!_29){
this.defer(function(){
this._cleanUp(true);
});
}
this._onBlur=function(){
this.inherited("_onBlur",arguments);
pm.close(this);
};
},destroy:function(){
_2.forEach(this._bindings,function(b){
if(b){
this.unBindDomNode(b.node);
}
},this);
this.inherited(arguments);
}});
});
