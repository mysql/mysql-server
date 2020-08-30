//>>built
define("dojox/mobile/_compat",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/fx","dojo/_base/lang","dojo/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dojo/dom-attr","dojo/fx","dojo/fx/easing","dojo/ready","dojo/uacss","dijit/registry","dojox/fx","dojox/fx/flip","./EdgeToEdgeList","./IconContainer","./ProgressIndicator","./RoundRect","./RoundRectList","./ScrollableView","./Switch","./View","./Heading","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,fx,_d,_e,_f,_10,xfx,_11,_12,_13,_14,_15,_16,_17,_18,_19,_1a,_1b){
var dm=_5.getObject("dojox.mobile",true);
if(!(_6("webkit")||_6("ie")===10||(!_6("ie")&&_6("trident")>6))){
_5.extend(_19,{_doTransition:function(_1c,_1d,_1e,dir){
var _1f;
this.wakeUp(_1d);
var s1,s2;
if(!_1e||_1e=="none"){
_1d.style.display="";
_1c.style.display="none";
_1d.style.left="0px";
this.invokeCallback();
}else{
if(_1e=="slide"||_1e=="cover"||_1e=="reveal"){
var w=_1c.offsetWidth;
s1=fx.slideTo({node:_1c,duration:400,left:-w*dir,top:_b.get(_1c,"top")});
s2=fx.slideTo({node:_1d,duration:400,left:0,top:_b.get(_1d,"top")});
_1d.style.position="absolute";
_1d.style.left=w*dir+"px";
_1d.style.display="";
_1f=fx.combine([s1,s2]);
_3.connect(_1f,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1c.style.display="none";
_1c.style.left="0px";
_1d.style.position="relative";
var _20=_10.byNode(_1d);
if(_20&&!_8.contains(_20.domNode,"out")){
_20.containerNode.style.paddingTop="";
}
this.invokeCallback();
});
_1f.play();
}else{
if(_1e=="slidev"||_1e=="coverv"||_1e=="reavealv"){
var h=_1c.offsetHeight;
s1=fx.slideTo({node:_1c,duration:400,left:0,top:-h*dir});
s2=fx.slideTo({node:_1d,duration:400,left:0,top:0});
_1d.style.position="absolute";
_1d.style.top=h*dir+"px";
_1d.style.left="0px";
_1d.style.display="";
_1f=fx.combine([s1,s2]);
_3.connect(_1f,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1c.style.display="none";
_1d.style.position="relative";
this.invokeCallback();
});
_1f.play();
}else{
if(_1e=="flip"){
_1f=xfx.flip({node:_1c,dir:"right",depth:0.5,duration:400});
_1d.style.position="absolute";
_1d.style.left="0px";
_3.connect(_1f,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1c.style.display="none";
_1d.style.position="relative";
_1d.style.display="";
this.invokeCallback();
});
_1f.play();
}else{
_1f=fx.chain([_4.fadeOut({node:_1c,duration:600}),_4.fadeIn({node:_1d,duration:600})]);
_1d.style.position="absolute";
_1d.style.left="0px";
_1d.style.display="";
_b.set(_1d,"opacity",0);
_3.connect(_1f,"onEnd",this,function(){
if(!this._inProgress){
return;
}
_1c.style.display="none";
_1d.style.position="relative";
_b.set(_1c,"opacity",1);
this.invokeCallback();
});
_1f.play();
}
}
}
}
},wakeUp:function(_21){
if(_6("ie")&&!_21._wokeup){
_21._wokeup=true;
var _22=_21.style.display;
_21.style.display="";
var _23=_21.getElementsByTagName("*");
for(var i=0,len=_23.length;i<len;i++){
var val=_23[i].style.display;
_23[i].style.display="none";
_23[i].style.display="";
_23[i].style.display=val;
}
_21.style.display=_22;
}
}});
_5.extend(_14,{scale:function(_24){
if(_6("ie")){
var dim={w:_24,h:_24};
_a.setMarginBox(this.domNode,dim);
_a.setMarginBox(this.containerNode,dim);
}else{
if(_6("ff")){
var _25=_24/40;
_b.set(this.containerNode,{MozTransform:"scale("+_25+")",MozTransformOrigin:"0 0"});
_a.setMarginBox(this.domNode,{w:_24,h:_24});
_a.setMarginBox(this.containerNode,{w:_24/_25,h:_24/_25});
}
}
}});
if(_6("ie")){
_5.extend(_15,{buildRendering:function(){
dm.createRoundRect(this);
this.domNode.className="mblRoundRect";
}});
_16._addChild=_16.prototype.addChild;
_16._postCreate=_16.prototype.postCreate;
_5.extend(_16,{buildRendering:function(){
dm.createRoundRect(this,true);
this.domNode.className="mblRoundRectList";
if(_6("ie")&&_6("dojo-bidi")&&!this.isLeftToRight()){
this.domNode.className="mblRoundRectList mblRoundRectListRtl";
}
},postCreate:function(){
_16._postCreate.apply(this,arguments);
this.redrawBorders();
},addChild:function(_26,_27){
_16._addChild.apply(this,arguments);
this.redrawBorders();
if(dm.applyPngFilter){
dm.applyPngFilter(_26.domNode);
}
},redrawBorders:function(){
if(this instanceof _12){
return;
}
var _28=false;
for(var i=this.containerNode.childNodes.length-1;i>=0;i--){
var c=this.containerNode.childNodes[i];
if(c.tagName=="LI"){
c.style.borderBottomStyle=_28?"solid":"none";
_28=true;
}
}
}});
_5.extend(_12,{buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_7.doc.createElement("ul");
this.domNode.className="mblEdgeToEdgeList";
}});
_13._addChild=_13.prototype.addChild;
_5.extend(_13,{addChild:function(_29,_2a){
_13._addChild.apply(this,arguments);
if(dm.applyPngFilter){
dm.applyPngFilter(_29.domNode);
}
}});
_5.mixin(dm,{createRoundRect:function(_2b,_2c){
var i,len;
_2b.domNode=_7.doc.createElement("div");
_2b.domNode.style.padding="0px";
_2b.domNode.style.backgroundColor="transparent";
_2b.domNode.style.border="none";
_2b.containerNode=_7.doc.createElement(_2c?"ul":"div");
_2b.containerNode.className="mblRoundRectContainer";
if(_2b.srcNodeRef){
_2b.srcNodeRef.parentNode.replaceChild(_2b.domNode,_2b.srcNodeRef);
for(i=0,len=_2b.srcNodeRef.childNodes.length;i<len;i++){
_2b.containerNode.appendChild(_2b.srcNodeRef.removeChild(_2b.srcNodeRef.firstChild));
}
_2b.srcNodeRef=null;
}
_2b.domNode.appendChild(_2b.containerNode);
for(i=0;i<=5;i++){
var top=_9.create("div");
top.className="mblRoundCorner mblRoundCorner"+i+"T";
_2b.domNode.insertBefore(top,_2b.containerNode);
var _2d=_9.create("div");
_2d.className="mblRoundCorner mblRoundCorner"+i+"B";
_2b.domNode.appendChild(_2d);
}
}});
_5.extend(_17,{postCreate:function(){
var _2e=_9.create("div",{className:"mblDummyForIE",innerHTML:"&nbsp;"},this.containerNode,"first");
_b.set(_2e,{position:"relative",marginBottom:"-2px",fontSize:"1px"});
}});
}
if(_6("ie")<=6){
dm.applyPngFilter=function(_2f){
_2f=_2f||_7.body();
var _30=_2f.getElementsByTagName("IMG");
var _31=_1b.toUrl("dojo/resources/blank.gif");
for(var i=0,len=_30.length;i<len;i++){
var img=_30[i];
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
img.src=_31;
img.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"')";
img.style.width=w+"px";
img.style.height=h+"px";
}
};
if(!dm._disableBgFilter&&dm.createDomButton){
dm._createDomButton_orig=dm.createDomButton;
dm.createDomButton=function(_32,_33,_34){
var _35=dm._createDomButton_orig.apply(this,arguments);
if(_35&&_35.className&&_35.className.indexOf("mblDomButton")!==-1){
var f=function(){
if(_35.currentStyle&&_35.currentStyle.backgroundImage.match(/url.*(mblDomButton.*\.png)/)){
var img=RegExp.$1;
var src=_1b.toUrl("dojox/mobile/themes/common/domButtons/compat/")+img;
_35.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"',sizingMethod='crop')";
_35.style.background="none";
}
};
setTimeout(f,1000);
setTimeout(f,5000);
}
return _35;
};
}
}
dm.loadCssFile=function(_36){
if(!dm.loadedCssFiles){
dm.loadedCssFiles=[];
}
if(_7.doc.createStyleSheet){
setTimeout(function(_37){
return function(){
var ss=_7.doc.createStyleSheet(_37);
ss&&dm.loadedCssFiles.push(ss.owningElement);
};
}(_36),0);
}else{
dm.loadedCssFiles.push(_9.create("link",{href:_36,type:"text/css",rel:"stylesheet"},_7.doc.getElementsByTagName("head")[0]));
}
};
dm.loadCss=function(_38){
if(!dm._loadedCss){
var obj={};
_1.forEach(dm.getCssPaths(),function(_39){
obj[_39]=true;
});
dm._loadedCss=obj;
}
if(!_5.isArray(_38)){
_38=[_38];
}
for(var i=0;i<_38.length;i++){
var _3a=_38[i];
if(!dm._loadedCss[_3a]){
dm._loadedCss[_3a]=true;
dm.loadCssFile(_3a);
}
}
};
dm.getCssPaths=function(){
var _3b=[];
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
_3b.push(r[j].href);
}
}
}
var _3c=_7.doc.getElementsByTagName("link");
for(i=0,len=_3c.length;i<len;i++){
if(_3c[i].href){
_3b.push(_3c[i].href);
}
}
return _3b;
};
dm.loadCompatPattern=/\/mobile\/themes\/.*\.css$/;
dm.loadCompatCssFiles=function(_3d){
if(_6("ie")&&!_3d){
setTimeout(function(){
dm.loadCompatCssFiles(true);
},0);
return;
}
dm._loadedCss=undefined;
var _3e=dm.getCssPaths();
if(_6("dojo-bidi")){
_3e=dm.loadRtlCssFiles(_3e);
}
for(var i=0;i<_3e.length;i++){
var _3f=_3e[i];
if((_3f.match(_2.mblLoadCompatPattern||dm.loadCompatPattern)||location.href.indexOf("mobile/tests/")!==-1)&&_3f.indexOf("-compat.css")===-1){
var _40=_3f.substring(0,_3f.length-4)+"-compat.css";
dm.loadCss(_40);
}
}
};
if(_6("dojo-bidi")){
dm.loadRtlCssFiles=function(_41){
for(var i=0;i<_41.length;i++){
var _42=_41[i];
if(_42.indexOf("_rtl")==-1){
var _43="android.css blackberry.css custom.css iphone.css holodark.css base.css Carousel.css ComboBox.css IconContainer.css IconMenu.css ListItem.css RoundRectCategory.css SpinWheel.css Switch.css TabBar.css ToggleButton.css ToolBarButton.css ProgressIndicator.css Accordion.css GridLayout.css FormLayout.css";
var _44=_42.substr(_42.lastIndexOf("/")+1);
if(_43.indexOf(_44)!=-1){
var _45=_42.replace(".css","_rtl.css");
_41.push(_45);
dm.loadCss(_45);
}
}
}
return _41;
};
}
dm.hideAddressBar=function(evt,_46){
if(_46!==false){
dm.resizeAll();
}
};
_e(function(){
if(_2.mblLoadCompatCssFiles!==false){
dm.loadCompatCssFiles();
}
if(dm.applyPngFilter){
dm.applyPngFilter();
}
});
}
return dm;
});
