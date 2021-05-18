//>>built
require({cache:{"url:dijit/templates/Dialog.html":"<div class=\"dijitDialog\" role=\"dialog\" aria-labelledby=\"${id}_title\">\n\t<div data-dojo-attach-point=\"titleBar\" class=\"dijitDialogTitleBar\">\n\t\t<span data-dojo-attach-point=\"titleNode\" class=\"dijitDialogTitle\" id=\"${id}_title\"\n\t\t\t\trole=\"heading\" level=\"1\"></span>\n\t\t<span data-dojo-attach-point=\"closeButtonNode\" class=\"dijitDialogCloseIcon\" data-dojo-attach-event=\"ondijitclick: onCancel\" title=\"${buttonCancel}\" role=\"button\" tabindex=\"-1\">\n\t\t\t<span data-dojo-attach-point=\"closeText\" class=\"closeText\" title=\"${buttonCancel}\">x</span>\n\t\t</span>\n\t</div>\n\t<div data-dojo-attach-point=\"containerNode\" class=\"dijitDialogPaneContent\"></div>\n\t${!actionBarTemplate}\n</div>\n\n"}});
define("dijit/Dialog",["require","dojo/_base/array","dojo/aspect","dojo/_base/declare","dojo/Deferred","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dojo/_base/fx","dojo/i18n","dojo/keys","dojo/_base/lang","dojo/on","dojo/ready","dojo/sniff","dojo/touch","dojo/window","dojo/dnd/Moveable","dojo/dnd/TimedMoveable","./focus","./_base/manager","./_Widget","./_TemplatedMixin","./_CssStateMixin","./form/_FormMixin","./_DialogMixin","./DialogUnderlay","./layout/ContentPane","./layout/utils","dojo/text!./templates/Dialog.html","./a11yclick","dojo/i18n!./nls/common"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,fx,_a,_b,_c,on,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b,_1c,_1d){
var _1e=new _5();
_1e.resolve(true);
function nop(){
};
var _1f=_4("dijit._DialogBase"+(_e("dojo-bidi")?"_NoBidi":""),[_16,_18,_19,_17],{templateString:_1d,baseClass:"dijitDialog",cssStateNodes:{closeButtonNode:"dijitDialogCloseIcon"},_setTitleAttr:{node:"titleNode",type:"innerHTML"},open:false,duration:_14.defaultDuration,refocus:true,autofocus:true,_firstFocusItem:null,_lastFocusItem:null,draggable:true,_setDraggableAttr:function(val){
this._set("draggable",val);
},maxRatio:0.9,closable:true,_setClosableAttr:function(val){
this.closeButtonNode.style.display=val?"":"none";
this._set("closable",val);
},postMixInProperties:function(){
var _20=_a.getLocalization("dijit","common");
_c.mixin(this,_20);
this.inherited(arguments);
},postCreate:function(){
_9.set(this.domNode,{display:"none",position:"absolute"});
this.ownerDocumentBody.appendChild(this.domNode);
this.inherited(arguments);
_3.after(this,"onExecute",_c.hitch(this,"hide"),true);
_3.after(this,"onCancel",_c.hitch(this,"hide"),true);
on(this.closeButtonNode,_f.press,function(e){
e.stopPropagation();
});
this._modalconnects=[];
},onLoad:function(){
this.resize();
this._position();
if(this.autofocus&&_21.isTop(this)){
this._getFocusItems();
_13.focus(this._firstFocusItem);
}
this.inherited(arguments);
},focus:function(){
this._getFocusItems();
_13.focus(this._firstFocusItem);
},_endDrag:function(){
var _22=_8.position(this.domNode),_23=_10.getBox(this.ownerDocument);
_22.y=Math.min(Math.max(_22.y,0),(_23.h-_22.h));
_22.x=Math.min(Math.max(_22.x,0),(_23.w-_22.w));
this._relativePosition=_22;
this._position();
},_setup:function(){
var _24=this.domNode;
if(this.titleBar&&this.draggable){
this._moveable=new ((_e("ie")==6)?_12:_11)(_24,{handle:this.titleBar});
_3.after(this._moveable,"onMoveStop",_c.hitch(this,"_endDrag"),true);
}else{
_7.add(_24,"dijitDialogFixed");
}
this.underlayAttrs={dialogId:this.id,"class":_2.map(this["class"].split(/\s/),function(s){
return s+"_underlay";
}).join(" "),_onKeyDown:_c.hitch(this,"_onKey"),ownerDocument:this.ownerDocument};
},_size:function(){
this.resize();
},_position:function(){
if(!_7.contains(this.ownerDocumentBody,"dojoMove")){
var _25=this.domNode,_26=_10.getBox(this.ownerDocument),p=this._relativePosition,bb=_8.position(_25),l=Math.floor(_26.l+(p?Math.min(p.x,_26.w-bb.w):(_26.w-bb.w)/2)),t=Math.floor(_26.t+(p?Math.min(p.y,_26.h-bb.h):(_26.h-bb.h)/2));
_9.set(_25,{left:l+"px",top:t+"px"});
}
},_onKey:function(evt){
if(evt.keyCode==_b.TAB){
this._getFocusItems();
var _27=evt.target;
if(this._firstFocusItem==this._lastFocusItem){
evt.stopPropagation();
evt.preventDefault();
}else{
if(_27==this._firstFocusItem&&evt.shiftKey){
_13.focus(this._lastFocusItem);
evt.stopPropagation();
evt.preventDefault();
}else{
if(_27==this._lastFocusItem&&!evt.shiftKey){
_13.focus(this._firstFocusItem);
evt.stopPropagation();
evt.preventDefault();
}
}
}
}else{
if(this.closable&&evt.keyCode==_b.ESCAPE){
this.onCancel();
evt.stopPropagation();
evt.preventDefault();
}
}
},show:function(){
if(this.open){
return _1e.promise;
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
_21.hide(this);
}
var win=_10.get(this.ownerDocument);
this._modalconnects.push(on(win,"scroll",_c.hitch(this,"resize",null)));
this._modalconnects.push(on(this.domNode,"keydown",_c.hitch(this,"_onKey")));
_9.set(this.domNode,{opacity:0,display:""});
this._set("open",true);
this._onShow();
this.resize();
this._position();
var _28;
this._fadeInDeferred=new _5(_c.hitch(this,function(){
_28.stop();
delete this._fadeInDeferred;
}));
this._fadeInDeferred.then(undefined,nop);
var _29=this._fadeInDeferred.promise;
_28=fx.fadeIn({node:this.domNode,duration:this.duration,beforeBegin:_c.hitch(this,function(){
_21.show(this,this.underlayAttrs);
}),onEnd:_c.hitch(this,function(){
if(this.autofocus&&_21.isTop(this)){
this._getFocusItems();
_13.focus(this._firstFocusItem);
}
this._fadeInDeferred.resolve(true);
delete this._fadeInDeferred;
})}).play();
return _29;
},hide:function(){
if(!this._alreadyInitialized||!this.open){
return _1e.promise;
}
if(this._fadeInDeferred){
this._fadeInDeferred.cancel();
}
var _2a;
this._fadeOutDeferred=new _5(_c.hitch(this,function(){
_2a.stop();
delete this._fadeOutDeferred;
}));
this._fadeOutDeferred.then(undefined,nop);
this._fadeOutDeferred.then(_c.hitch(this,"onHide"));
var _2b=this._fadeOutDeferred.promise;
_2a=fx.fadeOut({node:this.domNode,duration:this.duration,onEnd:_c.hitch(this,function(){
this.domNode.style.display="none";
_21.hide(this);
this._fadeOutDeferred.resolve(true);
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
return _2b;
},resize:function(dim){
if(this.domNode.style.display!="none"){
this._checkIfSingleChild();
if(!dim){
if(this._shrunk){
if(this._singleChild){
if(typeof this._singleChildOriginalStyle!="undefined"){
this._singleChild.domNode.style.cssText=this._singleChildOriginalStyle;
delete this._singleChildOriginalStyle;
}
}
_2.forEach([this.domNode,this.containerNode,this.titleBar,this.actionBarNode],function(_2c){
if(_2c){
_9.set(_2c,{position:"static",width:"auto",height:"auto"});
}
});
this.domNode.style.position="absolute";
}
var _2d=_10.getBox(this.ownerDocument);
_2d.w*=this.maxRatio;
_2d.h*=this.maxRatio;
var bb=_8.position(this.domNode);
this._shrunk=false;
if(bb.w>=_2d.w){
dim={w:_2d.w};
_8.setMarginBox(this.domNode,dim);
bb=_8.position(this.domNode);
this._shrunk=true;
}
if(bb.h>=_2d.h){
if(!dim){
dim={w:bb.w};
}
dim.h=_2d.h;
this._shrunk=true;
}
if(dim){
if(!dim.w){
dim.w=bb.w;
}
if(!dim.h){
dim.h=bb.h;
}
}
}
if(dim){
_8.setMarginBox(this.domNode,dim);
var _2e=[];
if(this.titleBar){
_2e.push({domNode:this.titleBar,region:"top"});
}
if(this.actionBarNode){
_2e.push({domNode:this.actionBarNode,region:"bottom"});
}
var _2f={domNode:this.containerNode,region:"center"};
_2e.push(_2f);
var _30=_1c.marginBox2contentBox(this.domNode,dim);
_1c.layoutChildren(this.domNode,_30,_2e);
if(this._singleChild){
var cb=_1c.marginBox2contentBox(this.containerNode,_2f);
this._singleChild.resize({w:cb.w,h:cb.h});
}else{
this.containerNode.style.overflow="auto";
this._layoutChildren();
}
}else{
this._layoutChildren();
}
if(!_e("touch")&&!dim){
this._position();
}
}
},_layoutChildren:function(){
_2.forEach(this.getChildren(),function(_31){
if(_31.resize){
_31.resize();
}
});
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
_21.hide(this);
this.inherited(arguments);
}});
if(_e("dojo-bidi")){
_1f=_4("dijit._DialogBase",_1f,{_setTitleAttr:function(_32){
this._set("title",_32);
this.titleNode.innerHTML=_32;
this.applyTextDir(this.titleNode);
},_setTextDirAttr:function(_33){
if(this._created&&this.textDir!=_33){
this._set("textDir",_33);
this.set("title",this.title);
}
}});
}
var _34=_4("dijit.Dialog",[_1b,_1f],{});
_34._DialogBase=_1f;
var _21=_34._DialogLevelManager={_beginZIndex:950,show:function(_35,_36){
ds[ds.length-1].focus=_13.curNode;
var _37=ds[ds.length-1].dialog?ds[ds.length-1].zIndex+2:_34._DialogLevelManager._beginZIndex;
_9.set(_35.domNode,"zIndex",_37);
_1a.show(_36,_37-1);
ds.push({dialog:_35,underlayAttrs:_36,zIndex:_37});
},hide:function(_38){
if(ds[ds.length-1].dialog==_38){
ds.pop();
var pd=ds[ds.length-1];
if(ds.length==1){
_1a.hide();
}else{
_1a.show(pd.underlayAttrs,pd.zIndex-1);
}
if(_38.refocus){
var _39=pd.focus;
if(pd.dialog&&(!_39||!_6.isDescendant(_39,pd.dialog.domNode))){
pd.dialog._getFocusItems();
_39=pd.dialog._firstFocusItem;
}
if(_39){
try{
_39.focus();
}
catch(e){
}
}
}
}else{
var idx=_2.indexOf(_2.map(ds,function(_3a){
return _3a.dialog;
}),_38);
if(idx!=-1){
ds.splice(idx,1);
}
}
},isTop:function(_3b){
return ds[ds.length-1].dialog==_3b;
}};
var ds=_34._dialogStack=[{dialog:null,focus:null,underlayAttrs:null}];
_13.watch("curNode",function(_3c,_3d,_3e){
var _3f=ds[ds.length-1].dialog;
if(_3e&&_3f&&!_3f._fadeOutDeferred&&_3e.ownerDocument==_3f.ownerDocument){
do{
if(_3e==_3f.domNode||_7.contains(_3e,"dijitPopup")){
return;
}
}while(_3e=_3e.parentNode);
_3f.focus();
}
});
if(_e("dijit-legacy-requires")){
_d(0,function(){
var _40=["dijit/TooltipDialog"];
_1(_40);
});
}
return _34;
});
