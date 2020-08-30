//>>built
define("dojox/mobile/View",["dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/sniff","dojo/_base/window","dojo/_base/Deferred","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-geometry","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./ViewController","./common","./transition","./viewRegistry","./_css3"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13,_14,_15,_16){
var dm=_5.getObject("dojox.mobile",true);
return _4("dojox.mobile.View",[_11,_10,_f],{selected:false,keepScrollPos:true,tag:"div",baseClass:"mblView",constructor:function(_17,_18){
if(_18){
_9.byId(_18).style.visibility="hidden";
}
},destroy:function(){
_15.remove(this.id);
this.inherited(arguments);
},buildRendering:function(){
if(!this.templateString){
this.domNode=this.containerNode=this.srcNodeRef||_b.create(this.tag);
}
this._animEndHandle=this.connect(this.domNode,_16.name("animationEnd"),"onAnimationEnd");
this._animStartHandle=this.connect(this.domNode,_16.name("animationStart"),"onAnimationStart");
if(!_2.mblCSS3Transition){
this._transEndHandle=this.connect(this.domNode,_16.name("transitionEnd"),"onAnimationEnd");
}
if(_6("mblAndroid3Workaround")){
_d.set(this.domNode,_16.name("transformStyle"),"preserve-3d");
}
_15.add(this);
this.inherited(arguments);
},startup:function(){
if(this._started){
return;
}
if(this._visible===undefined){
var _19=this.getSiblingViews();
var ids=location.hash&&location.hash.substring(1).split(/,/);
var _1a,_1b,_1c;
_1.forEach(_19,function(v,i){
if(_1.indexOf(ids,v.id)!==-1){
_1a=v;
}
if(i==0){
_1c=v;
}
if(v.selected){
_1b=v;
}
v._visible=false;
},this);
(_1a||_1b||_1c)._visible=true;
}
if(this._visible){
this.show(true,true);
this.defer(function(){
this.onStartView();
_3.publish("/dojox/mobile/startView",[this]);
});
}
if(this.domNode.style.visibility==="hidden"){
this.domNode.style.visibility="inherit";
}
this.inherited(arguments);
var _1d=this.getParent();
if(!_1d||!_1d.resize){
this.resize();
}
if(!this._visible){
this.hide();
}
},resize:function(){
_1.forEach(this.getChildren(),function(_1e){
if(_1e.resize){
_1e.resize();
}
});
},onStartView:function(){
},onBeforeTransitionIn:function(_1f,dir,_20,_21,_22){
},onAfterTransitionIn:function(_23,dir,_24,_25,_26){
},onBeforeTransitionOut:function(_27,dir,_28,_29,_2a){
},onAfterTransitionOut:function(_2b,dir,_2c,_2d,_2e){
},_clearClasses:function(_2f){
if(!_2f){
return;
}
var _30=[];
_1.forEach(_5.trim(_2f.className||"").split(/\s+/),function(c){
if(c.match(/^mbl\w*View$/)||c.indexOf("mbl")===-1){
_30.push(c);
}
},this);
_2f.className=_30.join(" ");
},_fixViewState:function(_31){
var _32=this.domNode.parentNode.childNodes;
for(var i=0;i<_32.length;i++){
var n=_32[i];
if(n.nodeType===1&&_a.contains(n,"mblView")){
this._clearClasses(n);
}
}
this._clearClasses(_31);
var _33=_e.byNode(_31);
if(_33){
_33._inProgress=false;
}
},convertToId:function(_34){
if(typeof (_34)=="string"){
return _34.replace(/^#?([^&?]+).*/,"$1");
}
return _34;
},_isBookmarkable:function(_35){
return _35.moveTo&&(_2.mblForceBookmarkable||_35.moveTo.charAt(0)==="#")&&!_35.hashchange;
},performTransition:function(_36,_37,_38,_39,_3a){
if(this._inProgress){
return;
}
this._inProgress=true;
var _3b,_3c;
if(_36&&typeof (_36)==="object"){
_3b=_36;
_3c=_37;
}else{
_3b={moveTo:_36,transitionDir:_37,transition:_38,context:_39,method:_3a};
_3c=[];
for(var i=5;i<arguments.length;i++){
_3c.push(arguments[i]);
}
}
this._detail=_3b;
this._optArgs=_3c;
this._arguments=[_3b.moveTo,_3b.transitionDir,_3b.transition,_3b.context,_3b.method];
if(_3b.moveTo==="#"){
return;
}
var _3d;
if(_3b.moveTo){
_3d=this.convertToId(_3b.moveTo);
}else{
if(!this._dummyNode){
this._dummyNode=_7.doc.createElement("div");
_7.body().appendChild(this._dummyNode);
}
_3d=this._dummyNode;
}
if(this.addTransitionInfo&&typeof (_3b.moveTo)=="string"&&this._isBookmarkable(_3b)){
this.addTransitionInfo(this.id,_3b.moveTo,{transitionDir:_3b.transitionDir,transition:_3b.transition});
}
var _3e=this.domNode;
var _3f=_3e.offsetTop;
_3d=this.toNode=_9.byId(_3d);
if(!_3d){
return;
}
_3d.style.visibility="hidden";
_3d.style.display="";
this._fixViewState(_3d);
var _40=_e.byNode(_3d);
if(_40){
if(_2.mblAlwaysResizeOnTransition||!_40._resized){
_13.resizeAll(null,_40);
_40._resized=true;
}
if(_3b.transition&&_3b.transition!="none"){
_40._addTransitionPaddingTop(_3f);
}
_40.load&&_40.load();
_40.movedFrom=_3e.id;
}
if(_6("mblAndroidWorkaround")&&!_2.mblCSS3Transition&&_3b.transition&&_3b.transition!="none"){
_d.set(_3d,_16.name("transformStyle"),"preserve-3d");
_d.set(_3e,_16.name("transformStyle"),"preserve-3d");
_a.add(_3d,"mblAndroidWorkaround");
}
this.onBeforeTransitionOut.apply(this,this._arguments);
_3.publish("/dojox/mobile/beforeTransitionOut",[this].concat(_5._toArray(this._arguments)));
if(_40){
if(this.keepScrollPos&&!this.getParent()){
var _41=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
_3e._scrollTop=_41;
var _42=(_3b.transitionDir==1)?0:(_3d._scrollTop||0);
_3d.style.top="0px";
if(_41>1||_42!==0){
_3e.style.top=_42-_41+"px";
if(!(_6("ios")>=7)&&_2.mblHideAddressBar!==false){
this.defer(function(){
_7.global.scrollTo(0,(_42||1));
});
}
}
}else{
_3d.style.top="0px";
}
_40.onBeforeTransitionIn.apply(_40,this._arguments);
_3.publish("/dojox/mobile/beforeTransitionIn",[_40].concat(_5._toArray(this._arguments)));
}
_3d.style.display="none";
_3d.style.visibility="inherit";
_13.fromView=this;
_13.toView=_40;
this._doTransition(_3e,_3d,_3b.transition,_3b.transitionDir);
},_addTransitionPaddingTop:function(_43){
this.containerNode.style.paddingTop=_43+"px";
},_removeTransitionPaddingTop:function(){
this.containerNode.style.paddingTop="";
},_toCls:function(s){
return "mbl"+s.charAt(0).toUpperCase()+s.substring(1);
},_doTransition:function(_44,_45,_46,_47){
var rev=(_47==-1)?" mblReverse":"";
_45.style.display="";
if(!_46||_46=="none"){
this.domNode.style.display="none";
this.invokeCallback();
}else{
if(_2.mblCSS3Transition){
_8.when(_14,_5.hitch(this,function(_48){
var _49=_d.get(_45,"position");
_d.set(_45,"position","absolute");
_8.when(_48(_44,_45,{transition:_46,reverse:(_47===-1)?true:false}),_5.hitch(this,function(){
_d.set(_45,"position",_49);
_45.style.paddingTop="";
this.invokeCallback();
}));
}));
}else{
if(_46.indexOf("cube")!=-1){
if(_6("ipad")){
_d.set(_45.parentNode,{webkitPerspective:1600});
}else{
if(_6("ios")){
_d.set(_45.parentNode,{webkitPerspective:800});
}
}
}
var s=this._toCls(_46);
if(_6("mblAndroidWorkaround")){
var _4a=this;
_4a.defer(function(){
_a.add(_44,s+" mblOut"+rev);
_a.add(_45,s+" mblIn"+rev);
_a.remove(_45,"mblAndroidWorkaround");
_4a.defer(function(){
_a.add(_44,"mblTransition");
_a.add(_45,"mblTransition");
},30);
},70);
}else{
_a.add(_44,s+" mblOut"+rev);
_a.add(_45,s+" mblIn"+rev);
this.defer(function(){
_a.add(_44,"mblTransition");
_a.add(_45,"mblTransition");
},100);
}
var _4b="50% 50%";
var _4c="50% 50%";
var _4d,_4e,_4f;
if(_46.indexOf("swirl")!=-1||_46.indexOf("zoom")!=-1){
if(this.keepScrollPos&&!this.getParent()){
_4d=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
}else{
_4d=-_c.position(_44,true).y;
}
_4f=_7.global.innerHeight/2+_4d;
_4b="50% "+_4f+"px";
_4c="50% "+_4f+"px";
}else{
if(_46.indexOf("scale")!=-1){
var _50=_c.position(_44,true);
_4e=((this.clickedPosX!==undefined)?this.clickedPosX:_7.global.innerWidth/2)-_50.x;
if(this.keepScrollPos&&!this.getParent()){
_4d=_7.body().scrollTop||_7.doc.documentElement.scrollTop||_7.global.pageYOffset||0;
}else{
_4d=-_50.y;
}
_4f=((this.clickedPosY!==undefined)?this.clickedPosY:_7.global.innerHeight/2)+_4d;
_4b=_4e+"px "+_4f+"px";
_4c=_4e+"px "+_4f+"px";
}
}
_d.set(_44,_16.add({},{transformOrigin:_4b}));
_d.set(_45,_16.add({},{transformOrigin:_4c}));
}
}
},onAnimationStart:function(e){
},onAnimationEnd:function(e){
var _51=e.animationName||e.target.className;
if(_51.indexOf("Out")===-1&&_51.indexOf("In")===-1&&_51.indexOf("Shrink")===-1){
return;
}
var _52=false;
if(_a.contains(this.domNode,"mblOut")){
_52=true;
this.domNode.style.display="none";
_a.remove(this.domNode,[this._toCls(this._detail.transition),"mblIn","mblOut","mblReverse"]);
}else{
this._removeTransitionPaddingTop();
}
_d.set(this.domNode,_16.add({},{transformOrigin:""}));
if(_51.indexOf("Shrink")!==-1){
var li=e.target;
li.style.display="none";
_a.remove(li,"mblCloseContent");
var p=_15.getEnclosingScrollable(this.domNode);
p&&p.onTouchEnd();
}
if(_52){
this.invokeCallback();
}
this._clearClasses(this.domNode);
this.clickedPosX=this.clickedPosY=undefined;
if(_51.indexOf("Cube")!==-1&&_51.indexOf("In")!==-1&&_6("ios")){
this.domNode.parentNode.style[_16.name("perspective")]="";
}
},invokeCallback:function(){
this.onAfterTransitionOut.apply(this,this._arguments);
_3.publish("/dojox/mobile/afterTransitionOut",[this].concat(this._arguments));
var _53=_e.byNode(this.toNode);
if(_53){
_53.onAfterTransitionIn.apply(_53,this._arguments);
_3.publish("/dojox/mobile/afterTransitionIn",[_53].concat(this._arguments));
_53.movedFrom=undefined;
if(this.setFragIds&&this._isBookmarkable(this._detail)){
this.setFragIds(_53);
}
}
if(_6("mblAndroidWorkaround")){
this.defer(function(){
if(_53){
_d.set(this.toNode,_16.name("transformStyle"),"");
}
_d.set(this.domNode,_16.name("transformStyle"),"");
});
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
},isVisible:function(_54){
var _55=function(_56){
return _d.get(_56,"display")!=="none";
};
if(_54){
for(var n=this.domNode;n.tagName!=="BODY";n=n.parentNode){
if(!_55(n)){
return false;
}
}
return true;
}else{
return _55(this.domNode);
}
},getShowingView:function(){
var _57=this.domNode.parentNode.childNodes;
for(var i=0;i<_57.length;i++){
var n=_57[i];
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
},show:function(_58,_59){
var out=this.getShowingView();
if(!_58){
if(out){
out.onBeforeTransitionOut(out.id);
_3.publish("/dojox/mobile/beforeTransitionOut",[out,out.id]);
}
this.onBeforeTransitionIn(this.id);
_3.publish("/dojox/mobile/beforeTransitionIn",[this,this.id]);
}
if(_59){
this.domNode.style.display="";
}else{
_1.forEach(this.getSiblingViews(),function(v){
v.domNode.style.display=(v===this)?"":"none";
},this);
}
this.load&&this.load();
if(!_58){
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
