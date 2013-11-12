//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/string,dojo/fx,dijit/_Widget,dijit/_Templated"],function(_1,_2,_3){
_2.provide("dojox.image.SlideShow");
_2.require("dojo.string");
_2.require("dojo.fx");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.declare("dojox.image.SlideShow",[_1._Widget,_1._Templated],{imageHeight:375,imageWidth:500,title:"",titleTemplate:"${title} <span class=\"slideShowCounterText\">(${current} of ${total})</span>",noLink:false,loop:true,hasNav:true,images:[],pageSize:20,autoLoad:true,autoStart:false,fixedHeight:false,imageStore:null,linkAttr:"link",imageLargeAttr:"imageUrl",titleAttr:"title",slideshowInterval:3,templateString:_2.cache("dojox.image","resources/SlideShow.html","<div dojoAttachPoint=\"outerNode\" class=\"slideShowWrapper\">\n\t<div style=\"position:relative;\" dojoAttachPoint=\"innerWrapper\">\n\t\t<div class=\"slideShowNav\" dojoAttachEvent=\"onclick: _handleClick\">\n\t\t\t<div class=\"dijitInline slideShowTitle\" dojoAttachPoint=\"titleNode\">${title}</div>\n\t\t</div>\n\t\t<div dojoAttachPoint=\"navNode\" class=\"slideShowCtrl\" dojoAttachEvent=\"onclick: _handleClick\">\n\t\t\t<span dojoAttachPoint=\"navPrev\" class=\"slideShowCtrlPrev\"></span>\n\t\t\t<span dojoAttachPoint=\"navPlay\" class=\"slideShowCtrlPlay\"></span>\n\t\t\t<span dojoAttachPoint=\"navNext\" class=\"slideShowCtrlNext\"></span>\n\t\t</div>\n\t\t<div dojoAttachPoint=\"largeNode\" class=\"slideShowImageWrapper\"></div>\t\t\n\t\t<div dojoAttachPoint=\"hiddenNode\" class=\"slideShowHidden\"></div>\n\t</div>\n</div>"),_imageCounter:0,_tmpImage:null,_request:null,postCreate:function(){
this.inherited(arguments);
var _4=document.createElement("img");
_4.setAttribute("width",this.imageWidth);
_4.setAttribute("height",this.imageHeight);
if(this.hasNav){
_2.connect(this.outerNode,"onmouseover",this,function(_5){
try{
this._showNav();
}
catch(e){
}
});
_2.connect(this.outerNode,"onmouseout",this,function(_6){
try{
this._hideNav(_6);
}
catch(e){
}
});
}
this.outerNode.style.width=this.imageWidth+"px";
_4.setAttribute("src",this._blankGif);
var _7=this;
this.largeNode.appendChild(_4);
this._tmpImage=this._currentImage=_4;
this._fitSize(true);
this._loadImage(0,_2.hitch(this,"showImage",0));
this._calcNavDimensions();
_2.style(this.navNode,"opacity",0);
},setDataStore:function(_8,_9,_a){
this.reset();
var _b=this;
this._request={query:{},start:_9.start||0,count:_9.count||this.pageSize,onBegin:function(_c,_d){
_b.maxPhotos=_c;
}};
if(_9.query){
_2.mixin(this._request.query,_9.query);
}
if(_a){
_2.forEach(["imageLargeAttr","linkAttr","titleAttr"],function(_e){
if(_a[_e]){
this[_e]=_a[_e];
}
},this);
}
var _f=function(_10){
_b.maxPhotos=_10.length;
_b._request.onComplete=null;
if(_b.autoStart){
_b.imageIndex=-1;
_b.toggleSlideShow();
}else{
_b.showImage(0);
}
};
this.imageStore=_8;
this._request.onComplete=_f;
this._request.start=0;
this.imageStore.fetch(this._request);
},reset:function(){
_2.query("> *",this.largeNode).orphan();
this.largeNode.appendChild(this._tmpImage);
_2.query("> *",this.hiddenNode).orphan();
_2.forEach(this.images,function(img){
if(img&&img.parentNode){
img.parentNode.removeChild(img);
}
});
this.images=[];
this.isInitialized=false;
this._imageCounter=0;
},isImageLoaded:function(_11){
return this.images&&this.images.length>_11&&this.images[_11];
},moveImageLoadingPointer:function(_12){
this._imageCounter=_12;
},destroy:function(){
if(this._slideId){
this._stop();
}
this.inherited(arguments);
},showNextImage:function(_13,_14){
if(_13&&this._timerCancelled){
return false;
}
if(this.imageIndex+1>=this.maxPhotos){
if(_13&&(this.loop||_14)){
this.imageIndex=-1;
}else{
if(this._slideId){
this._stop();
}
return false;
}
}
this.showImage(this.imageIndex+1,_2.hitch(this,function(){
if(_13){
this._startTimer();
}
}));
return true;
},toggleSlideShow:function(){
if(this._slideId){
this._stop();
}else{
_2.toggleClass(this.domNode,"slideShowPaused");
this._timerCancelled=false;
var idx=this.imageIndex;
if(idx<0||(this.images[idx]&&this.images[idx]._img.complete)){
var _15=this.showNextImage(true,true);
if(!_15){
this._stop();
}
}else{
var _16=_2.subscribe(this.getShowTopicName(),_2.hitch(this,function(_17){
setTimeout(_2.hitch(this,function(){
if(_17.index==idx){
var _18=this.showNextImage(true,true);
if(!_18){
this._stop();
}
_2.unsubscribe(_16);
}
}),this.slideshowInterval*1000);
}));
_2.publish(this.getShowTopicName(),[{index:idx,title:"",url:""}]);
}
}
},getShowTopicName:function(){
return (this.widgetId||this.id)+"/imageShow";
},getLoadTopicName:function(){
return (this.widgetId?this.widgetId:this.id)+"/imageLoad";
},showImage:function(_19,_1a){
if(!_1a&&this._slideId){
this.toggleSlideShow();
}
var _1b=this;
var _1c=this.largeNode.getElementsByTagName("div");
this.imageIndex=_19;
var _1d=function(){
if(_1b.images[_19]){
while(_1b.largeNode.firstChild){
_1b.largeNode.removeChild(_1b.largeNode.firstChild);
}
_2.style(_1b.images[_19],"opacity",0);
_1b.largeNode.appendChild(_1b.images[_19]);
_1b._currentImage=_1b.images[_19]._img;
_1b._fitSize();
var _1e=function(a,b,c){
var img=_1b.images[_19].firstChild;
if(img.tagName.toLowerCase()!="img"){
img=img.firstChild;
}
var _1f=img.getAttribute("title")||"";
if(_1b._navShowing){
_1b._showNav(true);
}
_2.publish(_1b.getShowTopicName(),[{index:_19,title:_1f,url:img.getAttribute("src")}]);
if(_1a){
_1a(a,b,c);
}
_1b._setTitle(_1f);
};
_2.fadeIn({node:_1b.images[_19],duration:300,onEnd:_1e}).play();
}else{
_1b._loadImage(_19,function(){
_1b.showImage(_19,_1a);
});
}
};
if(_1c&&_1c.length>0){
_2.fadeOut({node:_1c[0],duration:300,onEnd:function(){
_1b.hiddenNode.appendChild(_1c[0]);
_1d();
}}).play();
}else{
_1d();
}
},_fitSize:function(_20){
if(!this.fixedHeight||_20){
var _21=(this._currentImage.height+(this.hasNav?20:0));
_2.style(this.innerWrapper,"height",_21+"px");
return;
}
_2.style(this.largeNode,"paddingTop",this._getTopPadding()+"px");
},_getTopPadding:function(){
if(!this.fixedHeight){
return 0;
}
return (this.imageHeight-this._currentImage.height)/2;
},_loadNextImage:function(){
if(!this.autoLoad){
return;
}
while(this.images.length>=this._imageCounter&&this.images[this._imageCounter]){
this._imageCounter++;
}
this._loadImage(this._imageCounter);
},_loadImage:function(_22,_23){
if(this.images[_22]||!this._request){
return;
}
var _24=_22-(_22%(this._request.count||this.pageSize));
this._request.start=_24;
this._request.onComplete=function(_25){
var _26=_22-_24;
if(_25&&_25.length>_26){
_27(_25[_26]);
}else{
}
};
var _28=this;
var _29=this.imageStore;
var _27=function(_2a){
var url=_28.imageStore.getValue(_2a,_28.imageLargeAttr);
var img=new Image();
var div=_2.create("div",{id:_28.id+"_imageDiv"+_22});
div._img=img;
var _2b=_28.imageStore.getValue(_2a,_28.linkAttr);
if(!_2b||_28.noLink){
div.appendChild(img);
}else{
var a=_2.create("a",{"href":_2b,"target":"_blank"},div);
a.appendChild(img);
}
_2.connect(img,"onload",function(){
if(_29!=_28.imageStore){
return;
}
_28._fitImage(img);
_2.attr(div,{"width":_28.imageWidth,"height":_28.imageHeight});
_2.publish(_28.getLoadTopicName(),[_22]);
setTimeout(function(){
_28._loadNextImage();
},1);
if(_23){
_23();
}
});
_28.hiddenNode.appendChild(div);
var _2c=_2.create("div",{className:"slideShowTitle"},div);
_28.images[_22]=div;
_2.attr(img,"src",url);
var _2d=_28.imageStore.getValue(_2a,_28.titleAttr);
if(_2d){
_2.attr(img,"title",_2d);
}
};
this.imageStore.fetch(this._request);
},_stop:function(){
if(this._slideId){
clearTimeout(this._slideId);
}
this._slideId=null;
this._timerCancelled=true;
_2.removeClass(this.domNode,"slideShowPaused");
},_prev:function(){
if(this.imageIndex<1){
return;
}
this.showImage(this.imageIndex-1);
},_next:function(){
this.showNextImage();
},_startTimer:function(){
var id=this.id;
this._slideId=setTimeout(function(){
_1.byId(id).showNextImage(true);
},this.slideshowInterval*1000);
},_calcNavDimensions:function(){
_2.style(this.navNode,"position","absolute");
_2.style(this.navNode,"top","-10000px");
_2.style(this.navPlay,"marginLeft",0);
this.navPlay._size=_2.marginBox(this.navPlay);
this.navPrev._size=_2.marginBox(this.navPrev);
this.navNext._size=_2.marginBox(this.navNext);
_2.style(this.navNode,{"position":"",top:""});
},_setTitle:function(_2e){
this.titleNode.innerHTML=_2.string.substitute(this.titleTemplate,{title:_2e,current:1+this.imageIndex,total:this.maxPhotos||""});
},_fitImage:function(img){
var _2f=img.width;
var _30=img.height;
if(_2f>this.imageWidth){
_30=Math.floor(_30*(this.imageWidth/_2f));
img.height=_30;
img.width=_2f=this.imageWidth;
}
if(_30>this.imageHeight){
_2f=Math.floor(_2f*(this.imageHeight/_30));
img.height=this.imageHeight;
img.width=_2f;
}
},_handleClick:function(e){
switch(e.target){
case this.navNext:
this._next();
break;
case this.navPrev:
this._prev();
break;
case this.navPlay:
this.toggleSlideShow();
break;
}
},_showNav:function(_31){
if(this._navShowing&&!_31){
return;
}
this._calcNavDimensions();
_2.style(this.navNode,"marginTop","0px");
var _32=_2.style(this.navNode,"width")/2-this.navPlay._size.w/2-this.navPrev._size.w;
_2.style(this.navPlay,"marginLeft",_32+"px");
var _33=_2.marginBox(this.outerNode);
var _34=this._currentImage.height-this.navPlay._size.h-10+this._getTopPadding();
if(_34>this._currentImage.height){
_34+=10;
}
_2[this.imageIndex<1?"addClass":"removeClass"](this.navPrev,"slideShowCtrlHide");
_2[this.imageIndex+1>=this.maxPhotos?"addClass":"removeClass"](this.navNext,"slideShowCtrlHide");
var _35=this;
if(this._navAnim){
this._navAnim.stop();
}
if(this._navShowing){
return;
}
this._navAnim=_2.fadeIn({node:this.navNode,duration:300,onEnd:function(){
_35._navAnim=null;
}});
this._navAnim.play();
this._navShowing=true;
},_hideNav:function(e){
if(!e||!this._overElement(this.outerNode,e)){
var _36=this;
if(this._navAnim){
this._navAnim.stop();
}
this._navAnim=_2.fadeOut({node:this.navNode,duration:300,onEnd:function(){
_36._navAnim=null;
}});
this._navAnim.play();
this._navShowing=false;
}
},_overElement:function(_37,e){
if(typeof (_2)=="undefined"){
return false;
}
_37=_2.byId(_37);
var m={x:e.pageX,y:e.pageY};
var bb=_2.position(_37,true);
return (m.x>=bb.x&&m.x<=(bb.x+bb.w)&&m.y>=bb.y&&m.y<=(top+bb.h));
}});
});
