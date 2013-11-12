//>>built
define("dojox/mobile/View",["dojo/_base/kernel","dojo/_base/array","dojo/_base/config","dojo/_base/connect","dojo/_base/declare","dojo/_base/lang","dojo/_base/sniff","dojo/_base/window","dojo/_base/Deferred","dojo/dom","dojo/dom-class","dojo/dom-geometry","dojo/dom-style","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./ViewController","./transition"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13){
var dm=_6.getObject("dojox.mobile",true);
return _5("dojox.mobile.View",[_11,_10,_f],{selected:false,keepScrollPos:true,constructor:function(_14,_15){
if(_15){
_a.byId(_15).style.visibility="hidden";
}
this._aw=_7("android")>=2.2&&_7("android")<3;
},buildRendering:function(){
this.domNode=this.containerNode=this.srcNodeRef||_8.doc.createElement("DIV");
this.domNode.className="mblView";
this.connect(this.domNode,"webkitAnimationEnd","onAnimationEnd");
this.connect(this.domNode,"webkitAnimationStart","onAnimationStart");
if(!_3["mblCSS3Transition"]){
this.connect(this.domNode,"webkitTransitionEnd","onAnimationEnd");
}
var id=location.href.match(/#(\w+)([^\w=]|$)/)?RegExp.$1:null;
this._visible=this.selected&&!id||this.id==id;
if(this.selected){
dm._defaultView=this;
}
},startup:function(){
if(this._started){
return;
}
var _16=[];
var _17=this.domNode.parentNode.childNodes;
var _18=false;
for(var i=0;i<_17.length;i++){
var c=_17[i];
if(c.nodeType===1&&_b.contains(c,"mblView")){
_16.push(c);
_18=_18||_e.byNode(c)._visible;
}
}
var _19=this._visible;
if(_16.length===1||(!_18&&_16[0]===this.domNode)){
_19=true;
}
var _1a=this;
setTimeout(function(){
if(!_19){
_1a.domNode.style.display="none";
}else{
dm.currentView=_1a;
_1a.onStartView();
_4.publish("/dojox/mobile/startView",[_1a]);
}
if(_1a.domNode.style.visibility!="visible"){
_1a.domNode.style.visibility="visible";
}
var _1b=_1a.getParent&&_1a.getParent();
if(!_1b||!_1b.resize){
_1a.resize();
}
},_7("ie")?100:0);
this.inherited(arguments);
},resize:function(){
_2.forEach(this.getChildren(),function(_1c){
if(_1c.resize){
_1c.resize();
}
});
},onStartView:function(){
},onBeforeTransitionIn:function(_1d,dir,_1e,_1f,_20){
},onAfterTransitionIn:function(_21,dir,_22,_23,_24){
},onBeforeTransitionOut:function(_25,dir,_26,_27,_28){
},onAfterTransitionOut:function(_29,dir,_2a,_2b,_2c){
},_saveState:function(_2d,dir,_2e,_2f,_30){
this._context=_2f;
this._method=_30;
if(_2e=="none"){
_2e=null;
}
this._moveTo=_2d;
this._dir=dir;
this._transition=_2e;
this._arguments=_6._toArray(arguments);
this._args=[];
if(_2f||_30){
for(var i=5;i<arguments.length;i++){
this._args.push(arguments[i]);
}
}
},_fixViewState:function(_31){
var _32=this.domNode.parentNode.childNodes;
for(var i=0;i<_32.length;i++){
var n=_32[i];
if(n.nodeType===1&&_b.contains(n,"mblView")){
n.className="mblView";
}
}
_31.className="mblView";
},convertToId:function(_33){
if(typeof (_33)=="string"){
_33.match(/^#?([^&?]+)/);
return RegExp.$1;
}
return _33;
},performTransition:function(_34,dir,_35,_36,_37){
if(_34==="#"){
return;
}
if(_1.hash){
if(typeof (_34)=="string"&&_34.charAt(0)=="#"&&!dm._params){
dm._params=[];
for(var i=0;i<arguments.length;i++){
dm._params.push(arguments[i]);
}
_1.hash(_34);
return;
}
}
this._saveState.apply(this,arguments);
var _38;
if(_34){
_38=this.convertToId(_34);
}else{
if(!this._dummyNode){
this._dummyNode=_8.doc.createElement("DIV");
_8.body().appendChild(this._dummyNode);
}
_38=this._dummyNode;
}
var _39=this.domNode;
var _3a=_39.offsetTop;
_38=this.toNode=_a.byId(_38);
if(!_38){
return;
}
_38.style.visibility=this._aw?"visible":"hidden";
_38.style.display="";
this._fixViewState(_38);
var _3b=_e.byNode(_38);
if(_3b){
if(_3["mblAlwaysResizeOnTransition"]||!_3b._resized){
dm.resizeAll(null,_3b);
_3b._resized=true;
}
if(_35&&_35!="none"){
_3b.containerNode.style.paddingTop=_3a+"px";
}
_3b.movedFrom=_39.id;
}
this.onBeforeTransitionOut.apply(this,arguments);
_4.publish("/dojox/mobile/beforeTransitionOut",[this].concat(_6._toArray(arguments)));
if(_3b){
if(this.keepScrollPos&&!this.getParent()){
var _3c=_8.body().scrollTop||_8.doc.documentElement.scrollTop||_8.global.pageYOffset||0;
_39._scrollTop=_3c;
var _3d=(dir==1)?0:(_38._scrollTop||0);
_38.style.top="0px";
if(_3c>1||_3d!==0){
_39.style.top=_3d-_3c+"px";
if(_3["mblHideAddressBar"]!==false){
setTimeout(function(){
_8.global.scrollTo(0,(_3d||1));
},0);
}
}
}else{
_38.style.top="0px";
}
_3b.onBeforeTransitionIn.apply(_3b,arguments);
_4.publish("/dojox/mobile/beforeTransitionIn",[_3b].concat(_6._toArray(arguments)));
}
if(!this._aw){
_38.style.display="none";
_38.style.visibility="visible";
}
if(dm._iw&&dm.scrollable){
var ss=dm.getScreenSize();
_8.body().appendChild(dm._iwBgCover);
_d.set(dm._iwBgCover,{position:"absolute",top:"0px",left:"0px",height:(ss.h+1)+"px",width:ss.w+"px",backgroundColor:_d.get(_8.body(),"background-color"),zIndex:-10000,display:""});
_d.set(_38,{position:"absolute",zIndex:-10001,visibility:"visible",display:""});
setTimeout(_6.hitch(this,function(){
this._doTransition(_39,_38,_35,dir);
}),80);
}else{
this._doTransition(_39,_38,_35,dir);
}
},_toCls:function(s){
return "mbl"+s.charAt(0).toUpperCase()+s.substring(1);
},_doTransition:function(_3e,_3f,_40,dir){
var rev=(dir==-1)?" mblReverse":"";
if(dm._iw&&dm.scrollable){
_d.set(_3f,{position:"",zIndex:""});
_8.body().removeChild(dm._iwBgCover);
}else{
if(!this._aw){
_3f.style.display="";
}
}
if(!_40||_40=="none"){
this.domNode.style.display="none";
this.invokeCallback();
}else{
if(_3["mblCSS3Transition"]){
_9.when(_13,_6.hitch(this,function(_41){
var _42=_d.get(_3f,"position");
_d.set(_3f,"position","absolute");
_9.when(_41(_3e,_3f,{transition:_40,reverse:(dir===-1)?true:false}),_6.hitch(this,function(){
_d.set(_3f,"position",_42);
this.invokeCallback();
}));
}));
}else{
var s=this._toCls(_40);
_b.add(_3e,s+" mblOut"+rev);
_b.add(_3f,s+" mblIn"+rev);
setTimeout(function(){
_b.add(_3e,"mblTransition");
_b.add(_3f,"mblTransition");
},100);
var _43="50% 50%";
var _44="50% 50%";
var _45,_46,_47;
if(_40.indexOf("swirl")!=-1||_40.indexOf("zoom")!=-1){
if(this.keepScrollPos&&!this.getParent()){
_45=_8.body().scrollTop||_8.doc.documentElement.scrollTop||_8.global.pageYOffset||0;
}else{
_45=-_c.position(_3e,true).y;
}
_47=_8.global.innerHeight/2+_45;
_43="50% "+_47+"px";
_44="50% "+_47+"px";
}else{
if(_40.indexOf("scale")!=-1){
var _48=_c.position(_3e,true);
_46=((this.clickedPosX!==undefined)?this.clickedPosX:_8.global.innerWidth/2)-_48.x;
if(this.keepScrollPos&&!this.getParent()){
_45=_8.body().scrollTop||_8.doc.documentElement.scrollTop||_8.global.pageYOffset||0;
}else{
_45=-_48.y;
}
_47=((this.clickedPosY!==undefined)?this.clickedPosY:_8.global.innerHeight/2)+_45;
_43=_46+"px "+_47+"px";
_44=_46+"px "+_47+"px";
}
}
_d.set(_3e,{webkitTransformOrigin:_43});
_d.set(_3f,{webkitTransformOrigin:_44});
}
}
dm.currentView=_e.byNode(_3f);
},onAnimationStart:function(e){
},onAnimationEnd:function(e){
var _49=e.animationName||e.target.className;
if(_49.indexOf("Out")===-1&&_49.indexOf("In")===-1&&_49.indexOf("Shrink")===-1){
return;
}
var _4a=false;
if(_b.contains(this.domNode,"mblOut")){
_4a=true;
this.domNode.style.display="none";
_b.remove(this.domNode,[this._toCls(this._transition),"mblIn","mblOut","mblReverse"]);
}else{
this.containerNode.style.paddingTop="";
}
_d.set(this.domNode,{webkitTransformOrigin:""});
if(_49.indexOf("Shrink")!==-1){
var li=e.target;
li.style.display="none";
_b.remove(li,"mblCloseContent");
}
if(_4a){
this.invokeCallback();
}
this.domNode&&(this.domNode.className="mblView");
this.clickedPosX=this.clickedPosY=undefined;
},invokeCallback:function(){
this.onAfterTransitionOut.apply(this,this._arguments);
_4.publish("/dojox/mobile/afterTransitionOut",[this].concat(this._arguments));
var _4b=_e.byNode(this.toNode);
if(_4b){
_4b.onAfterTransitionIn.apply(_4b,this._arguments);
_4.publish("/dojox/mobile/afterTransitionIn",[_4b].concat(this._arguments));
_4b.movedFrom=undefined;
}
var c=this._context,m=this._method;
if(!c&&!m){
return;
}
if(!m){
m=c;
c=null;
}
c=c||_8.global;
if(typeof (m)=="string"){
c[m].apply(c,this._args);
}else{
m.apply(c,this._args);
}
},getShowingView:function(){
var _4c=this.domNode.parentNode.childNodes;
for(var i=0;i<_4c.length;i++){
var n=_4c[i];
if(n.nodeType===1&&_b.contains(n,"mblView")&&_d.get(n,"display")!=="none"){
return _e.byNode(n);
}
}
return null;
},show:function(){
var _4d=this.getShowingView();
if(_4d){
_4d.domNode.style.display="none";
}
this.domNode.style.display="";
dm.currentView=this;
}});
});
