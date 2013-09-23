//>>built
require({cache:{"url:dijit/templates/Dialog.html":"<div class=\"dijitDialog\" role=\"dialog\" aria-labelledby=\"${id}_title\">\n\t<div data-dojo-attach-point=\"titleBar\" class=\"dijitDialogTitleBar\">\n\t<span data-dojo-attach-point=\"titleNode\" class=\"dijitDialogTitle\" id=\"${id}_title\"></span>\n\t<span data-dojo-attach-point=\"closeButtonNode\" class=\"dijitDialogCloseIcon\" data-dojo-attach-event=\"ondijitclick: onCancel\" title=\"${buttonCancel}\" role=\"button\" tabIndex=\"-1\">\n\t\t<span data-dojo-attach-point=\"closeText\" class=\"closeText\" title=\"${buttonCancel}\">x</span>\n\t</span>\n\t</div>\n\t\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitDialogPaneContent\"></div>\n</div>\n"}});
define("dijit/Dialog",["require","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/Deferred","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/event","dojo/_base/fx","dojo/i18n","dojo/_base/kernel","dojo/keys","dojo/_base/lang","dojo/on","dojo/ready","dojo/_base/sniff","dojo/_base/window","dojo/window","dojo/dnd/Moveable","dojo/dnd/TimedMoveable","./focus","./_base/manager","./_Widget","./_TemplatedMixin","./_CssStateMixin","./form/_FormMixin","./_DialogMixin","./DialogUnderlay","./layout/ContentPane","dojo/text!./templates/Dialog.html",".","dojo/i18n!./nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,fx,_b,_c,_d,_e,on,_f,has,win,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d){
var _1e=_4("dijit._DialogBase",[_16,_18,_19,_17],{templateString:_1c,baseClass:"dijitDialog",cssStateNodes:{closeButtonNode:"dijitDialogCloseIcon"},_setTitleAttr:[{node:"titleNode",type:"innerHTML"},{node:"titleBar",type:"attribute"}],open:false,duration:_14.defaultDuration,refocus:true,autofocus:true,_firstFocusItem:null,_lastFocusItem:null,doLayout:false,draggable:true,"aria-describedby":"",postMixInProperties:function(){
var _1f=_b.getLocalization("dijit","common");
_e.mixin(this,_1f);
this.inherited(arguments);
},postCreate:function(){
_9.set(this.domNode,{display:"none",position:"absolute"});
win.body().appendChild(this.domNode);
this.inherited(arguments);
this.connect(this,"onExecute","hide");
this.connect(this,"onCancel","hide");
this._modalconnects=[];
},onLoad:function(){
this._position();
if(this.autofocus&&_20.isTop(this)){
this._getFocusItems(this.domNode);
_13.focus(this._firstFocusItem);
}
this.inherited(arguments);
},_endDrag:function(){
var _21=_8.position(this.domNode),_22=_10.getBox();
_21.y=Math.min(Math.max(_21.y,0),(_22.h-_21.h));
_21.x=Math.min(Math.max(_21.x,0),(_22.w-_21.w));
this._relativePosition=_21;
this._position();
},_setup:function(){
var _23=this.domNode;
if(this.titleBar&&this.draggable){
this._moveable=new ((has("ie")==6)?_12:_11)(_23,{handle:this.titleBar});
this.connect(this._moveable,"onMoveStop","_endDrag");
}else{
_7.add(_23,"dijitDialogFixed");
}
this.underlayAttrs={dialogId:this.id,"class":_2.map(this["class"].split(/\s/),function(s){
return s+"_underlay";
}).join(" ")};
},_size:function(){
this._checkIfSingleChild();
if(this._singleChild){
if(this._singleChildOriginalStyle){
this._singleChild.domNode.style.cssText=this._singleChildOriginalStyle;
}
delete this._singleChildOriginalStyle;
}else{
_9.set(this.containerNode,{width:"auto",height:"auto"});
}
var bb=_8.position(this.domNode);
var _24=_10.getBox();
if(bb.w>=_24.w||bb.h>=_24.h){
var w=Math.min(bb.w,Math.floor(_24.w*0.75)),h=Math.min(bb.h,Math.floor(_24.h*0.75));
if(this._singleChild&&this._singleChild.resize){
this._singleChildOriginalStyle=this._singleChild.domNode.style.cssText;
this._singleChild.resize({w:w,h:h});
}else{
_9.set(this.containerNode,{width:w+"px",height:h+"px",overflow:"auto",position:"relative"});
}
}else{
if(this._singleChild&&this._singleChild.resize){
this._singleChild.resize();
}
}
},_position:function(){
if(!_7.contains(win.body(),"dojoMove")){
var _25=this.domNode,_26=_10.getBox(),p=this._relativePosition,bb=p?null:_8.position(_25),l=Math.floor(_26.l+(p?p.x:(_26.w-bb.w)/2)),t=Math.floor(_26.t+(p?p.y:(_26.h-bb.h)/2));
_9.set(_25,{left:l+"px",top:t+"px"});
}
},_onKey:function(evt){
if(evt.charOrCode){
var _27=evt.target;
if(evt.charOrCode===_d.TAB){
this._getFocusItems(this.domNode);
}
var _28=(this._firstFocusItem==this._lastFocusItem);
if(_27==this._firstFocusItem&&evt.shiftKey&&evt.charOrCode===_d.TAB){
if(!_28){
_13.focus(this._lastFocusItem);
}
_a.stop(evt);
}else{
if(_27==this._lastFocusItem&&evt.charOrCode===_d.TAB&&!evt.shiftKey){
if(!_28){
_13.focus(this._firstFocusItem);
}
_a.stop(evt);
}else{
while(_27){
if(_27==this.domNode||_7.contains(_27,"dijitPopup")){
if(evt.charOrCode==_d.ESCAPE){
this.onCancel();
}else{
return;
}
}
_27=_27.parentNode;
}
if(evt.charOrCode!==_d.TAB){
_a.stop(evt);
}else{
if(!has("opera")){
try{
this._firstFocusItem.focus();
}
catch(e){
}
}
}
}
}
}
},show:function(){
if(this.open){
return;
}
if(!this._started){
this.startup();
}
if(!this._alreadyInitialized){
this._setup();
this._alreadyInitialized=true;
}
if(this._fadeOutDeferred){
this._fadeOutDeferred.cancel();
}
this._modalconnects.push(on(window,"scroll",_e.hitch(this,"layout")));
this._modalconnects.push(on(window,"resize",_e.hitch(this,function(){
var _29=_10.getBox();
if(!this._oldViewport||_29.h!=this._oldViewport.h||_29.w!=this._oldViewport.w){
this.layout();
this._oldViewport=_29;
}
})));
this._modalconnects.push(on(this.domNode,_3._keypress,_e.hitch(this,"_onKey")));
_9.set(this.domNode,{opacity:0,display:""});
this._set("open",true);
this._onShow();
this._size();
this._position();
var _2a;
this._fadeInDeferred=new _5(_e.hitch(this,function(){
_2a.stop();
delete this._fadeInDeferred;
}));
_2a=fx.fadeIn({node:this.domNode,duration:this.duration,beforeBegin:_e.hitch(this,function(){
_20.show(this,this.underlayAttrs);
}),onEnd:_e.hitch(this,function(){
if(this.autofocus&&_20.isTop(this)){
this._getFocusItems(this.domNode);
_13.focus(this._firstFocusItem);
}
this._fadeInDeferred.callback(true);
delete this._fadeInDeferred;
})}).play();
return this._fadeInDeferred;
},hide:function(){
if(!this._alreadyInitialized){
return;
}
if(this._fadeInDeferred){
this._fadeInDeferred.cancel();
}
var _2b;
this._fadeOutDeferred=new _5(_e.hitch(this,function(){
_2b.stop();
delete this._fadeOutDeferred;
}));
this._fadeOutDeferred.then(_e.hitch(this,"onHide"));
_2b=fx.fadeOut({node:this.domNode,duration:this.duration,onEnd:_e.hitch(this,function(){
this.domNode.style.display="none";
_20.hide(this);
this._fadeOutDeferred.callback(true);
delete this._fadeOutDeferred;
})}).play();
if(this._scrollConnected){
this._scrollConnected=false;
}
var h;
while(h=this._modalconnects.pop()){
h.remove();
}
if(this._relativePosition){
delete this._relativePosition;
}
this._set("open",false);
return this._fadeOutDeferred;
},layout:function(){
if(this.domNode.style.display!="none"){
if(_1d._underlay){
_1d._underlay.layout();
}
this._position();
}
},destroy:function(){
if(this._fadeInDeferred){
this._fadeInDeferred.cancel();
}
if(this._fadeOutDeferred){
this._fadeOutDeferred.cancel();
}
if(this._moveable){
this._moveable.destroy();
}
var h;
while(h=this._modalconnects.pop()){
h.remove();
}
_20.hide(this);
this.inherited(arguments);
}});
var _2c=_4("dijit.Dialog",[_1b,_1e],{});
_2c._DialogBase=_1e;
var _20=_2c._DialogLevelManager={_beginZIndex:950,show:function(_2d,_2e){
ds[ds.length-1].focus=_13.curNode;
var _2f=_1d._underlay;
if(!_2f||_2f._destroyed){
_2f=_1d._underlay=new _1a(_2e);
}else{
_2f.set(_2d.underlayAttrs);
}
var _30=ds[ds.length-1].dialog?ds[ds.length-1].zIndex+2:_2c._DialogLevelManager._beginZIndex;
if(ds.length==1){
_2f.show();
}
_9.set(_1d._underlay.domNode,"zIndex",_30-1);
_9.set(_2d.domNode,"zIndex",_30);
ds.push({dialog:_2d,underlayAttrs:_2e,zIndex:_30});
},hide:function(_31){
if(ds[ds.length-1].dialog==_31){
ds.pop();
var pd=ds[ds.length-1];
if(ds.length==1){
if(!_1d._underlay._destroyed){
_1d._underlay.hide();
}
}else{
_9.set(_1d._underlay.domNode,"zIndex",pd.zIndex-1);
_1d._underlay.set(pd.underlayAttrs);
}
if(_31.refocus){
var _32=pd.focus;
if(pd.dialog&&(!_32||!_6.isDescendant(_32,pd.dialog.domNode))){
pd.dialog._getFocusItems(pd.dialog.domNode);
_32=pd.dialog._firstFocusItem;
}
if(_32){
_32.focus();
}
}
}else{
var idx=_2.indexOf(_2.map(ds,function(_33){
return _33.dialog;
}),_31);
if(idx!=-1){
ds.splice(idx,1);
}
}
},isTop:function(_34){
return ds[ds.length-1].dialog==_34;
}};
var ds=_2c._dialogStack=[{dialog:null,focus:null,underlayAttrs:null}];
if(!_c.isAsync){
_f(0,function(){
var _35=["dijit/TooltipDialog"];
_1(_35);
});
}
return _2c;
});
