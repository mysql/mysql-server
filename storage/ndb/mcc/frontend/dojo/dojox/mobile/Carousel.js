//>>built
define("dojox/mobile/Carousel",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/lang","dojo/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./lazyLoadUtils","./CarouselItem","./PageIndicator","./SwapView","require","dojo/has!dojo-bidi?dojox/mobile/bidi/Carousel","dojo/i18n!dojox/mobile/nls/messages"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14){
var _15=_3(_6("dojo-bidi")?"dojox.mobile.NonBidiCarousel":"dojox.mobile.Carousel",[_d,_c,_b],{numVisible:2,itemWidth:0,title:"",pageIndicator:true,navButton:false,height:"",selectable:true,baseClass:"mblCarousel",buildRendering:function(){
this.containerNode=_8.create("div",{className:"mblCarouselPages",id:this.id+"_pages"});
this.inherited(arguments);
var i,len;
if(this.srcNodeRef){
for(i=0,len=this.srcNodeRef.childNodes.length;i<len;i++){
this.containerNode.appendChild(this.srcNodeRef.firstChild);
}
}
this.headerNode=_8.create("div",{className:"mblCarouselHeaderBar"},this.domNode);
if(this.navButton){
this.btnContainerNode=_8.create("div",{className:"mblCarouselBtnContainer"},this.headerNode);
_9.set(this.btnContainerNode,"float","right");
this.prevBtnNode=_8.create("button",{className:"mblCarouselBtn",title:_14["CarouselPrevious"],innerHTML:"&lt;","aria-controls":this.containerNode.id},this.btnContainerNode);
this.nextBtnNode=_8.create("button",{className:"mblCarouselBtn",title:_14["CarouselNext"],innerHTML:"&gt;","aria-controls":this.containerNode.id},this.btnContainerNode);
this._prevHandle=this.connect(this.prevBtnNode,"onclick","onPrevBtnClick");
this._nextHandle=this.connect(this.nextBtnNode,"onclick","onNextBtnClick");
}
if(this.pageIndicator){
if(!this.title){
this.title="&nbsp;";
}
this.piw=new _10();
this.headerNode.appendChild(this.piw.domNode);
}
this.titleNode=_8.create("div",{className:"mblCarouselTitle"},this.headerNode);
this.domNode.appendChild(this.containerNode);
this.subscribe("/dojox/mobile/viewChanged","handleViewChanged");
this.connect(this.domNode,"onclick","_onClick");
this.connect(this.domNode,"onkeydown","_onClick");
this._dragstartHandle=this.connect(this.domNode,"ondragstart",_4.stop);
this.selectedItemIndex=-1;
this.items=[];
},startup:function(){
if(this._started){
return;
}
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
if(h){
this.domNode.style.height=h;
}
if(this.store){
if(!this.setStore){
throw new Error("Use StoreCarousel or DataCarousel instead of Carousel.");
}
var _16=this.store;
this.store=null;
this.setStore(_16,this.query,this.queryOptions);
}else{
this.resizeItems();
}
this.inherited(arguments);
this.currentView=_1.filter(this.getChildren(),function(_17){
return _17.isVisible();
})[0];
},resizeItems:function(){
var idx=0,i,len;
var h=this.domNode.offsetHeight-(this.headerNode?this.headerNode.offsetHeight:0);
var m=(_6("ie")<10)?5/this.numVisible-1:5/this.numVisible;
var _18,_19;
_1.forEach(this.getChildren(),function(_1a){
if(!(_1a instanceof _11)){
return;
}
if(!(_1a.lazy)){
_1a._instantiated=true;
}
var ch=_1a.containerNode.childNodes;
for(i=0,len=ch.length;i<len;i++){
_18=ch[i];
if(_18.nodeType!==1){
continue;
}
_19=this.items[idx]||{};
_9.set(_18,{width:_19.width||(90/this.numVisible+"%"),height:_19.height||h+"px",margin:"0 "+(_19.margin||m+"%")});
_7.add(_18,"mblCarouselSlot");
idx++;
}
},this);
if(this.piw){
this.piw.refId=this.containerNode.firstChild;
this.piw.reset();
}
},resize:function(){
if(!this.itemWidth){
return;
}
var num=Math.floor(this.domNode.offsetWidth/this.itemWidth);
if(num===this.numVisible){
return;
}
this.selectedItemIndex=this.getIndexByItemWidget(this.selectedItem);
this.numVisible=num;
if(this.items.length>0){
this.onComplete(this.items);
this.select(this.selectedItemIndex);
}
},fillPages:function(){
_1.forEach(this.getChildren(),function(_1b,i){
var s="";
var j;
for(j=0;j<this.numVisible;j++){
var _1c,_1d="",_1e;
var idx=i*this.numVisible+j;
var _1f={};
if(idx<this.items.length){
_1f=this.items[idx];
_1c=this.store.getValue(_1f,"type");
if(_1c){
_1d=this.store.getValue(_1f,"props");
_1e=this.store.getValue(_1f,"mixins");
}else{
_1c="dojox.mobile.CarouselItem";
_1.forEach(["alt","src","headerText","footerText"],function(p){
var v=this.store.getValue(_1f,p);
if(v!==undefined){
if(_1d){
_1d+=",";
}
_1d+=p+":\""+v+"\"";
}
},this);
}
}else{
_1c="dojox.mobile.CarouselItem";
_1d="src:\""+_12.toUrl("dojo/resources/blank.gif")+"\""+", className:\"mblCarouselItemBlank\"";
}
s+="<div data-dojo-type=\""+_1c+"\"";
if(_1d){
s+=" data-dojo-props='"+_1d+"'";
}
if(_1e){
s+=" data-dojo-mixins='"+_1e+"'";
}
s+="></div>";
}
_1b.containerNode.innerHTML=s;
},this);
},onComplete:function(_20){
_1.forEach(this.getChildren(),function(_21){
if(_21 instanceof _11){
_21.destroyRecursive();
}
});
this.selectedItem=null;
this.items=_20;
var _22=Math.ceil(_20.length/this.numVisible),i,h=this.domNode.offsetHeight-this.headerNode.offsetHeight,idx=this.selectedItemIndex===-1?0:this.selectedItemIndex,pg=Math.floor(idx/this.numVisible);
for(i=0;i<_22;i++){
var w=new _11({height:h+"px",lazy:true});
this.addChild(w);
if(i===pg){
w.show();
this.currentView=w;
}else{
w.hide();
}
}
this.fillPages();
this.resizeItems();
var _23=this.getChildren();
var _24=pg-1<0?0:pg-1;
var to=pg+1>_22-1?_22-1:pg+1;
for(i=_24;i<=to;i++){
this.instantiateView(_23[i]);
}
},onError:function(){
},onUpdate:function(){
},onDelete:function(){
},onSet:function(_25,_26,_27,_28){
},onNew:function(_29,_2a){
},onStoreClose:function(_2b){
},getParentView:function(_2c){
var w;
for(w=_a.getEnclosingWidget(_2c);w;w=w.getParent()){
if(w.getParent() instanceof _11){
return w;
}
}
return null;
},getIndexByItemWidget:function(w){
if(!w){
return -1;
}
var _2d=w.getParent();
return _1.indexOf(this.getChildren(),_2d)*this.numVisible+_1.indexOf(_2d.getChildren(),w);
},getItemWidgetByIndex:function(_2e){
if(_2e===-1){
return null;
}
var _2f=this.getChildren()[Math.floor(_2e/this.numVisible)];
return _2f.getChildren()[_2e%this.numVisible];
},onPrevBtnClick:function(){
if(this.currentView){
this.currentView.goTo(-1);
}
},onNextBtnClick:function(){
if(this.currentView){
this.currentView.goTo(1);
}
},_onClick:function(e){
if(this.onClick(e)===false){
return;
}
if(e&&e.type==="keydown"){
if(e.keyCode===39){
this.onNextBtnClick();
}else{
if(e.keyCode===37){
this.onPrevBtnClick();
}else{
if(e.keyCode!==13){
return;
}
}
}
}
var w;
for(w=_a.getEnclosingWidget(e.target);;w=w.getParent()){
if(!w){
return;
}
if(w.getParent() instanceof _11){
break;
}
}
this.select(w);
var idx=this.getIndexByItemWidget(w);
_2.publish("/dojox/mobile/carouselSelect",[this,w,this.items[idx],idx]);
},select:function(_30){
if(typeof (_30)==="number"){
_30=this.getItemWidgetByIndex(_30);
}
if(this.selectable){
if(this.selectedItem){
this.selectedItem.set("selected",false);
_7.remove(this.selectedItem.domNode,"mblCarouselSlotSelected");
}
if(_30){
_30.set("selected",true);
_7.add(_30.domNode,"mblCarouselSlotSelected");
}
this.selectedItem=_30;
}
},onClick:function(){
},instantiateView:function(_31){
if(_31&&!_31._instantiated){
var _32=(_9.get(_31.domNode,"display")==="none");
if(_32){
_9.set(_31.domNode,{visibility:"hidden",display:""});
}
_e.instantiateLazyWidgets(_31.containerNode,null,function(_33){
if(_32){
_9.set(_31.domNode,{visibility:"visible",display:"none"});
}
});
_31._instantiated=true;
}
},handleViewChanged:function(_34){
if(_34.getParent()!==this){
return;
}
if(this.currentView.nextView(this.currentView.domNode)===_34){
this.instantiateView(_34.nextView(_34.domNode));
}else{
this.instantiateView(_34.previousView(_34.domNode));
}
this.currentView=_34;
},_setTitleAttr:function(_35){
this.titleNode.innerHTML=this._cv?this._cv(_35):_35;
this._set("title",_35);
}});
_15.ChildSwapViewProperties={lazy:false};
_5.extend(_11,_15.ChildSwapViewProperties);
return _6("dojo-bidi")?_3("dojox.mobile.Carousel",[_15,_13]):_15;
});
