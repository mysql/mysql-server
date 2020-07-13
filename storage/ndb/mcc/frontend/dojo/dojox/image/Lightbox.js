//>>built
require({cache:{"url:dojox/image/resources/Lightbox.html":"<div class=\"dojoxLightbox\" dojoAttachPoint=\"containerNode\">\n\t<div style=\"position:relative\">\n\t\t<div dojoAttachPoint=\"imageContainer\" class=\"dojoxLightboxContainer\" dojoAttachEvent=\"onclick: _onImageClick\">\n\t\t\t<img dojoAttachPoint=\"imgNode\" src=\"${imgUrl}\" class=\"${imageClass}\" alt=\"${title}\">\n\t\t\t<div class=\"dojoxLightboxFooter\" dojoAttachPoint=\"titleNode\">\n\t\t\t\t<div class=\"dijitInline LightboxClose\" dojoAttachPoint=\"closeButtonNode\"></div>\n\t\t\t\t<div class=\"dijitInline LightboxNext\" dojoAttachPoint=\"nextButtonNode\"></div>\t\n\t\t\t\t<div class=\"dijitInline LightboxPrev\" dojoAttachPoint=\"prevButtonNode\"></div>\n\t\t\t\t<div class=\"dojoxLightboxText\" dojoAttachPoint=\"titleTextNode\"><span dojoAttachPoint=\"textNode\">${title}</span><span dojoAttachPoint=\"groupCount\" class=\"dojoxLightboxGroupText\"></span></div>\n\t\t\t</div>\n\t\t</div>\n\t</div>\n</div>"}});
define("dojox/image/Lightbox",["require","dojo","dijit","dojox","dojo/text!./resources/Lightbox.html","dijit/Dialog","dojox/fx/_base"],function(_1,_2,_3,_4,_5){
_2.experimental("dojox.image.Lightbox");
_2.getObject("image",true,_4);
var _6=_2.declare("dojox.image.Lightbox",_3._Widget,{group:"",title:"",href:"",duration:500,modal:false,_allowPassthru:false,_attachedDialog:null,startup:function(){
this.inherited(arguments);
var _7=_3.byId("dojoxLightboxDialog");
if(_7){
this._attachedDialog=_7;
}else{
this._attachedDialog=new _4.image.LightboxDialog({id:"dojoxLightboxDialog"});
this._attachedDialog.startup();
}
if(!this.store){
this._addSelf();
this.connect(this.domNode,"onclick","_handleClick");
}
},_addSelf:function(){
this._attachedDialog.addImage({href:this.href,title:this.title},this.group||null);
},_handleClick:function(e){
if(!this._allowPassthru){
e.preventDefault();
}else{
return;
}
this.show();
},show:function(){
this._attachedDialog.show(this);
},hide:function(){
this._attachedDialog.hide();
},disable:function(){
this._allowPassthru=true;
},enable:function(){
this._allowPassthru=false;
},onClick:function(){
},destroy:function(){
this._attachedDialog.removeImage(this);
this.inherited(arguments);
}});
_6.LightboxDialog=_2.declare("dojox.image.LightboxDialog",_3.Dialog,{title:"",inGroup:null,imgUrl:_3._Widget.prototype._blankGif,errorMessage:"Image not found.",adjust:true,modal:false,imageClass:"dojoxLightboxImage",errorImg:_1.toUrl("./resources/images/warning.png"),templateString:_5,constructor:function(_8){
this._groups=this._groups||(_8&&_8._groups)||{XnoGroupX:[]};
},startup:function(){
this.inherited(arguments);
this._animConnects=[];
this.connect(this.nextButtonNode,"onclick","_nextImage");
this.connect(this.prevButtonNode,"onclick","_prevImage");
this.connect(this.closeButtonNode,"onclick","hide");
this._makeAnims();
this._vp=_2.window.getBox();
return this;
},show:function(_9){
var _a=this;
this._lastGroup=_9;
if(!_a.open){
_a.inherited(arguments);
_a._modalconnects.push(_2.connect(_2.global,"onscroll",this,"_position"),_2.connect(_2.global,"onresize",this,"_position"),_2.connect(_2.body(),"onkeypress",this,"_handleKey"));
if(!_9.modal){
_a._modalconnects.push(_2.connect(_3._underlay.domNode,"onclick",this,"onCancel"));
}
}
if(this._wasStyled){
var _b=_2.create("img",{className:_a.imageClass},_a.imgNode,"after");
_2.destroy(_a.imgNode);
_a.imgNode=_b;
_a._makeAnims();
_a._wasStyled=false;
}
_2.style(_a.imgNode,"opacity","0");
_2.style(_a.titleNode,"opacity","0");
var _c=_9.href;
if((_9.group&&_9!=="XnoGroupX")||_a.inGroup){
if(!_a.inGroup){
_a.inGroup=_a._groups[(_9.group)];
_2.forEach(_a.inGroup,function(g,i){
if(g.href==_9.href){
_a._index=i;
}
});
}
if(!_a._index){
_a._index=0;
var sr=_a.inGroup[_a._index];
_c=(sr&&sr.href)||_a.errorImg;
}
_a.groupCount.innerHTML=" ("+(_a._index+1)+" of "+Math.max(1,_a.inGroup.length)+")";
_a.prevButtonNode.style.visibility="visible";
_a.nextButtonNode.style.visibility="visible";
}else{
_a.groupCount.innerHTML="";
_a.prevButtonNode.style.visibility="hidden";
_a.nextButtonNode.style.visibility="hidden";
}
if(!_9.leaveTitle){
_a.textNode.innerHTML=_9.title;
}
_a._ready(_c);
},_ready:function(_d){
var _e=this;
_e._imgError=_2.connect(_e.imgNode,"error",_e,function(){
_2.disconnect(_e._imgError);
_e.imgNode.src=_e.errorImg;
_e.textNode.innerHTML=_e.errorMessage;
});
_e._imgConnect=_2.connect(_e.imgNode,"load",_e,function(e){
_e.resizeTo({w:_e.imgNode.width,h:_e.imgNode.height,duration:_e.duration});
_2.disconnect(_e._imgConnect);
if(_e._imgError){
_2.disconnect(_e._imgError);
}
});
_e.imgNode.src=_d;
},_nextImage:function(){
if(!this.inGroup){
return;
}
if(this._index+1<this.inGroup.length){
this._index++;
}else{
this._index=0;
}
this._loadImage();
},_prevImage:function(){
if(this.inGroup){
if(this._index==0){
this._index=this.inGroup.length-1;
}else{
this._index--;
}
this._loadImage();
}
},_loadImage:function(){
this._loadingAnim.play(1);
},_prepNodes:function(){
this._imageReady=false;
if(this.inGroup&&this.inGroup[this._index]){
this.show({href:this.inGroup[this._index].href,title:this.inGroup[this._index].title});
}else{
this.show({title:this.errorMessage,href:this.errorImg});
}
},_calcTitleSize:function(){
var _f=_2.map(_2.query("> *",this.titleNode).position(),function(s){
return s.h;
});
return {h:Math.max.apply(Math,_f)};
},resizeTo:function(_10,_11){
var _12=_2.boxModel=="border-box"?_2._getBorderExtents(this.domNode).w:0,_13=_11||this._calcTitleSize();
this._lastTitleSize=_13;
if(this.adjust&&(_10.h+_13.h+_12+80>this._vp.h||_10.w+_12+60>this._vp.w)){
this._lastSize=_10;
_10=this._scaleToFit(_10);
}
this._currentSize=_10;
var _14=_4.fx.sizeTo({node:this.containerNode,duration:_10.duration||this.duration,width:_10.w+_12,height:_10.h+_13.h+_12});
this.connect(_14,"onEnd","_showImage");
_14.play(15);
},_scaleToFit:function(_15){
var ns={},nvp={w:this._vp.w-80,h:this._vp.h-60-this._lastTitleSize.h};
var _16=nvp.w/nvp.h,_17=_15.w/_15.h;
if(_17>=_16){
ns.h=nvp.w/_17;
ns.w=nvp.w;
}else{
ns.w=_17*nvp.h;
ns.h=nvp.h;
}
this._wasStyled=true;
this._setImageSize(ns);
ns.duration=_15.duration;
return ns;
},_setImageSize:function(_18){
var s=this.imgNode;
s.height=_18.h;
s.width=_18.w;
},_size:function(){
},_position:function(e){
this._vp=_2.window.getBox();
this.inherited(arguments);
if(e&&e.type=="resize"){
if(this._wasStyled){
this._setImageSize(this._lastSize);
this.resizeTo(this._lastSize);
}else{
if(this.imgNode.height+80>this._vp.h||this.imgNode.width+60>this._vp.h){
this.resizeTo({w:this.imgNode.width,h:this.imgNode.height});
}
}
}
},_showImage:function(){
this._showImageAnim.play(1);
},_showNav:function(){
var _19=_2.marginBox(this.titleNode);
if(_19.h>this._lastTitleSize.h){
this.resizeTo(this._wasStyled?this._lastSize:this._currentSize,_19);
}else{
this._showNavAnim.play(1);
}
},hide:function(){
_2.fadeOut({node:this.titleNode,duration:200,onEnd:_2.hitch(this,function(){
this.imgNode.src=this._blankGif;
})}).play(5);
this.inherited(arguments);
this.inGroup=null;
this._index=null;
},addImage:function(_1a,_1b){
var g=_1b;
if(!_1a.href){
return;
}
if(g){
if(!this._groups[g]){
this._groups[g]=[];
}
this._groups[g].push(_1a);
}else{
this._groups["XnoGroupX"].push(_1a);
}
},removeImage:function(_1c){
var g=_1c.group||"XnoGroupX";
_2.every(this._groups[g],function(_1d,i,ar){
if(_1d.href==_1c.href){
ar.splice(i,1);
return false;
}
return true;
});
},removeGroup:function(_1e){
if(this._groups[_1e]){
this._groups[_1e]=[];
}
},_handleKey:function(e){
if(!this.open){
return;
}
var dk=_2.keys;
switch(e.charOrCode){
case dk.ESCAPE:
this.hide();
break;
case dk.DOWN_ARROW:
case dk.RIGHT_ARROW:
case 78:
this._nextImage();
break;
case dk.UP_ARROW:
case dk.LEFT_ARROW:
case 80:
this._prevImage();
break;
}
},_makeAnims:function(){
_2.forEach(this._animConnects,_2.disconnect);
this._animConnects=[];
this._showImageAnim=_2.fadeIn({node:this.imgNode,duration:this.duration});
this._animConnects.push(_2.connect(this._showImageAnim,"onEnd",this,"_showNav"));
this._loadingAnim=_2.fx.combine([_2.fadeOut({node:this.imgNode,duration:175}),_2.fadeOut({node:this.titleNode,duration:175})]);
this._animConnects.push(_2.connect(this._loadingAnim,"onEnd",this,"_prepNodes"));
this._showNavAnim=_2.fadeIn({node:this.titleNode,duration:225});
},onClick:function(_1f){
},_onImageClick:function(e){
if(e&&e.target==this.imgNode){
this.onClick(this._lastGroup);
if(this._lastGroup.declaredClass){
this._lastGroup.onClick(this._lastGroup);
}
}
}});
return _6;
});
