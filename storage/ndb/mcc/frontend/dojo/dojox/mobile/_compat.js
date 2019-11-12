//>>built
define("dojox/mobile/_compat",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/fx","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/fx","dojo/fx/easing","dojo/ready","dojo/uacss","dijit/registry","dojox/fx","dojox/fx/flip","./EdgeToEdgeList","./IconContainer","./ProgressIndicator","./RoundRect","./RoundRectList","./ScrollableView","./Switch","./View","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,fx,_c,_d,_e,_f,xfx,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19){
var dm=_5.getObject("dojox.mobile",true);
if(!_6("webkit")){
_5.extend(_18,{_doTransition:function(_1a,_1b,_1c,dir){
var _1d;
this.wakeUp(_1b);
var s1,s2;
if(!_1c||_1c=="none"){
_1b.style.display="";
_1a.style.display="none";
_1b.style.left="0px";
this.invokeCallback();
}else{
if(_1c=="slide"||_1c=="cover"||_1c=="reveal"){
var w=_1a.offsetWidth;
s1=fx.slideTo({node:_1a,duration:400,left:-w*dir,top:_b.get(_1a,"top")});
s2=fx.slideTo({node:_1b,duration:400,left:0,top:_b.get(_1b,"top")});
_1b.style.position="absolute";
_1b.style.left=w*dir+"px";
_1b.style.display="";
_1d=fx.combine([s1,s2]);
_3.connect(_1d,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1a.style.display="none";
_1a.style.left="0px";
_1b.style.position="relative";
var _1e=_f.byNode(_1b);
if(_1e&&!_8.contains(_1e.domNode,"out")){
_1e.containerNode.style.paddingTop="";
}
this.invokeCallback();
});
_1d.play();
}else{
if(_1c=="slidev"||_1c=="coverv"||_1c=="reavealv"){
var h=_1a.offsetHeight;
s1=fx.slideTo({node:_1a,duration:400,left:0,top:-h*dir});
s2=fx.slideTo({node:_1b,duration:400,left:0,top:0});
_1b.style.position="absolute";
_1b.style.top=h*dir+"px";
_1b.style.left="0px";
_1b.style.display="";
_1d=fx.combine([s1,s2]);
_3.connect(_1d,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1a.style.display="none";
_1b.style.position="relative";
this.invokeCallback();
});
_1d.play();
}else{
if(_1c=="flip"){
_1d=xfx.flip({node:_1a,dir:"right",depth:0.5,duration:400});
_1b.style.position="absolute";
_1b.style.left="0px";
_3.connect(_1d,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1a.style.display="none";
_1b.style.position="relative";
_1b.style.display="";
this.invokeCallback();
});
_1d.play();
}else{
_1d=fx.chain([_4.fadeOut({node:_1a,duration:600}),_4.fadeIn({node:_1b,duration:600})]);
_1b.style.position="absolute";
_1b.style.left="0px";
_1b.style.display="";
_b.set(_1b,"opacity",0);
_3.connect(_1d,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1a.style.display="none";
_1b.style.position="relative";
_b.set(_1a,"opacity",1);
this.invokeCallback();
});
_1d.play();
}
}
}
}
},wakeUp:function(_1f){
if(_6("ie")&&!_1f._wokeup){
_1f._wokeup=true;
var _20=_1f.style.display;
_1f.style.display="";
var _21=_1f.getElementsByTagName("*");
for(var i=0,len=_21.length;i<len;i++){
var val=_21[i].style.display;
_21[i].style.display="none";
_21[i].style.display="";
_21[i].style.display=val;
}
_1f.style.display=_20;
}
}});
_5.extend(_17,{_changeState:function(_22,_23){
var on=(_22==="on");
var pos;
if(!on){
pos=-this.inner.firstChild.firstChild.offsetWidth;
}else{
pos=0;
}
this.left.style.display="";
this.right.style.display="";
var _24=this;
var f=function(){
_8.remove(_24.domNode,on?"mblSwitchOff":"mblSwitchOn");
_8.add(_24.domNode,on?"mblSwitchOn":"mblSwitchOff");
_24.left.style.display=on?"":"none";
_24.right.style.display=!on?"":"none";
};
if(_23){
var a=fx.slideTo({node:this.inner,duration:300,left:pos,onEnd:f});
a.play();
}else{
if(on||pos){
this.inner.style.left=pos+"px";
}
f();
}
}});
_5.extend(_13,{scale:function(_25){
if(_6("ie")){
var dim={w:_25,h:_25};
_a.setMarginBox(this.domNode,dim);
_a.setMarginBox(this.containerNode,dim);
}else{
if(_6("ff")){
var _26=_25/40;
_b.set(this.containerNode,{MozTransform:"scale("+_26+")",MozTransformOrigin:"0 0"});
_a.setMarginBox(this.domNode,{w:_25,h:_25});
_a.setMarginBox(this.containerNode,{w:_25/_26,h:_25/_26});
}
}
}});
if(_6("ie")){
_5.extend(_14,{buildRendering:function(){
dm.createRoundRect(this);
this.domNode.className="mblRoundRect";
}});
_15._addChild=_15.prototype.addChild;
_15._postCreate=_15.prototype.postCreate;
_5.extend(_15,{buildRendering:function(){
dm.createRoundRect(this,true);
this.domNode.className="mblRoundRectList";
},postCreate:function(){
_15._postCreate.apply(this,arguments);
this.redrawBorders();
},addChild:function(_27,_28){
_15._addChild.apply(this,arguments);
this.redrawBorders();
if(dm.applyPngFilter){
dm.applyPngFilter(_27.domNode);
}
},redrawBorders:function(){
if(this instanceof _11){
return;
}
var _29=false;
for(var i=this.containerNode.childNodes.length-1;i>=0;i--){
var c=this.containerNode.childNodes[i];
if(c.tagName=="LI"){
c.style.borderBottomStyle=_29?"solid":"none";
_29=true;
}
}
}});
_5.extend(_11,{buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_7.doc.createElement("ul");
this.domNode.className="mblEdgeToEdgeList";
}});
_12._addChild=_12.prototype.addChild;
_5.extend(_12,{addChild:function(_2a,_2b){
_12._addChild.apply(this,arguments);
if(dm.applyPngFilter){
dm.applyPngFilter(_2a.domNode);
}
}});
_5.mixin(dm,{createRoundRect:function(_2c,_2d){
var i,len;
_2c.domNode=_7.doc.createElement("div");
_2c.domNode.style.padding="0px";
_2c.domNode.style.backgroundColor="transparent";
_2c.domNode.style.border="none";
_2c.containerNode=_7.doc.createElement(_2d?"ul":"div");
_2c.containerNode.className="mblRoundRectContainer";
if(_2c.srcNodeRef){
_2c.srcNodeRef.parentNode.replaceChild(_2c.domNode,_2c.srcNodeRef);
for(i=0,len=_2c.srcNodeRef.childNodes.length;i<len;i++){
_2c.containerNode.appendChild(_2c.srcNodeRef.removeChild(_2c.srcNodeRef.firstChild));
}
_2c.srcNodeRef=null;
}
_2c.domNode.appendChild(_2c.containerNode);
for(i=0;i<=5;i++){
var top=_9.create("div");
top.className="mblRoundCorner mblRoundCorner"+i+"T";
_2c.domNode.insertBefore(top,_2c.containerNode);
var _2e=_9.create("div");
_2e.className="mblRoundCorner mblRoundCorner"+i+"B";
_2c.domNode.appendChild(_2e);
}
}});
_5.extend(_16,{postCreate:function(){
var _2f=_9.create("div",{className:"mblDummyForIE",innerHTML:"&nbsp;"},this.containerNode,"first");
_b.set(_2f,{position:"relative",marginBottom:"-2px",fontSize:"1px"});
}});
}
if(_6("ie")<=6){
dm.applyPngFilter=function(_30){
_30=_30||_7.body();
var _31=_30.getElementsByTagName("IMG");
var _32=_19.toUrl("dojo/resources/blank.gif");
for(var i=0,len=_31.length;i<len;i++){
var img=_31[i];
var w=img.offsetWidth;
var h=img.offsetHeight;
if(w===0||h===0){
if(_b.get(img,"display")!="none"){
continue;
}
img.style.display="";
w=img.offsetWidth;
h=img.offsetHeight;
img.style.display="none";
if(w===0||h===0){
continue;
}
}
var src=img.src;
if(src.indexOf("resources/blank.gif")!=-1){
continue;
}
img.src=_32;
img.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"')";
img.style.width=w+"px";
img.style.height=h+"px";
}
};
if(!dm._disableBgFilter&&dm.createDomButton){
dm._createDomButton_orig=dm.createDomButton;
dm.createDomButton=function(_33,_34,_35){
var _36=dm._createDomButton_orig.apply(this,arguments);
if(_36&&_36.className&&_36.className.indexOf("mblDomButton")!==-1){
var f=function(){
if(_36.currentStyle&&_36.currentStyle.backgroundImage.match(/url.*(mblDomButton.*\.png)/)){
var img=RegExp.$1;
var src=_19.toUrl("dojox/mobile/themes/common/domButtons/compat/")+img;
_36.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"',sizingMethod='crop')";
_36.style.background="none";
}
};
setTimeout(f,1000);
setTimeout(f,5000);
}
return _36;
};
}
}
dm.loadCssFile=function(_37){
if(!dm.loadedCssFiles){
dm.loadedCssFiles=[];
}
if(_7.doc.createStyleSheet){
setTimeout(function(_38){
return function(){
var ss=_7.doc.createStyleSheet(_38);
ss&&dm.loadedCssFiles.push(ss.owningElement);
};
}(_37),0);
}else{
dm.loadedCssFiles.push(_9.create("link",{href:_37,type:"text/css",rel:"stylesheet"},_7.doc.getElementsByTagName("head")[0]));
}
};
dm.loadCss=function(_39){
if(!dm._loadedCss){
var obj={};
_1.forEach(dm.getCssPaths(),function(_3a){
obj[_3a]=true;
});
dm._loadedCss=obj;
}
if(!_5.isArray(_39)){
_39=[_39];
}
for(var i=0;i<_39.length;i++){
var _3b=_39[i];
if(!dm._loadedCss[_3b]){
dm._loadedCss[_3b]=true;
dm.loadCssFile(_3b);
}
}
};
dm.getCssPaths=function(){
var _3c=[];
var i,j,len;
var s=_7.doc.styleSheets;
for(i=0;i<s.length;i++){
if(s[i].href){
continue;
}
var r=s[i].cssRules||s[i].imports;
if(!r){
continue;
}
for(j=0;j<r.length;j++){
if(r[j].href){
_3c.push(r[j].href);
}
}
}
var _3d=_7.doc.getElementsByTagName("link");
for(i=0,len=_3d.length;i<len;i++){
if(_3d[i].href){
_3c.push(_3d[i].href);
}
}
return _3c;
};
dm.loadCompatPattern=/\/mobile\/themes\/.*\.css$/;
dm.loadCompatCssFiles=function(_3e){
if(_6("ie")&&!_3e){
setTimeout(function(){
dm.loadCompatCssFiles(true);
},0);
return;
}
dm._loadedCss=undefined;
var _3f=dm.getCssPaths();
for(var i=0;i<_3f.length;i++){
var _40=_3f[i];
if((_40.match(_2.mblLoadCompatPattern||dm.loadCompatPattern)||location.href.indexOf("mobile/tests/")!==-1)&&_40.indexOf("-compat.css")===-1){
var _41=_40.substring(0,_40.length-4)+"-compat.css";
dm.loadCss(_41);
}
}
};
dm.hideAddressBar=function(evt,_42){
if(_42!==false){
dm.resizeAll();
}
};
_d(function(){
if(_2["mblLoadCompatCssFiles"]!==false){
dm.loadCompatCssFiles();
}
if(dm.applyPngFilter){
dm.applyPngFilter();
}
});
}
return dm;
});
