//>>built
define("dojox/image/Gallery",["dojo","dijit","dojox","dojo/require!dojo/fx,dijit/_Widget,dijit/_Templated,dojox/image/ThumbnailPicker,dojox/image/SlideShow"],function(_1,_2,_3){
_1.provide("dojox.image.Gallery");
_1.experimental("dojox.image.Gallery");
_1.require("dojo.fx");
_1.require("dijit._Widget");
_1.require("dijit._Templated");
_1.require("dojox.image.ThumbnailPicker");
_1.require("dojox.image.SlideShow");
_1.declare("dojox.image.Gallery",[_2._Widget,_2._Templated],{imageHeight:375,imageWidth:500,pageSize:_3.image.SlideShow.prototype.pageSize,autoLoad:true,linkAttr:"link",imageThumbAttr:"imageUrlThumb",imageLargeAttr:"imageUrl",titleAttr:"title",slideshowInterval:3,templateString:_1.cache("dojox.image","resources/Gallery.html","<div dojoAttachPoint=\"outerNode\" class=\"imageGalleryWrapper\">\n\t<div dojoAttachPoint=\"thumbPickerNode\"></div>\n\t<div dojoAttachPoint=\"slideShowNode\"></div>\n</div>"),postCreate:function(){
this.widgetid=this.id;
this.inherited(arguments);
this.thumbPicker=new _3.image.ThumbnailPicker({linkAttr:this.linkAttr,imageLargeAttr:this.imageLargeAttr,imageThumbAttr:this.imageThumbAttr,titleAttr:this.titleAttr,useLoadNotifier:true,size:this.imageWidth},this.thumbPickerNode);
this.slideShow=new _3.image.SlideShow({imageHeight:this.imageHeight,imageWidth:this.imageWidth,autoLoad:this.autoLoad,linkAttr:this.linkAttr,imageLargeAttr:this.imageLargeAttr,titleAttr:this.titleAttr,slideshowInterval:this.slideshowInterval,pageSize:this.pageSize},this.slideShowNode);
var _4=this;
_1.subscribe(this.slideShow.getShowTopicName(),function(_5){
_4.thumbPicker._showThumbs(_5.index);
});
_1.subscribe(this.thumbPicker.getClickTopicName(),function(_6){
_4.slideShow.showImage(_6.index);
});
_1.subscribe(this.thumbPicker.getShowTopicName(),function(_7){
_4.slideShow.moveImageLoadingPointer(_7.index);
});
_1.subscribe(this.slideShow.getLoadTopicName(),function(_8){
_4.thumbPicker.markImageLoaded(_8);
});
this._centerChildren();
},setDataStore:function(_9,_a,_b){
this.thumbPicker.setDataStore(_9,_a,_b);
this.slideShow.setDataStore(_9,_a,_b);
},reset:function(){
this.slideShow.reset();
this.thumbPicker.reset();
},showNextImage:function(_c){
this.slideShow.showNextImage();
},toggleSlideshow:function(){
_1.deprecated("dojox.widget.Gallery.toggleSlideshow is deprecated.  Use toggleSlideShow instead.","","2.0");
this.toggleSlideShow();
},toggleSlideShow:function(){
this.slideShow.toggleSlideShow();
},showImage:function(_d,_e){
this.slideShow.showImage(_d,_e);
},resize:function(_f){
this.thumbPicker.resize(_f);
},_centerChildren:function(){
var _10=_1.marginBox(this.thumbPicker.outerNode);
var _11=_1.marginBox(this.slideShow.outerNode);
var _12=(_10.w-_11.w)/2;
if(_12>0){
_1.style(this.slideShow.outerNode,"marginLeft",_12+"px");
}else{
if(_12<0){
_1.style(this.thumbPicker.outerNode,"marginLeft",(_12*-1)+"px");
}
}
}});
});
