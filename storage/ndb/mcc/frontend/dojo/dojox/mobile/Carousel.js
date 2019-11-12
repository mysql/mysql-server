//>>built
define("dojox/mobile/Carousel",["dojo/_base/array","dojo/_base/connect","dojo/_base/declare","dojo/_base/event","dojo/_base/sniff","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./lazyLoadUtils","./CarouselItem","./PageIndicator","./SwapView","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11){
return _3("dojox.mobile.Carousel",[_c,_b,_a],{numVisible:2,itemWidth:0,title:"",pageIndicator:true,navButton:false,height:"",selectable:true,baseClass:"mblCarousel",buildRendering:function(){
this.containerNode=_7.create("div",{className:"mblCarouselPages"});
this.inherited(arguments);
if(this.srcNodeRef){
for(var i=0,len=this.srcNodeRef.childNodes.length;i<len;i++){
this.containerNode.appendChild(this.srcNodeRef.firstChild);
}
}
this.headerNode=_7.create("div",{className:"mblCarouselHeaderBar"},this.domNode);
if(this.navButton){
this.btnContainerNode=_7.create("div",{className:"mblCarouselBtnContainer"},this.headerNode);
_8.set(this.btnContainerNode,"float","right");
this.prevBtnNode=_7.create("button",{className:"mblCarouselBtn",title:"Previous",innerHTML:"&lt;"},this.btnContainerNode);
this.nextBtnNode=_7.create("button",{className:"mblCarouselBtn",title:"Next",innerHTML:"&gt;"},this.btnContainerNode);
this._prevHandle=this.connect(this.prevBtnNode,"onclick","onPrevBtnClick");
this._nextHandle=this.connect(this.nextBtnNode,"onclick","onNextBtnClick");
}
if(this.pageIndicator){
if(!this.title){
this.title="&nbsp;";
}
this.piw=new _f();
_8.set(this.piw,"float","right");
this.headerNode.appendChild(this.piw.domNode);
}
this.titleNode=_7.create("div",{className:"mblCarouselTitle"},this.headerNode);
this.domNode.appendChild(this.containerNode);
this.subscribe("/dojox/mobile/viewChanged","handleViewChanged");
this._clickHandle=this.connect(this.domNode,"onclick","_onClick");
this._keydownHandle=this.connect(this.domNode,"onkeydown","_onClick");
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
var _12=this.store;
this.store=null;
this.setStore(_12,this.query,this.queryOptions);
}else{
this.resizeItems();
}
this.inherited(arguments);
this.currentView=_1.filter(this.getChildren(),function(_13){
return _13.isVisible();
})[0];
},resizeItems:function(){
var idx=0;
var h=this.domNode.offsetHeight-(this.headerNode?this.headerNode.offsetHeight:0);
var m=_5("ie")?5/this.numVisible-1:5/this.numVisible;
_1.forEach(this.getChildren(),function(_14){
if(!(_14 instanceof _10)){
return;
}
if(!(_14.lazy||_14.domNode.getAttribute("lazy"))){
_14._instantiated=true;
}
var ch=_14.containerNode.childNodes;
for(var i=0,len=ch.length;i<len;i++){
var _15=ch[i];
if(_15.nodeType!==1){
continue;
}
var _16=this.items[idx]||{};
_8.set(_15,{width:_16.width||(90/this.numVisible+"%"),height:_16.height||h+"px",margin:"0 "+(_16.margin||m+"%")});
_6.add(_15,"mblCarouselSlot");
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
_1.forEach(this.getChildren(),function(_17,i){
var s="";
for(var j=0;j<this.numVisible;j++){
var _18,_19="",_1a;
var idx=i*this.numVisible+j;
var _1b={};
if(idx<this.items.length){
_1b=this.items[idx];
_18=this.store.getValue(_1b,"type");
if(_18){
_19=this.store.getValue(_1b,"props");
_1a=this.store.getValue(_1b,"mixins");
}else{
_18="dojox.mobile.CarouselItem";
_1.forEach(["alt","src","headerText","footerText"],function(p){
var v=this.store.getValue(_1b,p);
if(v!==undefined){
if(_19){
_19+=",";
}
_19+=p+":\""+v+"\"";
}
},this);
}
}else{
_18="dojox.mobile.CarouselItem";
_19="src:\""+_11.toUrl("dojo/resources/blank.gif")+"\""+", className:\"mblCarouselItemBlank\"";
}
s+="<div data-dojo-type=\""+_18+"\"";
if(_19){
s+=" data-dojo-props='"+_19+"'";
}
if(_1a){
s+=" data-dojo-mixins='"+_1a+"'";
}
s+="></div>";
}
_17.containerNode.innerHTML=s;
},this);
},onComplete:function(_1c){
_1.forEach(this.getChildren(),function(_1d){
if(_1d instanceof _10){
_1d.destroyRecursive();
}
});
this.selectedItem=null;
this.items=_1c;
var _1e=Math.ceil(_1c.length/this.numVisible),i,h=this.domNode.offsetHeight-this.headerNode.offsetHeight,idx=this.selectedItemIndex===-1?0:this.selectedItemIndex;
pg=Math.floor(idx/this.numVisible);
for(i=0;i<_1e;i++){
var w=new _10({height:h+"px",lazy:true});
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
var _1f=this.getChildren();
var _20=pg-1<0?0:pg-1;
var to=pg+1>_1e-1?_1e-1:pg+1;
for(i=_20;i<=to;i++){
this.instantiateView(_1f[i]);
}
},onError:function(){
},onUpdate:function(){
},onDelete:function(){
},onSet:function(_21,_22,_23,_24){
},onNew:function(_25,_26){
},onStoreClose:function(_27){
},getParentView:function(_28){
for(var w=_9.getEnclosingWidget(_28);w;w=w.getParent()){
if(w.getParent() instanceof _10){
return w;
}
}
return null;
},getIndexByItemWidget:function(w){
if(!w){
return -1;
}
var _29=w.getParent();
return _1.indexOf(this.getChildren(),_29)*this.numVisible+_1.indexOf(_29.getChildren(),w);
},getItemWidgetByIndex:function(_2a){
if(_2a===-1){
return null;
}
var _2b=this.getChildren()[Math.floor(_2a/this.numVisible)];
return _2b.getChildren()[_2a%this.numVisible];
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
for(w=_9.getEnclosingWidget(e.target);;w=w.getParent()){
if(!w){
return;
}
if(w.getParent() instanceof _10){
break;
}
}
this.select(w);
var idx=this.getIndexByItemWidget(w);
_2.publish("/dojox/mobile/carouselSelect",[this,w,this.items[idx],idx]);
},select:function(_2c){
if(typeof (_2c)==="number"){
_2c=this.getItemWidgetByIndex(_2c);
}
if(this.selectable){
if(this.selectedItem){
this.selectedItem.set("selected",false);
_6.remove(this.selectedItem.domNode,"mblCarouselSlotSelected");
}
if(_2c){
_2c.set("selected",true);
_6.add(_2c.domNode,"mblCarouselSlotSelected");
}
this.selectedItem=_2c;
}
},onClick:function(){
},instantiateView:function(_2d){
if(_2d&&!_2d._instantiated){
var _2e=(_8.get(_2d.domNode,"display")==="none");
if(_2e){
_8.set(_2d.domNode,{visibility:"hidden",display:""});
}
_d.instantiateLazyWidgets(_2d.containerNode,null,function(_2f){
if(_2e){
_8.set(_2d.domNode,{visibility:"visible",display:"none"});
}
});
_2d._instantiated=true;
}
},handleViewChanged:function(_30){
if(_30.getParent()!==this){
return;
}
if(this.currentView.nextView(this.currentView.domNode)===_30){
this.instantiateView(_30.nextView(_30.domNode));
}else{
this.instantiateView(_30.previousView(_30.domNode));
}
this.currentView=_30;
},_setTitleAttr:function(_31){
this.titleNode.innerHTML=this._cv?this._cv(_31):_31;
this._set("title",_31);
}});
});
