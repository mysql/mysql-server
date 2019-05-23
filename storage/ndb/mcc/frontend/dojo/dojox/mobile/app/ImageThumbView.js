//>>built
define("dojox/mobile/app/ImageThumbView",["dojo","dijit","dojox","dojo/require!dijit/_WidgetBase,dojo/string"],function(_1,_2,_3){
_1.provide("dojox.mobile.app.ImageThumbView");
_1.experimental("dojox.mobile.app.ImageThumbView");
_1.require("dijit._WidgetBase");
_1.require("dojo.string");
_1.declare("dojox.mobile.app.ImageThumbView",_2._WidgetBase,{items:[],urlParam:"url",labelParam:null,itemTemplate:"<div class=\"mblThumbInner\">"+"<div class=\"mblThumbOverlay\"></div>"+"<div class=\"mblThumbMask\">"+"<div class=\"mblThumbSrc\" style=\"background-image:url(${url})\"></div>"+"</div>"+"</div>",minPadding:4,maxPerRow:3,maxRows:-1,baseClass:"mblImageThumbView",thumbSize:"medium",animationEnabled:true,selectedIndex:-1,cache:null,cacheMustMatch:false,clickEvent:"onclick",cacheBust:false,disableHide:false,constructor:function(_4,_5){
},postCreate:function(){
this.inherited(arguments);
var _6=this;
var _7="mblThumbHover";
this.addThumb=_1.hitch(this,this.addThumb);
this.handleImgLoad=_1.hitch(this,this.handleImgLoad);
this.hideCached=_1.hitch(this,this.hideCached);
this._onLoadImages={};
this.cache=[];
this.visibleImages=[];
this._cacheCounter=0;
this.connect(this.domNode,this.clickEvent,function(_8){
var _9=_6._getItemNodeFromEvent(_8);
if(_9&&!_9._cached){
_6.onSelect(_9._item,_9._index,_6.items);
_1.query(".selected",this.domNode).removeClass("selected");
_1.addClass(_9,"selected");
}
});
_1.addClass(this.domNode,this.thumbSize);
this.resize();
this.render();
},onSelect:function(_a,_b,_c){
},_setAnimationEnabledAttr:function(_d){
this.animationEnabled=_d;
_1[_d?"addClass":"removeClass"](this.domNode,"animated");
},_setItemsAttr:function(_e){
this.items=_e||[];
var _f={};
var i;
for(i=0;i<this.items.length;i++){
_f[this.items[i][this.urlParam]]=1;
}
var _10=[];
for(var url in this._onLoadImages){
if(!_f[url]&&this._onLoadImages[url]._conn){
_1.disconnect(this._onLoadImages[url]._conn);
this._onLoadImages[url].src=null;
_10.push(url);
}
}
for(i=0;i<_10.length;i++){
delete this._onLoadImages[url];
}
this.render();
},_getItemNode:function(_11){
while(_11&&!_1.hasClass(_11,"mblThumb")&&_11!=this.domNode){
_11=_11.parentNode;
}
return (_11==this.domNode)?null:_11;
},_getItemNodeFromEvent:function(_12){
if(_12.touches&&_12.touches.length>0){
_12=_12.touches[0];
}
return this._getItemNode(_12.target);
},resize:function(){
this._thumbSize=null;
this._size=_1.contentBox(this.domNode);
this.disableHide=true;
this.render();
this.disableHide=false;
},hideCached:function(){
for(var i=0;i<this.cache.length;i++){
if(this.cache[i]){
_1.style(this.cache[i],"display","none");
}
}
},render:function(){
var i;
var url;
var _13;
var _14;
while(this.visibleImages&&this.visibleImages.length>0){
_14=this.visibleImages.pop();
this.cache.push(_14);
if(!this.disableHide){
_1.addClass(_14,"hidden");
}
_14._cached=true;
}
if(this.cache&&this.cache.length>0){
setTimeout(this.hideCached,1000);
}
if(!this.items||this.items.length==0){
return;
}
for(i=0;i<this.items.length;i++){
_13=this.items[i];
url=(_1.isString(_13)?_13:_13[this.urlParam]);
this.addThumb(_13,url,i);
if(this.maxRows>0&&(i+1)/this.maxPerRow>=this.maxRows){
break;
}
}
if(!this._thumbSize){
return;
}
var _15=0;
var row=-1;
var _16=this._thumbSize.w+(this.padding*2);
var _17=this._thumbSize.h+(this.padding*2);
var _18=this.thumbNodes=_1.query(".mblThumb",this.domNode);
var pos=0;
_18=this.visibleImages;
for(i=0;i<_18.length;i++){
if(_18[i]._cached){
continue;
}
if(pos%this.maxPerRow==0){
row++;
}
_15=pos%this.maxPerRow;
this.place(_18[i],(_15*_16)+this.padding,(row*_17)+this.padding);
if(!_18[i]._loading){
_1.removeClass(_18[i],"hidden");
}
if(pos==this.selectedIndex){
_1[pos==this.selectedIndex?"addClass":"removeClass"](_18[i],"selected");
}
pos++;
}
var _19=Math.ceil(pos/this.maxPerRow);
this._numRows=_19;
this.setContainerHeight((_19*(this._thumbSize.h+this.padding*2)));
},setContainerHeight:function(_1a){
_1.style(this.domNode,"height",_1a+"px");
},addThumb:function(_1b,url,_1c){
var _1d;
var _1e=false;
if(this.cache.length>0){
var _1f=false;
for(var i=0;i<this.cache.length;i++){
if(this.cache[i]._url==url){
_1d=this.cache.splice(i,1)[0];
_1f=true;
break;
}
}
if(!_1d&&!this.cacheMustMatch){
_1d=this.cache.pop();
_1.removeClass(_1d,"selected");
}else{
_1e=true;
}
}
if(!_1d){
_1d=_1.create("div",{"class":"mblThumb hidden",innerHTML:_1.string.substitute(this.itemTemplate,{url:url},null,this)},this.domNode);
}
if(this.labelParam){
var _20=_1.query(".mblThumbLabel",_1d)[0];
if(!_20){
_20=_1.create("div",{"class":"mblThumbLabel"},_1d);
}
_20.innerHTML=_1b[this.labelParam]||"";
}
_1.style(_1d,"display","");
if(!this.disableHide){
_1.addClass(_1d,"hidden");
}
if(!_1e){
var _21=_1.create("img",{});
_21._thumbDiv=_1d;
_21._conn=_1.connect(_21,"onload",this.handleImgLoad);
_21._url=url;
_1d._loading=true;
this._onLoadImages[url]=_21;
if(_21){
_21.src=url;
}
}
this.visibleImages.push(_1d);
_1d._index=_1c;
_1d._item=_1b;
_1d._url=url;
_1d._cached=false;
if(!this._thumbSize){
this._thumbSize=_1.marginBox(_1d);
if(this._thumbSize.h==0){
this._thumbSize.h=100;
this._thumbSize.w=100;
}
if(this.labelParam){
this._thumbSize.h+=8;
}
this.calcPadding();
}
},handleImgLoad:function(_22){
var img=_22.target;
_1.disconnect(img._conn);
_1.removeClass(img._thumbDiv,"hidden");
img._thumbDiv._loading=false;
img._conn=null;
var url=img._url;
if(this.cacheBust){
url+=(url.indexOf("?")>-1?"&":"?")+"cacheBust="+(new Date()).getTime()+"_"+(this._cacheCounter++);
}
_1.query(".mblThumbSrc",img._thumbDiv).style("backgroundImage","url("+url+")");
delete this._onLoadImages[img._url];
},calcPadding:function(){
var _23=this._size.w;
var _24=this._thumbSize.w;
var _25=_24+this.minPadding;
this.maxPerRow=Math.floor(_23/_25);
this.padding=Math.floor((_23-(_24*this.maxPerRow))/(this.maxPerRow*2));
},place:function(_26,x,y){
_1.style(_26,{"-webkit-transform":"translate("+x+"px,"+y+"px)"});
},destroy:function(){
var img;
var _27=0;
for(var url in this._onLoadImages){
img=this._onLoadImages[url];
if(img){
img.src=null;
_27++;
}
}
this.inherited(arguments);
}});
});
