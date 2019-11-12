//>>built
define("dojox/mobile/_ItemBase",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/touch","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./TransitionEvent","./iconUtils","./sniff"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d){
return _2("dojox.mobile._ItemBase",[_a,_9,_8],{icon:"",iconPos:"",alt:"",href:"",hrefTarget:"",moveTo:"",scene:"",clickable:false,url:"",urlTarget:"",back:false,transition:"",transitionDir:1,transitionOptions:null,callback:null,label:"",toggle:false,selected:false,tabIndex:"0",_setTabIndexAttr:"",paramsToInherit:"transition,icon",_selStartMethod:"none",_selEndMethod:"none",_delayedSelection:false,_duration:800,_handleClick:true,buildRendering:function(){
this.inherited(arguments);
this._isOnLine=this.inheritParams();
},startup:function(){
if(this._started){
return;
}
if(!this._isOnLine){
this.inheritParams();
}
if(this._handleClick&&this._selStartMethod==="touch"){
this._onTouchStartHandle=this.connect(this.domNode,_6.press,"_onTouchStart");
}
this.inherited(arguments);
},inheritParams:function(){
var _e=this.getParent();
if(_e){
_1.forEach(this.paramsToInherit.split(/,/),function(p){
if(p.match(/icon/i)){
var _f=p+"Base",pos=p+"Pos";
if(this[p]&&_e[_f]&&_e[_f].charAt(_e[_f].length-1)==="/"){
this[p]=_e[_f]+this[p];
}
if(!this[p]){
this[p]=_e[_f];
}
if(!this[pos]){
this[pos]=_e[pos];
}
}
if(!this[p]){
this[p]=_e[p];
}
},this);
}
return !!_e;
},getTransOpts:function(){
var _10=this.transitionOptions||{};
_1.forEach(["moveTo","href","hrefTarget","url","target","urlTarget","scene","transition","transitionDir"],function(p){
_10[p]=_10[p]||this[p];
},this);
return _10;
},userClickAction:function(){
},defaultClickAction:function(e){
this.handleSelection(e);
if(this.userClickAction(e)===false){
return;
}
this.makeTransition(e);
},handleSelection:function(e){
if(this._delayedSelection){
this.set("selected",true);
}
if(this._onTouchEndHandle){
this.disconnect(this._onTouchEndHandle);
this._onTouchEndHandle=null;
}
var p=this.getParent();
if(this.toggle){
this.set("selected",!this._currentSel);
}else{
if(p&&p.selectOne){
this.set("selected",true);
}else{
if(this._selEndMethod==="touch"){
this.set("selected",false);
}else{
if(this._selEndMethod==="timer"){
var _11=this;
this.defer(function(){
_11.set("selected",false);
},this._duration);
}
}
}
}
},makeTransition:function(e){
if(this.back&&history){
history.back();
return;
}
if(this.href&&this.hrefTarget){
_4.global.open(this.href,this.hrefTarget||"_blank");
this._onNewWindowOpened(e);
return;
}
var _12=this.getTransOpts();
var _13=!!(_12.moveTo||_12.href||_12.url||_12.target||_12.scene);
if(this._prepareForTransition(e,_13?_12:null)===false){
return;
}
if(_13){
this.setTransitionPos(e);
new _b(this.domNode,_12,e).dispatch();
}
},_onNewWindowOpened:function(){
},_prepareForTransition:function(e,_14){
},_onTouchStart:function(e){
if(this.getParent().isEditing||this.onTouchStart(e)===false){
return;
}
if(!this._onTouchEndHandle&&this._selStartMethod==="touch"){
this._onTouchMoveHandle=this.connect(_4.body(),_6.move,"_onTouchMove");
this._onTouchEndHandle=this.connect(_4.body(),_6.release,"_onTouchEnd");
}
this.touchStartX=e.touches?e.touches[0].pageX:e.clientX;
this.touchStartY=e.touches?e.touches[0].pageY:e.clientY;
this._currentSel=this.selected;
if(this._delayedSelection){
this._selTimer=setTimeout(_3.hitch(this,function(){
this.set("selected",true);
}),100);
}else{
this.set("selected",true);
}
},onTouchStart:function(){
},_onTouchMove:function(e){
var x=e.touches?e.touches[0].pageX:e.clientX;
var y=e.touches?e.touches[0].pageY:e.clientY;
if(Math.abs(x-this.touchStartX)>=4||Math.abs(y-this.touchStartY)>=4){
this.cancel();
var p=this.getParent();
if(p&&p.selectOne){
this._prevSel&&this._prevSel.set("selected",true);
}else{
this.set("selected",false);
}
}
},_disconnect:function(){
this.disconnect(this._onTouchMoveHandle);
this.disconnect(this._onTouchEndHandle);
this._onTouchMoveHandle=this._onTouchEndHandle=null;
},cancel:function(){
if(this._selTimer){
clearTimeout(this._selTimer);
this._selTimer=null;
}
this._disconnect();
},_onTouchEnd:function(e){
if(!this._selTimer&&this._delayedSelection){
return;
}
this.cancel();
this._onClick(e);
},setTransitionPos:function(e){
var w=this;
while(true){
w=w.getParent();
if(!w||_5.contains(w.domNode,"mblView")){
break;
}
}
if(w){
w.clickedPosX=e.clientX;
w.clickedPosY=e.clientY;
}
},transitionTo:function(_15,_16,url,_17){
var _18=(_15&&typeof (_15)==="object")?_15:{moveTo:_15,href:_16,url:url,scene:_17,transition:this.transition,transitionDir:this.transitionDir};
new _b(this.domNode,_18).dispatch();
},_setIconAttr:function(_19){
if(!this._isOnLine){
this._pendingIcon=_19;
return;
}
this._set("icon",_19);
this.iconNode=_c.setIcon(_19,this.iconPos,this.iconNode,this.alt,this.iconParentNode,this.refNode,this.position);
},_setLabelAttr:function(_1a){
this._set("label",_1a);
this.labelNode.innerHTML=this._cv?this._cv(_1a):_1a;
},_setSelectedAttr:function(_1b){
if(_1b){
var p=this.getParent();
if(p&&p.selectOne){
var arr=_1.filter(p.getChildren(),function(w){
return w.selected;
});
_1.forEach(arr,function(c){
this._prevSel=c;
c.set("selected",false);
},this);
}
}
this._set("selected",_1b);
}});
});
