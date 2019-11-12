//>>built
define("dojox/mobile/View",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/_base/Deferred","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./ViewController","./common","./transition","./viewRegistry"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15){
var dm=_5.getObject("dojox.mobile",true);
return _4("dojox.mobile.View",[_11,_10,_f],{selected:false,keepScrollPos:true,tag:"div",baseClass:"mblView",constructor:function(_16,_17){
if(_17){
_9.byId(_17).style.visibility="hidden";
}
},destroy:function(){
_15.remove(this.id);
this.inherited(arguments);
},buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_b.create(this.tag);
this._animEndHandle=this.connect(this.domNode,"webkitAnimationEnd","onAnimationEnd");
this._animStartHandle=this.connect(this.domNode,"webkitAnimationStart","onAnimationStart");
if(!_2["mblCSS3Transition"]){
this._transEndHandle=this.connect(this.domNode,"webkitTransitionEnd","onAnimationEnd");
}
if(_6("mblAndroid3Workaround")){
_d.set(this.domNode,"webkitTransformStyle","preserve-3d");
}
_15.add(this);
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
if(this._visible===undefined){
var _18=this.getSiblingViews();
var ids=location.hash&&location.hash.substring(1).split(/,/);
var _19,_1a,_1b;
_1.forEach(_18,function(v,i){
if(_1.indexOf(ids,v.id)!==-1){
_19=v;
}
if(i==0){
_1b=v;
}
if(v.selected){
_1a=v;
}
v._visible=false;
},this);
(_19||_1a||_1b)._visible=true;
}
if(this._visible){
this.show(true,true);
this.defer(function(){
this.onStartView();
_3.publish("/dojox/mobile/startView",[this]);
});
}
if(this.domNode.style.visibility!="visible"){
this.domNode.style.visibility="visible";
}
this.inherited(arguments);
var _1c=this.getParent();
if(!_1c||!_1c.resize){
this.resize();
}
if(!this._visible){
this.hide();
}
},resize:function(){
_1.forEach(this.getChildren(),function(_1d){
if(_1d.resize){
_1d.resize();
}
});
},onStartView:function(){
},onBeforeTransitionIn:function(_1e,dir,_1f,_20,_21){
},onAfterTransitionIn:function(_22,dir,_23,_24,_25){
},onBeforeTransitionOut:function(_26,dir,_27,_28,_29){
},onAfterTransitionOut:function(_2a,dir,_2b,_2c,_2d){
},_clearClasses:function(_2e){
if(!_2e){
return;
}
var _2f=[];
_1.forEach(_5.trim(_2e.className||"").split(/\s+/),function(c){
if(c.match(/^mbl\w*View$/)||c.indexOf("mbl")===-1){
_2f.push(c);
}
},this);
_2e.className=_2f.join(" ");
},_fixViewState:function(_30){
var _31=this.domNode.parentNode.childNodes;
for(var i=0;i<_31.length;i++){
var n=_31[i];
if(n.nodeType===1&&_a.contains(n,"mblView")){
this._clearClasses(n);
}
}
this._clearClasses(_30);
var _32=_e.byNode(_30);
if(_32){
_32._inProgress=false;
}
},convertToId:function(_33){
if(typeof (_33)=="string"){
return _33.replace(/^#?([^&?]+).*/,"$1");
}
return _33;
},_isBookmarkable:function(_34){
return _34.moveTo&&(_2["mblForceBookmarkable"]||_34.moveTo.charAt(0)==="#")&&!_34.hashchange;
},performTransition:function(_35,_36,_37,_38,_39){
if(this._inProgress){
return;
}
this._inProgress=true;
var _3a,_3b;
if(_35&&typeof (_35)==="object"){
_3a=_35;
_3b=_36;
}else{
_3a={moveTo:_35,transitionDir:_36,transition:_37,context:_38,method:_39};
_3b=[];
for(var i=5;i<arguments.length;i++){
_3b.push(arguments[i]);
}
}
this._detail=_3a;
this._optArgs=_3b;
this._arguments=[_3a.moveTo,_3a.transitionDir,_3a.transition,_3a.context,_3a.method];
if(_3a.moveTo==="#"){
return;
}
var _3c;
if(_3a.moveTo){
_3c=this.convertToId(_3a.moveTo);
}else{
if(!this._dummyNode){
this._dummyNode=_7.doc.createElement("div");
_7.body().appendChild(this._dummyNode);
}
_3c=this._dummyNode;
}
if(this.addTransitionInfo&&typeof (_3a.moveTo)=="string"&&this._isBookmarkable(_3a)){
this.addTransitionInfo(this.id,_3a.moveTo,{transitionDir:_3a.transitionDir,transition:_3a.transition});
}
var _3d=this.domNode;
var _3e=_3d.offsetTop;
_3c=this.toNode=_9.byId(_3c);
if(!_3c){
return;
}
_3c.style.visibility="hidden";
_3c.style.display="";
this._fixViewState(_3c);
var _3f=_e.byNode(_3c);
if(_3f){
if(_2["mblAlwaysResizeOnTransition"]||!_3f._resized){
_13.resizeAll(null,_3f);
_3f._resized=true;
}
if(_3a.transition&&_3a.transition!="none"){
_3f.containerNode.style.paddingTop=_3e+"px";
}
_3f.load&&_3f.load();
_3f.movedFrom=_3d.id;
}
if(_6("mblAndroidWorkaround")&&!_2["mblCSS3Transition"]&&_3a.transition&&_3a.transition!="none"){
_d.set(_3c,"webkitTransformStyle","preserve-3d");
_d.set(_3d,"webkitTransformStyle","preserve-3d");
_a.add(_3c,"mblAndroidWorkaround");
}
this.onBeforeTransitionOut.apply(this,this._arguments);
_3.publish("/dojox/mobile/beforeTransitionOut",[this].concat(_5._toArray(this._arguments)));
if(_3f){
if(this.keepScrollPos&&!this.getParent()){
var _40=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
_3d._scrollTop=_40;
var _41=(_3a.transitionDir==1)?0:(_3c._scrollTop||0);
_3c.style.top="0px";
if(_40>1||_41!==0){
_3d.style.top=_41-_40+"px";
if(_2["mblHideAddressBar"]!==false){
setTimeout(function(){
_7.global.scrollTo(0,(_41||1));
},0);
}
}
}else{
_3c.style.top="0px";
}
_3f.onBeforeTransitionIn.apply(_3f,this._arguments);
_3.publish("/dojox/mobile/beforeTransitionIn",[_3f].concat(_5._toArray(this._arguments)));
}
_3c.style.display="none";
_3c.style.visibility="visible";
_13.fromView=this;
_13.toView=_3f;
this._doTransition(_3d,_3c,_3a.transition,_3a.transitionDir);
},_toCls:function(s){
return "mbl"+s.charAt(0).toUpperCase()+s.substring(1);
},_doTransition:function(_42,_43,_44,_45){
var rev=(_45==-1)?" mblReverse":"";
_43.style.display="";
if(!_44||_44=="none"){
this.domNode.style.display="none";
this.invokeCallback();
}else{
if(_2["mblCSS3Transition"]){
_8.when(_14,_5.hitch(this,function(_46){
var _47=_d.get(_43,"position");
_d.set(_43,"position","absolute");
_8.when(_46(_42,_43,{transition:_44,reverse:(_45===-1)?true:false}),_5.hitch(this,function(){
_d.set(_43,"position",_47);
this.invokeCallback();
}));
}));
}else{
if(_44.indexOf("cube")!=-1){
if(_6("ipad")){
_d.set(_43.parentNode,{webkitPerspective:1600});
}else{
if(_6("iphone")){
_d.set(_43.parentNode,{webkitPerspective:800});
}
}
}
var s=this._toCls(_44);
if(_6("mblAndroidWorkaround")){
setTimeout(function(){
_a.add(_42,s+" mblOut"+rev);
_a.add(_43,s+" mblIn"+rev);
_a.remove(_43,"mblAndroidWorkaround");
setTimeout(function(){
_a.add(_42,"mblTransition");
_a.add(_43,"mblTransition");
},30);
},70);
}else{
_a.add(_42,s+" mblOut"+rev);
_a.add(_43,s+" mblIn"+rev);
setTimeout(function(){
_a.add(_42,"mblTransition");
_a.add(_43,"mblTransition");
},100);
}
var _48="50% 50%";
var _49="50% 50%";
var _4a,_4b,_4c;
if(_44.indexOf("swirl")!=-1||_44.indexOf("zoom")!=-1){
if(this.keepScrollPos&&!this.getParent()){
_4a=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
}else{
_4a=-_c.position(_42,true).y;
}
_4c=_7.global.innerHeight/2+_4a;
_48="50% "+_4c+"px";
_49="50% "+_4c+"px";
}else{
if(_44.indexOf("scale")!=-1){
var _4d=_c.position(_42,true);
_4b=((this.clickedPosX!==undefined)?this.clickedPosX:_7.global.innerWidth/2)-_4d.x;
if(this.keepScrollPos&&!this.getParent()){
_4a=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
}else{
_4a=-_4d.y;
}
_4c=((this.clickedPosY!==undefined)?this.clickedPosY:_7.global.innerHeight/2)+_4a;
_48=_4b+"px "+_4c+"px";
_49=_4b+"px "+_4c+"px";
}
}
_d.set(_42,{webkitTransformOrigin:_48});
_d.set(_43,{webkitTransformOrigin:_49});
}
}
},onAnimationStart:function(e){
},onAnimationEnd:function(e){
var _4e=e.animationName||e.target.className;
if(_4e.indexOf("Out")===-1&&_4e.indexOf("In")===-1&&_4e.indexOf("Shrink")===-1){
return;
}
var _4f=false;
if(_a.contains(this.domNode,"mblOut")){
_4f=true;
this.domNode.style.display="none";
_a.remove(this.domNode,[this._toCls(this._detail.transition),"mblIn","mblOut","mblReverse"]);
}else{
this.containerNode.style.paddingTop="";
}
_d.set(this.domNode,{webkitTransformOrigin:""});
if(_4e.indexOf("Shrink")!==-1){
var li=e.target;
li.style.display="none";
_a.remove(li,"mblCloseContent");
var p=_15.getEnclosingScrollable(this.domNode);
p&&p.onTouchEnd();
}
if(_4f){
this.invokeCallback();
}
this._clearClasses(this.domNode);
this.clickedPosX=this.clickedPosY=undefined;
if(_4e.indexOf("Cube")!==-1&&_4e.indexOf("In")!==-1&&_6("iphone")){
this.domNode.parentNode.style.webkitPerspective="";
}
},invokeCallback:function(){
this.onAfterTransitionOut.apply(this,this._arguments);
_3.publish("/dojox/mobile/afterTransitionOut",[this].concat(this._arguments));
var _50=_e.byNode(this.toNode);
if(_50){
_50.onAfterTransitionIn.apply(_50,this._arguments);
_3.publish("/dojox/mobile/afterTransitionIn",[_50].concat(this._arguments));
_50.movedFrom=undefined;
if(this.setFragIds&&this._isBookmarkable(this._detail)){
this.setFragIds(_50);
}
}
if(_6("mblAndroidWorkaround")){
setTimeout(_5.hitch(this,function(){
if(_50){
_d.set(this.toNode,"webkitTransformStyle","");
}
_d.set(this.domNode,"webkitTransformStyle","");
}),0);
}
var c=this._detail.context,m=this._detail.method;
if(c||m){
if(!m){
m=c;
c=null;
}
c=c||_7.global;
if(typeof (m)=="string"){
c[m].apply(c,this._optArgs);
}else{
if(typeof (m)=="function"){
m.apply(c,this._optArgs);
}
}
}
this._detail=this._optArgs=this._arguments=undefined;
this._inProgress=false;
},isVisible:function(_51){
var _52=function(_53){
return _d.get(_53,"display")!=="none";
};
if(_51){
for(var n=this.domNode;n.tagName!=="BODY";n=n.parentNode){
if(!_52(n)){
return false;
}
}
return true;
}else{
return _52(this.domNode);
}
},getShowingView:function(){
var _54=this.domNode.parentNode.childNodes;
for(var i=0;i<_54.length;i++){
var n=_54[i];
if(n.nodeType===1&&_a.contains(n,"mblView")&&n.style.display!=="none"){
return _e.byNode(n);
}
}
return null;
},getSiblingViews:function(){
if(!this.domNode.parentNode){
return [this];
}
return _1.map(_1.filter(this.domNode.parentNode.childNodes,function(n){
return n.nodeType===1&&_a.contains(n,"mblView");
}),function(n){
return _e.byNode(n);
});
},show:function(_55,_56){
var out=this.getShowingView();
if(!_55){
if(out){
out.onBeforeTransitionOut(out.id);
_3.publish("/dojox/mobile/beforeTransitionOut",[out,out.id]);
}
this.onBeforeTransitionIn(this.id);
_3.publish("/dojox/mobile/beforeTransitionIn",[this,this.id]);
}
if(_56){
this.domNode.style.display="";
}else{
_1.forEach(this.getSiblingViews(),function(v){
v.domNode.style.display=(v===this)?"":"none";
},this);
}
this.load&&this.load();
if(!_55){
if(out){
out.onAfterTransitionOut(out.id);
_3.publish("/dojox/mobile/afterTransitionOut",[out,out.id]);
}
this.onAfterTransitionIn(this.id);
_3.publish("/dojox/mobile/afterTransitionIn",[this,this.id]);
}
},hide:function(){
this.domNode.style.display="none";
}});
});
