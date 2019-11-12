//>>built
require({cache:{"url:dojox/image/resources/Lightbox.html":"<div class=\"dojoxLightbox\" dojoAttachPoint=\"containerNode\">\n\t<div style=\"position:relative\">\n\t\t<div dojoAttachPoint=\"imageContainer\" class=\"dojoxLightboxContainer\" dojoAttachEvent=\"onclick: _onImageClick\">\n\t\t\t<img dojoAttachPoint=\"imgNode\" src=\"${imgUrl}\" class=\"${imageClass}\" alt=\"${title}\">\n\t\t\t<div class=\"dojoxLightboxFooter\" dojoAttachPoint=\"titleNode\">\n\t\t\t\t<div class=\"dijitInline LightboxClose\" dojoAttachPoint=\"closeButtonNode\"></div>\n\t\t\t\t<div class=\"dijitInline LightboxNext\" dojoAttachPoint=\"nextButtonNode\"></div>\t\n\t\t\t\t<div class=\"dijitInline LightboxPrev\" dojoAttachPoint=\"prevButtonNode\"></div>\n\t\t\t\t<div class=\"dojoxLightboxText\" dojoAttachPoint=\"titleTextNode\"><span dojoAttachPoint=\"textNode\">${title}</span><span dojoAttachPoint=\"groupCount\" class=\"dojoxLightboxGroupText\"></span></div>\n\t\t\t</div>\n\t\t</div>\n\t</div>\n</div>"}});
define("dojox/image/Lightbox",["require","dojo","dijit","dojox","dojo/text!./resources/Lightbox.html","dijit/Dialog","dojox/fx/_base"],function(_1,_2,_3,_4,_5){
_2.experimental("dojox.image.Lightbox");
_2.getObject("image",true,_4);
_2.declare("dojox.image.Lightbox",_3._Widget,{group:"",title:"",href:"",duration:500,modal:false,_allowPassthru:false,_attachedDialog:null,startup:function(){
this.inherited(arguments);
var _6=_3.byId("dojoxLightboxDialog");
if(_6){
this._attachedDialog=_6;
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
_2.declare("dojox.image.LightboxDialog",_3.Dialog,{title:"",inGroup:null,imgUrl:_3._Widget.prototype._blankGif,errorMessage:"Image not found.",adjust:true,modal:false,imageClass:"dojoxLightboxImage",errorImg:_1.toUrl("./resources/images/warning.png"),templateString:_5,constructor:function(_7){
this._groups=this._groups||(_7&&_7._groups)||{XnoGroupX:[]};
},startup:function(){
this.inherited(arguments);
this._animConnects=[];
this.connect(this.nextButtonNode,"onclick","_nextImage");
this.connect(this.prevButtonNode,"onclick","_prevImage");
this.connect(this.closeButtonNode,"onclick","hide");
this._makeAnims();
this._vp=_2.window.getBox();
return this;
},show:function(_8){
var _9=this;
this._lastGroup=_8;
if(!_9.open){
_9.inherited(arguments);
_9._modalconnects.push(_2.connect(_2.global,"onscroll",this,"_position"),_2.connect(_2.global,"onresize",this,"_position"),_2.connect(_2.body(),"onkeypress",this,"_handleKey"));
if(!_8.modal){
_9._modalconnects.push(_2.connect(_3._underlay.domNode,"onclick",this,"onCancel"));
}
}
if(this._wasStyled){
var _a=_2.create("img",{className:_9.imageClass},_9.imgNode,"after");
_2.destroy(_9.imgNode);
_9.imgNode=_a;
_9._makeAnims();
_9._wasStyled=false;
}
_2.style(_9.imgNode,"opacity","0");
_2.style(_9.titleNode,"opacity","0");
var _b=_8.href;
if((_8.group&&_8!=="XnoGroupX")||_9.inGroup){
if(!_9.inGroup){
_9.inGroup=_9._groups[(_8.group)];
_2.forEach(_9.inGroup,function(g,i){
if(g.href==_8.href){
_9._index=i;
}
});
}
if(!_9._index){
_9._index=0;
var sr=_9.inGroup[_9._index];
_b=(sr&&sr.href)||_9.errorImg;
}
_9.groupCount.innerHTML=" ("+(_9._index+1)+" of "+Math.max(1,_9.inGroup.length)+")";
_9.prevButtonNode.style.visibility="visible";
_9.nextButtonNode.style.visibility="visible";
}else{
_9.groupCount.innerHTML="";
_9.prevButtonNode.style.visibility="hidden";
_9.nextButtonNode.style.visibility="hidden";
}
if(!_8.leaveTitle){
_9.textNode.innerHTML=_8.title;
}
_9._ready(_b);
},_ready:function(_c){
var _d=this;
_d._imgError=_2.connect(_d.imgNode,"error",_d,function(){
_2.disconnect(_d._imgError);
_d.imgNode.src=_d.errorImg;
_d.textNode.innerHTML=_d.errorMessage;
});
_d._imgConnect=_2.connect(_d.imgNode,"load",_d,function(e){
_d.resizeTo({w:_d.imgNode.width,h:_d.imgNode.height,duration:_d.duration});
_2.disconnect(_d._imgConnect);
if(_d._imgError){
_2.disconnect(_d._imgError);
}
});
_d.imgNode.src=_c;
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
var _e=_2.map(_2.query("> *",this.titleNode).position(),function(s){
return s.h;
});
return {h:Math.max.apply(Math,_e)};
},resizeTo:function(_f,_10){
var _11=_2.boxModel=="border-box"?_2._getBorderExtents(this.domNode).w:0,_12=_10||this._calcTitleSize();
this._lastTitleSize=_12;
if(this.adjust&&(_f.h+_12.h+_11+80>this._vp.h||_f.w+_11+60>this._vp.w)){
this._lastSize=_f;
_f=this._scaleToFit(_f);
}
this._currentSize=_f;
var _13=_4.fx.sizeTo({node:this.containerNode,duration:_f.duration||this.duration,width:_f.w+_11,height:_f.h+_12.h+_11});
this.connect(_13,"onEnd","_showImage");
_13.play(15);
},_scaleToFit:function(_14){
var ns={},nvp={w:this._vp.w-80,h:this._vp.h-60-this._lastTitleSize.h};
var _15=nvp.w/nvp.h,_16=_14.w/_14.h;
if(_16>=_15){
ns.h=nvp.w/_16;
ns.w=nvp.w;
}else{
ns.w=_16*nvp.h;
ns.h=nvp.h;
}
this._wasStyled=true;
this._setImageSize(ns);
ns.duration=_14.duration;
return ns;
},_setImageSize:function(_17){
var s=this.imgNode;
s.height=_17.h;
s.width=_17.w;
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
var _18=_2.marginBox(this.titleNode);
if(_18.h>this._lastTitleSize.h){
this.resizeTo(this._wasStyled?this._lastSize:this._currentSize,_18);
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
},addImage:function(_19,_1a){
var g=_1a;
if(!_19.href){
return;
}
if(g){
if(!this._groups[g]){
this._groups[g]=[];
}
this._groups[g].push(_19);
}else{
this._groups["XnoGroupX"].push(_19);
}
},removeImage:function(_1b){
var g=_1b.group||"XnoGroupX";
_2.every(this._groups[g],function(_1c,i,ar){
if(_1c.href==_1b.href){
ar.splice(i,1);
return false;
}
return true;
});
},removeGroup:function(_1d){
if(this._groups[_1d]){
this._groups[_1d]=[];
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
},onClick:function(_1e){
},_onImageClick:function(e){
if(e&&e.target==this.imgNode){
this.onClick(this._lastGroup);
if(this._lastGroup.declaredClass){
this._lastGroup.onClick(this._lastGroup);
}
}
}});
return _4.image.Lightbox;
});
