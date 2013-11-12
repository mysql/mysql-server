//>>built
require({cache:{"url:dojox/image/resources/Lightbox.html":"<div class=\"dojoxLightbox\" dojoAttachPoint=\"containerNode\">\n\t<div style=\"position:relative\">\n\t\t<div dojoAttachPoint=\"imageContainer\" class=\"dojoxLightboxContainer\" dojoAttachEvent=\"onclick: _onImageClick\">\n\t\t\t<img dojoAttachPoint=\"imgNode\" src=\"${imgUrl}\" class=\"dojoxLightboxImage\" alt=\"${title}\">\n\t\t\t<div class=\"dojoxLightboxFooter\" dojoAttachPoint=\"titleNode\">\n\t\t\t\t<div class=\"dijitInline LightboxClose\" dojoAttachPoint=\"closeButtonNode\"></div>\n\t\t\t\t<div class=\"dijitInline LightboxNext\" dojoAttachPoint=\"nextButtonNode\"></div>\t\n\t\t\t\t<div class=\"dijitInline LightboxPrev\" dojoAttachPoint=\"prevButtonNode\"></div>\n\t\t\t\t<div class=\"dojoxLightboxText\" dojoAttachPoint=\"titleTextNode\"><span dojoAttachPoint=\"textNode\">${title}</span><span dojoAttachPoint=\"groupCount\" class=\"dojoxLightboxGroupText\"></span></div>\n\t\t\t</div>\n\t\t</div>\n\t</div>\n</div>"}});
define("dojox/image/Lightbox",["dojo","dijit","dojox","dojo/text!./resources/Lightbox.html","dijit/Dialog","dojox/fx/_base"],function(_1,_2,_3,_4){
_1.experimental("dojox.image.Lightbox");
_1.getObject("image",true,_3);
_1.declare("dojox.image.Lightbox",_2._Widget,{group:"",title:"",href:"",duration:500,modal:false,_allowPassthru:false,_attachedDialog:null,startup:function(){
this.inherited(arguments);
var _5=_2.byId("dojoxLightboxDialog");
if(_5){
this._attachedDialog=_5;
}else{
this._attachedDialog=new _3.image.LightboxDialog({id:"dojoxLightboxDialog"});
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
_1.declare("dojox.image.LightboxDialog",_2.Dialog,{title:"",inGroup:null,imgUrl:_2._Widget.prototype._blankGif,errorMessage:"Image not found.",adjust:true,modal:false,errorImg:_1.moduleUrl("dojox.image","resources/images/warning.png"),templateString:_4,constructor:function(_6){
this._groups=this._groups||(_6&&_6._groups)||{XnoGroupX:[]};
},startup:function(){
this.inherited(arguments);
this._animConnects=[];
this.connect(this.nextButtonNode,"onclick","_nextImage");
this.connect(this.prevButtonNode,"onclick","_prevImage");
this.connect(this.closeButtonNode,"onclick","hide");
this._makeAnims();
this._vp=_1.window.getBox();
return this;
},show:function(_7){
var _8=this;
this._lastGroup=_7;
if(!_8.open){
_8.inherited(arguments);
_8._modalconnects.push(_1.connect(_1.global,"onscroll",this,"_position"),_1.connect(_1.global,"onresize",this,"_position"),_1.connect(_1.body(),"onkeypress",this,"_handleKey"));
if(!_7.modal){
_8._modalconnects.push(_1.connect(_2._underlay.domNode,"onclick",this,"onCancel"));
}
}
if(this._wasStyled){
var _9=_1.create("img",null,_8.imgNode,"after");
_1.destroy(_8.imgNode);
_8.imgNode=_9;
_8._makeAnims();
_8._wasStyled=false;
}
_1.style(_8.imgNode,"opacity","0");
_1.style(_8.titleNode,"opacity","0");
var _a=_7.href;
if((_7.group&&_7!=="XnoGroupX")||_8.inGroup){
if(!_8.inGroup){
_8.inGroup=_8._groups[(_7.group)];
_1.forEach(_8.inGroup,function(g,i){
if(g.href==_7.href){
_8._index=i;
}
});
}
if(!_8._index){
_8._index=0;
var sr=_8.inGroup[_8._index];
_a=(sr&&sr.href)||_8.errorImg;
}
_8.groupCount.innerHTML=" ("+(_8._index+1)+" of "+Math.max(1,_8.inGroup.length)+")";
_8.prevButtonNode.style.visibility="visible";
_8.nextButtonNode.style.visibility="visible";
}else{
_8.groupCount.innerHTML="";
_8.prevButtonNode.style.visibility="hidden";
_8.nextButtonNode.style.visibility="hidden";
}
if(!_7.leaveTitle){
_8.textNode.innerHTML=_7.title;
}
_8._ready(_a);
},_ready:function(_b){
var _c=this;
_c._imgError=_1.connect(_c.imgNode,"error",_c,function(){
_1.disconnect(_c._imgError);
_c.imgNode.src=_c.errorImg;
_c.textNode.innerHTML=_c.errorMessage;
});
_c._imgConnect=_1.connect(_c.imgNode,"load",_c,function(e){
_c.resizeTo({w:_c.imgNode.width,h:_c.imgNode.height,duration:_c.duration});
_1.disconnect(_c._imgConnect);
if(_c._imgError){
_1.disconnect(_c._imgError);
}
});
_c.imgNode.src=_b;
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
var _d=_1.map(_1.query("> *",this.titleNode).position(),function(s){
return s.h;
});
return {h:Math.max.apply(Math,_d)};
},resizeTo:function(_e,_f){
var _10=_1.boxModel=="border-box"?_1._getBorderExtents(this.domNode).w:0,_11=_f||this._calcTitleSize();
this._lastTitleSize=_11;
if(this.adjust&&(_e.h+_11.h+_10+80>this._vp.h||_e.w+_10+60>this._vp.w)){
this._lastSize=_e;
_e=this._scaleToFit(_e);
}
this._currentSize=_e;
var _12=_3.fx.sizeTo({node:this.containerNode,duration:_e.duration||this.duration,width:_e.w+_10,height:_e.h+_11.h+_10});
this.connect(_12,"onEnd","_showImage");
_12.play(15);
},_scaleToFit:function(_13){
var ns={},nvp={w:this._vp.w-80,h:this._vp.h-60-this._lastTitleSize.h};
var _14=nvp.w/nvp.h,_15=_13.w/_13.h;
if(_15>=_14){
ns.h=nvp.w/_15;
ns.w=nvp.w;
}else{
ns.w=_15*nvp.h;
ns.h=nvp.h;
}
this._wasStyled=true;
this._setImageSize(ns);
ns.duration=_13.duration;
return ns;
},_setImageSize:function(_16){
var s=this.imgNode;
s.height=_16.h;
s.width=_16.w;
},_size:function(){
},_position:function(e){
this._vp=_1.window.getBox();
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
var _17=_1.marginBox(this.titleNode);
if(_17.h>this._lastTitleSize.h){
this.resizeTo(this._wasStyled?this._lastSize:this._currentSize,_17);
}else{
this._showNavAnim.play(1);
}
},hide:function(){
_1.fadeOut({node:this.titleNode,duration:200,onEnd:_1.hitch(this,function(){
this.imgNode.src=this._blankGif;
})}).play(5);
this.inherited(arguments);
this.inGroup=null;
this._index=null;
},addImage:function(_18,_19){
var g=_19;
if(!_18.href){
return;
}
if(g){
if(!this._groups[g]){
this._groups[g]=[];
}
this._groups[g].push(_18);
}else{
this._groups["XnoGroupX"].push(_18);
}
},removeImage:function(_1a){
var g=_1a.group||"XnoGroupX";
_1.every(this._groups[g],function(_1b,i,ar){
if(_1b.href==_1a.href){
ar.splice(i,1);
return false;
}
return true;
});
},removeGroup:function(_1c){
if(this._groups[_1c]){
this._groups[_1c]=[];
}
},_handleKey:function(e){
if(!this.open){
return;
}
var dk=_1.keys;
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
_1.forEach(this._animConnects,_1.disconnect);
this._animConnects=[];
this._showImageAnim=_1.fadeIn({node:this.imgNode,duration:this.duration});
this._animConnects.push(_1.connect(this._showImageAnim,"onEnd",this,"_showNav"));
this._loadingAnim=_1.fx.combine([_1.fadeOut({node:this.imgNode,duration:175}),_1.fadeOut({node:this.titleNode,duration:175})]);
this._animConnects.push(_1.connect(this._loadingAnim,"onEnd",this,"_prepNodes"));
this._showNavAnim=_1.fadeIn({node:this.titleNode,duration:225});
},onClick:function(_1d){
},_onImageClick:function(e){
if(e&&e.target==this.imgNode){
this.onClick(this._lastGroup);
if(this._lastGroup.declaredClass){
this._lastGroup.onClick(this._lastGroup);
}
}
}});
return _3.image.Lightbox;
});
