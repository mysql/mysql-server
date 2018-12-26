//>>built
define("dojox/mobile/Carousel",["dojo/_base/kernel","dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/_base/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./PageIndicator","./SwapView","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
_1.experimental("dojox.mobile.Carousel");
return _4("dojox.mobile.Carousel",[_d,_c,_b],{numVisible:3,title:"",pageIndicator:true,navButton:false,height:"300px",store:null,query:null,queryOptions:null,buildRendering:function(){
this.inherited(arguments);
this.domNode.className="mblCarousel";
var h;
if(this.height==="inherit"){
if(this.domNode.offsetParent){
h=this.domNode.offsetParent.offsetHeight+"px";
}
}else{
if(this.height){
h=this.height;
}
}
this.domNode.style.height=h;
this.headerNode=_9.create("DIV",{className:"mblCarouselHeaderBar"},this.domNode);
if(this.navButton){
this.btnContainerNode=_9.create("DIV",{className:"mblCarouselBtnContainer"},this.headerNode);
_a.set(this.btnContainerNode,"float","right");
this.prevBtnNode=_9.create("BUTTON",{className:"mblCarouselBtn",title:"Previous",innerHTML:"&lt;"},this.btnContainerNode);
this.nextBtnNode=_9.create("BUTTON",{className:"mblCarouselBtn",title:"Next",innerHTML:"&gt;"},this.btnContainerNode);
this.connect(this.prevBtnNode,"onclick","onPrevBtnClick");
this.connect(this.nextBtnNode,"onclick","onNextBtnClick");
}
if(this.pageIndicator){
if(!this.title){
this.title="&nbsp;";
}
this.piw=new _e();
_a.set(this.piw,"float","right");
this.headerNode.appendChild(this.piw.domNode);
}
this.titleNode=_9.create("DIV",{className:"mblCarouselTitle"},this.headerNode);
this.containerNode=_9.create("DIV",{className:"mblCarouselPages"},this.domNode);
_3.subscribe("/dojox/mobile/viewChanged",this,"handleViewChanged");
},startup:function(){
if(this._started){
return;
}
if(this.store){
var _11=this.store;
this.store=null;
this.setStore(_11,this.query,this.queryOptions);
}
this.inherited(arguments);
},setStore:function(_12,_13,_14){
if(_12===this.store){
return;
}
this.store=_12;
this.query=_13;
this.queryOptions=_14;
this.refresh();
},refresh:function(){
if(!this.store){
return;
}
this.store.fetch({query:this.query,queryOptions:this.queryOptions,onComplete:_6.hitch(this,"generate"),onError:_6.hitch(this,"onError")});
},generate:function(_15,_16){
_2.forEach(this.getChildren(),function(_17){
if(_17 instanceof _f){
_17.destroyRecursive();
}
});
this.items=_15;
this.swapViews=[];
this.images=[];
var _18=Math.ceil(_15.length/this.numVisible);
var h=this.domNode.offsetHeight-this.headerNode.offsetHeight;
for(var i=0;i<_18;i++){
var w=new _f({height:h+"px"});
this.addChild(w);
this.swapViews.push(w);
w._carouselImages=[];
if(i===0&&this.piw){
this.piw.refId=w.id;
}
for(var j=0;j<this.numVisible;j++){
var idx=i*this.numVisible+j;
var _19=idx<_15.length?_15[idx]:{src:_10.toUrl("dojo/resources/blank.gif"),height:"1px"};
var _1a=w.domNode.style.display;
w.domNode.style.display="";
var box=this.createBox(_19,h);
w.containerNode.appendChild(box);
box.appendChild(this.createHeaderText(_19));
var img=this.createContent(_19,idx);
box.appendChild(img);
box.appendChild(this.createFooterText(_19));
this.resizeContent(_19,box,img);
w.domNode.style.display=_1a;
if(_19.height!=="1px"){
this.images.push(img);
w._carouselImages.push(img);
}
}
}
if(this.swapViews[0]){
this.loadImages(this.swapViews[0]);
}
if(this.swapViews[1]){
this.loadImages(this.swapViews[1]);
}
this.currentView=this.swapViews[0];
if(this.piw){
this.piw.reset();
}
},createBox:function(_1b,h){
var _1c=_1b.width||(90/this.numVisible+"%");
var _1d=_1b.height||h+"px";
var m=_7("ie")?5/this.numVisible-1:5/this.numVisible;
var _1e=_1b.margin||(m+"%");
var box=_9.create("DIV",{className:"mblCarouselBox"});
_a.set(box,{margin:"0px "+_1e,width:_1c,height:_1d});
return box;
},createHeaderText:function(_1f){
this.headerTextNode=_9.create("DIV",{className:"mblCarouselImgHeaderText",innerHTML:_1f.headerText?_1f.headerText:"&nbsp;"});
return this.headerTextNode;
},createContent:function(_20,idx){
var _21={alt:_20.alt||"",tabIndex:"0",className:"mblCarouselImg"};
var img=_9.create("IMG",_21);
img._idx=idx;
if(_20.height!=="1px"){
this.connect(img,"onclick","onClick");
this.connect(img,"onkeydown","onClick");
_3.connect(img,"ondragstart",_5.stop);
}else{
img.style.visibility="hidden";
}
return img;
},createFooterText:function(_22){
this.footerTextNode=_9.create("DIV",{className:"mblCarouselImgFooterText",innerHTML:_22.footerText?_22.footerText:"&nbsp;"});
return this.footerTextNode;
},resizeContent:function(_23,box,img){
if(_23.height!=="1px"){
img.style.height=(box.offsetHeight-this.headerTextNode.offsetHeight-this.footerTextNode.offsetHeight)+"px";
}
},onError:function(_24){
},onPrevBtnClick:function(e){
if(this.currentView){
this.currentView.goTo(-1);
}
},onNextBtnClick:function(e){
if(this.currentView){
this.currentView.goTo(1);
}
},onClick:function(e){
if(e&&e.type==="keydown"&&e.keyCode!==13){
return;
}
var img=e.currentTarget;
for(var i=0;i<this.images.length;i++){
if(this.images[i]===img){
_8.add(img,"mblCarouselImgSelected");
}else{
_8.remove(this.images[i],"mblCarouselImgSelected");
}
}
_a.set(img,"opacity",0.4);
setTimeout(function(){
_a.set(img,"opacity",1);
},1000);
_3.publish("/dojox/mobile/carouselSelect",[this,img,this.items[img._idx],img._idx]);
},loadImages:function(_25){
if(!_25){
return;
}
var _26=_25._carouselImages;
_2.forEach(_26,function(img){
if(!img.src){
var _27=this.items[img._idx];
img.src=_27.src;
}
},this);
},handleViewChanged:function(_28){
if(_28.getParent()!==this){
return;
}
this.currentView=_28;
this.loadImages(_28.nextView(_28.domNode));
},_setTitleAttr:function(_29){
this.title=_29;
this.titleNode.innerHTML=this._cv?this._cv(_29):_29;
}});
});
