//>>built
define("dijit/popup",["dojo/_base/array","dojo/aspect","dojo/_base/connect","dojo/_base/declare","dojo/dom","dojo/dom-attr","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/on","dojo/_base/sniff","dojo/_base/window","./place","./BackgroundIframe","."],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,on,_d,_e,_f,_10,_11){
var _12=_4(null,{_stack:[],_beginZIndex:1000,_idGen:1,_createWrapper:function(_13){
var _14=_13._popupWrapper,_15=_13.domNode;
if(!_14){
_14=_7.create("div",{"class":"dijitPopup",style:{display:"none"},role:"presentation"},_e.body());
_14.appendChild(_15);
var s=_15.style;
s.display="";
s.visibility="";
s.position="";
s.top="0px";
_13._popupWrapper=_14;
_2.after(_13,"destroy",function(){
_7.destroy(_14);
delete _13._popupWrapper;
});
}
return _14;
},moveOffScreen:function(_16){
var _17=this._createWrapper(_16);
_9.set(_17,{visibility:"hidden",top:"-9999px",display:""});
},hide:function(_18){
var _19=this._createWrapper(_18);
_9.set(_19,"display","none");
},getTopPopup:function(){
var _1a=this._stack;
for(var pi=_1a.length-1;pi>0&&_1a[pi].parent===_1a[pi-1].widget;pi--){
}
return _1a[pi];
},open:function(_1b){
var _1c=this._stack,_1d=_1b.popup,_1e=_1b.orient||["below","below-alt","above","above-alt"],ltr=_1b.parent?_1b.parent.isLeftToRight():_8.isBodyLtr(),_1f=_1b.around,id=(_1b.around&&_1b.around.id)?(_1b.around.id+"_dropdown"):("popup_"+this._idGen++);
while(_1c.length&&(!_1b.parent||!_5.isDescendant(_1b.parent.domNode,_1c[_1c.length-1].widget.domNode))){
this.close(_1c[_1c.length-1].widget);
}
var _20=this._createWrapper(_1d);
_6.set(_20,{id:id,style:{zIndex:this._beginZIndex+_1c.length},"class":"dijitPopup "+(_1d.baseClass||_1d["class"]||"").split(" ")[0]+"Popup",dijitPopupParent:_1b.parent?_1b.parent.id:""});
if(_d("ie")||_d("mozilla")){
if(!_1d.bgIframe){
_1d.bgIframe=new _10(_20);
}
}
var _21=_1f?_f.around(_20,_1f,_1e,ltr,_1d.orient?_c.hitch(_1d,"orient"):null):_f.at(_20,_1b,_1e=="R"?["TR","BR","TL","BL"]:["TL","BL","TR","BR"],_1b.padding);
_20.style.display="";
_20.style.visibility="visible";
_1d.domNode.style.visibility="visible";
var _22=[];
_22.push(on(_20,_3._keypress,_c.hitch(this,function(evt){
if(evt.charOrCode==_b.ESCAPE&&_1b.onCancel){
_a.stop(evt);
_1b.onCancel();
}else{
if(evt.charOrCode===_b.TAB){
_a.stop(evt);
var _23=this.getTopPopup();
if(_23&&_23.onCancel){
_23.onCancel();
}
}
}
})));
if(_1d.onCancel&&_1b.onCancel){
_22.push(_1d.on("cancel",_1b.onCancel));
}
_22.push(_1d.on(_1d.onExecute?"execute":"change",_c.hitch(this,function(){
var _24=this.getTopPopup();
if(_24&&_24.onExecute){
_24.onExecute();
}
})));
_1c.push({widget:_1d,parent:_1b.parent,onExecute:_1b.onExecute,onCancel:_1b.onCancel,onClose:_1b.onClose,handlers:_22});
if(_1d.onOpen){
_1d.onOpen(_21);
}
return _21;
},close:function(_25){
var _26=this._stack;
while((_25&&_1.some(_26,function(_27){
return _27.widget==_25;
}))||(!_25&&_26.length)){
var top=_26.pop(),_28=top.widget,_29=top.onClose;
if(_28.onClose){
_28.onClose();
}
var h;
while(h=top.handlers.pop()){
h.remove();
}
if(_28&&_28.domNode){
this.hide(_28);
}
if(_29){
_29();
}
}
}});
return (_11.popup=new _12());
});
