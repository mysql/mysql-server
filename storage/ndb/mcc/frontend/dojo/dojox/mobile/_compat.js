//>>built
define("dojox/mobile/_compat",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/fx","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/fx","dojo/fx/easing","dojo/ready","dojo/uacss","dijit/registry","dojox/fx","dojox/fx/flip","./EdgeToEdgeList","./IconContainer","./RoundRect","./RoundRectList","./ScrollableView","./Switch","./View","require"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,fx,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16,_17,_18){
var dm=_5.getObject("dojox.mobile",true);
if(!_6("webkit")){
_5.extend(_17,{_doTransition:function(_19,_1a,_1b,dir){
var _1c;
this.wakeUp(_1a);
if(!_1b||_1b=="none"){
_1a.style.display="";
_19.style.display="none";
_1a.style.left="0px";
this.invokeCallback();
}else{
if(_1b=="slide"||_1b=="cover"||_1b=="reveal"){
var w=_19.offsetWidth;
var s1=fx.slideTo({node:_19,duration:400,left:-w*dir,top:_a.get(_19,"top")});
var s2=fx.slideTo({node:_1a,duration:400,left:0,top:_a.get(_1a,"top")});
_1a.style.position="absolute";
_1a.style.left=w*dir+"px";
_1a.style.display="";
_1c=fx.combine([s1,s2]);
_3.connect(_1c,"onEnd",this,function(){
_19.style.display="none";
_19.style.left="0px";
_1a.style.position="relative";
var _1d=_e.byNode(_1a);
if(_1d&&!_8.contains(_1d.domNode,"out")){
_1d.containerNode.style.paddingTop="";
}
this.invokeCallback();
});
_1c.play();
}else{
if(_1b=="slidev"||_1b=="coverv"||_1b=="reavealv"){
var h=_19.offsetHeight;
var s1=fx.slideTo({node:_19,duration:400,left:0,top:-h*dir});
var s2=fx.slideTo({node:_1a,duration:400,left:0,top:0});
_1a.style.position="absolute";
_1a.style.top=h*dir+"px";
_1a.style.left="0px";
_1a.style.display="";
_1c=fx.combine([s1,s2]);
_3.connect(_1c,"onEnd",this,function(){
_19.style.display="none";
_1a.style.position="relative";
this.invokeCallback();
});
_1c.play();
}else{
if(_1b=="flip"){
_1c=_f.flip({node:_19,dir:"right",depth:0.5,duration:400});
_1a.style.position="absolute";
_1a.style.left="0px";
_3.connect(_1c,"onEnd",this,function(){
_19.style.display="none";
_1a.style.position="relative";
_1a.style.display="";
this.invokeCallback();
});
_1c.play();
}else{
_1c=fx.chain([_4.fadeOut({node:_19,duration:600}),_4.fadeIn({node:_1a,duration:600})]);
_1a.style.position="absolute";
_1a.style.left="0px";
_1a.style.display="";
_a.set(_1a,"opacity",0);
_3.connect(_1c,"onEnd",this,function(){
_19.style.display="none";
_1a.style.position="relative";
_a.set(_19,"opacity",1);
this.invokeCallback();
});
_1c.play();
}
}
}
}
dm.currentView=_e.byNode(_1a);
},wakeUp:function(_1e){
if(_6("ie")&&!_1e._wokeup){
_1e._wokeup=true;
var _1f=_1e.style.display;
_1e.style.display="";
var _20=_1e.getElementsByTagName("*");
for(var i=0,len=_20.length;i<len;i++){
var val=_20[i].style.display;
_20[i].style.display="none";
_20[i].style.display="";
_20[i].style.display=val;
}
_1e.style.display=_1f;
}
}});
_5.extend(_16,{_changeState:function(_21,_22){
var on=(_21==="on");
var pos;
if(!on){
pos=-this.inner.firstChild.firstChild.offsetWidth;
}else{
pos=0;
}
this.left.style.display="";
this.right.style.display="";
var _23=this;
var f=function(){
_8.remove(_23.domNode,on?"mblSwitchOff":"mblSwitchOn");
_8.add(_23.domNode,on?"mblSwitchOn":"mblSwitchOff");
_23.left.style.display=on?"":"none";
_23.right.style.display=!on?"":"none";
};
if(_22){
var a=fx.slideTo({node:this.inner,duration:300,left:pos,onEnd:f});
a.play();
}else{
if(on||pos){
this.inner.style.left=pos+"px";
}
f();
}
}});
if(_6("ie")){
_5.extend(_13,{buildRendering:function(){
dm.createRoundRect(this);
this.domNode.className="mblRoundRect";
}});
_14._addChild=_14.prototype.addChild;
_5.extend(_14,{buildRendering:function(){
dm.createRoundRect(this,true);
this.domNode.className="mblRoundRectList";
},postCreate:function(){
this.redrawBorders();
},addChild:function(_24,_25){
_14._addChild.apply(this,arguments);
this.redrawBorders();
if(dm.applyPngFilter){
dm.applyPngFilter(_24.domNode);
}
},redrawBorders:function(){
if(this instanceof _11){
return;
}
var _26=false;
for(var i=this.containerNode.childNodes.length-1;i>=0;i--){
var c=this.containerNode.childNodes[i];
if(c.tagName=="LI"){
c.style.borderBottomStyle=_26?"solid":"none";
_26=true;
}
}
}});
_5.extend(_11,{buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_7.doc.createElement("UL");
this.domNode.className="mblEdgeToEdgeList";
}});
_12._addChild=_12.prototype.addChild;
_5.extend(_12,{addChild:function(_27,_28){
_12._addChild.apply(this,arguments);
if(dm.applyPngFilter){
dm.applyPngFilter(_27.domNode);
}
}});
_5.mixin(dm,{createRoundRect:function(_29,_2a){
var i,len;
_29.domNode=_7.doc.createElement("DIV");
_29.domNode.style.padding="0px";
_29.domNode.style.backgroundColor="transparent";
_29.domNode.style.border="none";
_29.containerNode=_7.doc.createElement(_2a?"UL":"DIV");
_29.containerNode.className="mblRoundRectContainer";
if(_29.srcNodeRef){
_29.srcNodeRef.parentNode.replaceChild(_29.domNode,_29.srcNodeRef);
for(i=0,len=_29.srcNodeRef.childNodes.length;i<len;i++){
_29.containerNode.appendChild(_29.srcNodeRef.removeChild(_29.srcNodeRef.firstChild));
}
_29.srcNodeRef=null;
}
_29.domNode.appendChild(_29.containerNode);
for(i=0;i<=5;i++){
var top=_9.create("DIV");
top.className="mblRoundCorner mblRoundCorner"+i+"T";
_29.domNode.insertBefore(top,_29.containerNode);
var _2b=_9.create("DIV");
_2b.className="mblRoundCorner mblRoundCorner"+i+"B";
_29.domNode.appendChild(_2b);
}
}});
_5.extend(_15,{postCreate:function(){
var _2c=_9.create("DIV",{className:"mblDummyForIE",innerHTML:"&nbsp;"},this.containerNode,"first");
_a.set(_2c,{position:"relative",marginBottom:"-2px",fontSize:"1px"});
}});
}
if(_6("ie")<=6){
dm.applyPngFilter=function(_2d){
_2d=_2d||_7.body();
var _2e=_2d.getElementsByTagName("IMG");
var _2f=_18.toUrl("dojo/resources/blank.gif");
for(var i=0,len=_2e.length;i<len;i++){
var img=_2e[i];
var w=img.offsetWidth;
var h=img.offsetHeight;
if(w===0||h===0){
if(_a.get(img,"display")!="none"){
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
img.src=_2f;
img.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"')";
img.style.width=w+"px";
img.style.height=h+"px";
}
};
if(!dm._disableBgFilter&&dm.createDomButton){
dm._createDomButton_orig=dm.createDomButton;
dm.createDomButton=function(_30,_31,_32){
var _33=dm._createDomButton_orig.apply(this,arguments);
if(_33&&_33.className&&_33.className.indexOf("mblDomButton")!==-1){
var f=function(){
if(_33.currentStyle&&_33.currentStyle.backgroundImage.match(/url.*(mblDomButton.*\.png)/)){
var img=RegExp.$1;
var src=_18.toUrl("dojox/mobile/themes/common/domButtons/compat/")+img;
_33.runtimeStyle.filter="progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+src+"',sizingMethod='crop')";
_33.style.background="none";
}
};
setTimeout(f,1000);
setTimeout(f,5000);
}
return _33;
};
}
}
dm.loadCssFile=function(_34){
if(!dm.loadedCssFiles){
dm.loadedCssFiles=[];
}
if(_7.doc.createStyleSheet){
setTimeout(function(_35){
return function(){
var ss=_7.doc.createStyleSheet(_35);
ss&&dm.loadedCssFiles.push(ss.owningElement);
};
}(_34),0);
}else{
dm.loadedCssFiles.push(_9.create("LINK",{href:_34,type:"text/css",rel:"stylesheet"},_7.doc.getElementsByTagName("head")[0]));
}
};
dm.loadCss=function(_36){
if(!dm._loadedCss){
var obj={};
_1.forEach(dm.getCssPaths(),function(_37){
obj[_37]=true;
});
dm._loadedCss=obj;
}
if(!_5.isArray(_36)){
_36=[_36];
}
for(var i=0;i<_36.length;i++){
var _38=_36[i];
if(!dm._loadedCss[_38]){
dm._loadedCss[_38]=true;
dm.loadCssFile(_38);
}
}
};
dm.getCssPaths=function(){
var _39=[];
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
_39.push(r[j].href);
}
}
}
var _3a=_7.doc.getElementsByTagName("link");
for(i=0,len=_3a.length;i<len;i++){
if(_3a[i].href){
_39.push(_3a[i].href);
}
}
return _39;
};
dm.loadCompatPattern=/\/mobile\/themes\/.*\.css$/;
dm.loadCompatCssFiles=function(_3b){
if(_6("ie")&&!_3b){
setTimeout(function(){
dm.loadCompatCssFiles(true);
},0);
}
dm._loadedCss=undefined;
var _3c=dm.getCssPaths();
for(var i=0;i<_3c.length;i++){
var _3d=_3c[i];
if((_3d.match(dm.loadCompatPattern)||location.href.indexOf("mobile/tests/")!==-1)&&_3d.indexOf("-compat.css")===-1){
var _3e=_3d.substring(0,_3d.length-4)+"-compat.css";
dm.loadCss(_3e);
}
}
};
dm.hideAddressBar=function(evt,_3f){
if(_3f!==false){
dm.resizeAll();
}
};
_c(function(){
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
