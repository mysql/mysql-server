//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/fx/scroll,dojo/fx/easing,dojo/fx,dijit/_Widget,dijit/_Templated"],function(_1,_2,_3){
_2.provide("dojox.image.ThumbnailPicker");
_2.experimental("dojox.image.ThumbnailPicker");
_2.require("dojox.fx.scroll");
_2.require("dojo.fx.easing");
_2.require("dojo.fx");
_2.require("dijit._Widget");
_2.require("dijit._Templated");
_2.declare("dojox.image.ThumbnailPicker",[_1._Widget,_1._Templated],{imageStore:null,request:null,size:500,thumbHeight:75,thumbWidth:100,useLoadNotifier:false,useHyperlink:false,hyperlinkTarget:"new",isClickable:true,isScrollable:true,isHorizontal:true,autoLoad:true,linkAttr:"link",imageThumbAttr:"imageUrlThumb",imageLargeAttr:"imageUrl",pageSize:20,titleAttr:"title",templateString:_2.cache("dojox.image","resources/ThumbnailPicker.html","<div dojoAttachPoint=\"outerNode\" class=\"thumbOuter\">\n\t<div dojoAttachPoint=\"navPrev\" class=\"thumbNav thumbClickable\">\n\t  <img src=\"\" dojoAttachPoint=\"navPrevImg\"/>    \n\t</div>\n\t<div dojoAttachPoint=\"thumbScroller\" class=\"thumbScroller\">\n\t  <div dojoAttachPoint=\"thumbsNode\" class=\"thumbWrapper\"></div>\n\t</div>\n\t<div dojoAttachPoint=\"navNext\" class=\"thumbNav thumbClickable\">\n\t  <img src=\"\" dojoAttachPoint=\"navNextImg\"/>  \n\t</div>\n</div>"),_thumbs:[],_thumbIndex:0,_maxPhotos:0,_loadedImages:{},postCreate:function(){
this.widgetid=this.id;
this.inherited(arguments);
this.pageSize=Number(this.pageSize);
this._scrollerSize=this.size-(51*2);
var _4=this._sizeProperty=this.isHorizontal?"width":"height";
_2.style(this.outerNode,"textAlign","center");
_2.style(this.outerNode,_4,this.size+"px");
_2.style(this.thumbScroller,_4,this._scrollerSize+"px");
if(this.useHyperlink){
_2.subscribe(this.getClickTopicName(),this,function(_5){
var _6=_5.index;
var _7=this.imageStore.getValue(_5.data,this.linkAttr);
if(!_7){
return;
}
if(this.hyperlinkTarget=="new"){
window.open(_7);
}else{
window.location=_7;
}
});
}
if(this.isClickable){
_2.addClass(this.thumbsNode,"thumbClickable");
}
this._totalSize=0;
this.init();
},init:function(){
if(this.isInitialized){
return false;
}
var _8=this.isHorizontal?"Horiz":"Vert";
_2.addClass(this.navPrev,"prev"+_8);
_2.addClass(this.navNext,"next"+_8);
_2.addClass(this.thumbsNode,"thumb"+_8);
_2.addClass(this.outerNode,"thumb"+_8);
_2.attr(this.navNextImg,"src",this._blankGif);
_2.attr(this.navPrevImg,"src",this._blankGif);
this.connect(this.navPrev,"onclick","_prev");
this.connect(this.navNext,"onclick","_next");
this.isInitialized=true;
if(this.isHorizontal){
this._offsetAttr="offsetLeft";
this._sizeAttr="offsetWidth";
this._scrollAttr="scrollLeft";
}else{
this._offsetAttr="offsetTop";
this._sizeAttr="offsetHeight";
this._scrollAttr="scrollTop";
}
this._updateNavControls();
if(this.imageStore&&this.request){
this._loadNextPage();
}
return true;
},getClickTopicName:function(){
return (this.widgetId||this.id)+"/select";
},getShowTopicName:function(){
return (this.widgetId||this.id)+"/show";
},setDataStore:function(_9,_a,_b){
this.reset();
this.request={query:{},start:_a.start||0,count:_a.count||10,onBegin:_2.hitch(this,function(_c){
this._maxPhotos=_c;
})};
if(_a.query){
_2.mixin(this.request.query,_a.query);
}
if(_b){
_2.forEach(["imageThumbAttr","imageLargeAttr","linkAttr","titleAttr"],function(_d){
if(_b[_d]){
this[_d]=_b[_d];
}
},this);
}
this.request.start=0;
this.request.count=this.pageSize;
this.imageStore=_9;
this._loadInProgress=false;
if(!this.init()){
this._loadNextPage();
}
},reset:function(){
this._loadedImages={};
_2.forEach(this._thumbs,function(_e){
if(_e&&_e.parentNode){
_2.destroy(_e);
}
});
this._thumbs=[];
this.isInitialized=false;
this._noImages=true;
},isVisible:function(_f){
var img=this._thumbs[_f];
if(!img){
return false;
}
var pos=this.isHorizontal?"offsetLeft":"offsetTop";
var _10=this.isHorizontal?"offsetWidth":"offsetHeight";
var _11=this.isHorizontal?"scrollLeft":"scrollTop";
var _12=img[pos]-this.thumbsNode[pos];
return (_12>=this.thumbScroller[_11]&&_12+img[_10]<=this.thumbScroller[_11]+this._scrollerSize);
},resize:function(dim){
var _13=this.isHorizontal?"w":"h";
var _14=0;
if(this._thumbs.length>0&&_2.marginBox(this._thumbs[0]).w==0){
return;
}
_2.forEach(this._thumbs,_2.hitch(this,function(_15){
var mb=_2.marginBox(_15.firstChild);
var _16=mb[_13];
_14+=(Number(_16)+10);
if(this.useLoadNotifier&&mb.w>0){
_2.style(_15.lastChild,"width",(mb.w-4)+"px");
}
_2.style(_15,"width",mb.w+"px");
}));
_2.style(this.thumbsNode,this._sizeProperty,_14+"px");
this._updateNavControls();
},_next:function(){
var pos=this.isHorizontal?"offsetLeft":"offsetTop";
var _17=this.isHorizontal?"offsetWidth":"offsetHeight";
var _18=this.thumbsNode[pos];
var _19=this._thumbs[this._thumbIndex];
var _1a=_19[pos]-_18;
var _1b=-1,img;
for(var i=this._thumbIndex+1;i<this._thumbs.length;i++){
img=this._thumbs[i];
if(img[pos]-_18+img[_17]-_1a>this._scrollerSize){
this._showThumbs(i);
return;
}
}
},_prev:function(){
if(this.thumbScroller[this.isHorizontal?"scrollLeft":"scrollTop"]==0){
return;
}
var pos=this.isHorizontal?"offsetLeft":"offsetTop";
var _1c=this.isHorizontal?"offsetWidth":"offsetHeight";
var _1d=this._thumbs[this._thumbIndex];
var _1e=_1d[pos]-this.thumbsNode[pos];
var _1f=-1,img;
for(var i=this._thumbIndex-1;i>-1;i--){
img=this._thumbs[i];
if(_1e-img[pos]>this._scrollerSize){
this._showThumbs(i+1);
return;
}
}
this._showThumbs(0);
},_checkLoad:function(img,_20){
_2.publish(this.getShowTopicName(),[{index:_20}]);
this._updateNavControls();
this._loadingImages={};
this._thumbIndex=_20;
if(this.thumbsNode.offsetWidth-img.offsetLeft<(this._scrollerSize*2)){
this._loadNextPage();
}
},_showThumbs:function(_21){
_21=Math.min(Math.max(_21,0),this._maxPhotos);
if(_21>=this._maxPhotos){
return;
}
var img=this._thumbs[_21];
if(!img){
return;
}
var _22=img.offsetLeft-this.thumbsNode.offsetLeft;
var top=img.offsetTop-this.thumbsNode.offsetTop;
var _23=this.isHorizontal?_22:top;
if((_23>=this.thumbScroller[this._scrollAttr])&&(_23+img[this._sizeAttr]<=this.thumbScroller[this._scrollAttr]+this._scrollerSize)){
return;
}
if(this.isScrollable){
var _24=this.isHorizontal?{x:_22,y:0}:{x:0,y:top};
_3.fx.smoothScroll({target:_24,win:this.thumbScroller,duration:300,easing:_2.fx.easing.easeOut,onEnd:_2.hitch(this,"_checkLoad",img,_21)}).play(10);
}else{
if(this.isHorizontal){
this.thumbScroller.scrollLeft=_22;
}else{
this.thumbScroller.scrollTop=top;
}
this._checkLoad(img,_21);
}
},markImageLoaded:function(_25){
var _26=_2.byId("loadingDiv_"+this.widgetid+"_"+_25);
if(_26){
this._setThumbClass(_26,"thumbLoaded");
}
this._loadedImages[_25]=true;
},_setThumbClass:function(_27,_28){
if(!this.autoLoad){
return;
}
_2.addClass(_27,_28);
},_loadNextPage:function(){
if(this._loadInProgress){
return;
}
this._loadInProgress=true;
var _29=this.request.start+(this._noImages?0:this.pageSize);
var pos=_29;
while(pos<this._thumbs.length&&this._thumbs[pos]){
pos++;
}
var _2a=this.imageStore;
var _2b=function(_2c,_2d){
if(_2a!=this.imageStore){
return;
}
if(_2c&&_2c.length){
var _2e=0;
var _2f=_2.hitch(this,function(){
if(_2e>=_2c.length){
this._loadInProgress=false;
return;
}
var _30=_2e++;
this._loadImage(_2c[_30],pos+_30,_2f);
});
_2f();
this._updateNavControls();
}else{
this._loadInProgress=false;
}
};
var _31=function(){
this._loadInProgress=false;
};
this.request.onComplete=_2.hitch(this,_2b);
this.request.onError=_2.hitch(this,_31);
this.request.start=_29;
this._noImages=false;
this.imageStore.fetch(this.request);
},_loadImage:function(_32,_33,_34){
var _35=this.imageStore;
var url=_35.getValue(_32,this.imageThumbAttr);
var _36=_2.create("div",{id:"img_"+this.widgetid+"_"+_33});
var img=_2.create("img",{},_36);
img._index=_33;
img._data=_32;
this._thumbs[_33]=_36;
var _37;
if(this.useLoadNotifier){
_37=_2.create("div",{id:"loadingDiv_"+this.widgetid+"_"+_33},_36);
this._setThumbClass(_37,this._loadedImages[_33]?"thumbLoaded":"thumbNotifier");
}
var _38=_2.marginBox(this.thumbsNode);
var _39;
var _3a;
if(this.isHorizontal){
_39=this.thumbWidth;
_3a="w";
}else{
_39=this.thumbHeight;
_3a="h";
}
_38=_38[_3a];
var sl=this.thumbScroller.scrollLeft,st=this.thumbScroller.scrollTop;
_2.style(this.thumbsNode,this._sizeProperty,(_38+_39+20)+"px");
this.thumbScroller.scrollLeft=sl;
this.thumbScroller.scrollTop=st;
this.thumbsNode.appendChild(_36);
_2.connect(img,"onload",this,_2.hitch(this,function(){
if(_35!=this.imageStore){
return false;
}
this.resize();
setTimeout(_34,0);
return false;
}));
_2.connect(img,"onclick",this,function(evt){
_2.publish(this.getClickTopicName(),[{index:evt.target._index,data:evt.target._data,url:img.getAttribute("src"),largeUrl:this.imageStore.getValue(_32,this.imageLargeAttr),title:this.imageStore.getValue(_32,this.titleAttr),link:this.imageStore.getValue(_32,this.linkAttr)}]);
return false;
});
_2.addClass(img,"imageGalleryThumb");
img.setAttribute("src",url);
var _3b=this.imageStore.getValue(_32,this.titleAttr);
if(_3b){
img.setAttribute("title",_3b);
}
this._updateNavControls();
},_updateNavControls:function(){
var _3c=[];
var _3d=function(_3e,add){
var fn=add?"addClass":"removeClass";
_2[fn](_3e,"enabled");
_2[fn](_3e,"thumbClickable");
};
var pos=this.isHorizontal?"scrollLeft":"scrollTop";
var _3f=this.isHorizontal?"offsetWidth":"offsetHeight";
_3d(this.navPrev,(this.thumbScroller[pos]>0));
var _40=this._thumbs[this._thumbs.length-1];
var _41=(this.thumbScroller[pos]+this._scrollerSize<this.thumbsNode[_3f]);
_3d(this.navNext,_41);
}});
});
