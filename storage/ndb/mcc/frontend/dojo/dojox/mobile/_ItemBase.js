//>>built
define("dojox/mobile/_ItemBase",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojo/_base/window","dojo/dom-class","dojo/touch","dijit/registry","dijit/_Contained","dijit/_Container","dijit/_WidgetBase","./TransitionEvent","./iconUtils","./sniff","./viewRegistry","dojo/has!dojo-bidi?dojox/mobile/bidi/_ItemBase"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f){
var _10=_2(_d("dojo-bidi")?"dojox.mobile._NonBidiItemBase":"dojox.mobile._ItemBase",[_a,_9,_8],{icon:"",iconPos:"",alt:"",href:"",hrefTarget:"",moveTo:"",scene:"",clickable:false,url:"",urlTarget:"",back:false,transition:"",transitionDir:1,transitionOptions:null,callback:null,label:"",toggle:false,selected:false,tabIndex:"0",_setTabIndexAttr:"",paramsToInherit:"transition,icon",_selStartMethod:"none",_selEndMethod:"none",_delayedSelection:false,_duration:800,_handleClick:true,buildRendering:function(){
this.inherited(arguments);
this._isOnLine=this.inheritParams();
},startup:function(){
if(this._started){
return;
}
if(!this._isOnLine){
this.inheritParams();
}
this._updateHandles();
this.inherited(arguments);
},inheritParams:function(){
var _11=this.getParent();
if(_11){
_1.forEach(this.paramsToInherit.split(/,/),function(p){
if(p.match(/icon/i)){
var _12=p+"Base",pos=p+"Pos";
if(this[p]&&_11[_12]&&_11[_12].charAt(_11[_12].length-1)==="/"){
this[p]=_11[_12]+this[p];
}
if(!this[p]){
this[p]=_11[_12];
}
if(!this[pos]){
this[pos]=_11[pos];
}
}
if(!this[p]){
this[p]=_11[p];
}
},this);
}
return !!_11;
},_updateHandles:function(){
if(this._handleClick&&this._selStartMethod==="touch"){
if(!this._onTouchStartHandle){
this._onTouchStartHandle=this.connect(this.domNode,_6.press,"_onTouchStart");
}
}else{
if(this._onTouchStartHandle){
this.disconnect(this._onTouchStartHandle);
this._onTouchStartHandle=null;
}
}
},getTransOpts:function(){
var _13=this.transitionOptions||{};
_1.forEach(["moveTo","href","hrefTarget","url","target","urlTarget","scene","transition","transitionDir"],function(p){
_13[p]=_13[p]||this[p];
},this);
return _13;
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
this.defer(function(){
this.set("selected",false);
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
if(this.href&&this.hrefTarget&&this.hrefTarget!="_self"){
_4.global.open(this.href,this.hrefTarget||"_blank");
this._onNewWindowOpened(e);
return;
}
var _14=this.getTransOpts();
var _15=!!(_14.moveTo||_14.href||_14.url||_14.target||_14.scene);
if(this._prepareForTransition(e,_15?_14:null)===false){
return;
}
if(_15){
this.setTransitionPos(e);
new _b(this.domNode,_14,e).dispatch();
}
},_onNewWindowOpened:function(){
},_prepareForTransition:function(e,_16){
},_onTouchStart:function(e){
if(this.getParent().isEditing||this.onTouchStart(e)===false){
return;
}
var _17=_e.getEnclosingScrollable(this.domNode);
if(_17&&_5.contains(_17.containerNode,"mblScrollableScrollTo2")){
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
this._selTimer=this.defer(function(){
this.set("selected",true);
},100);
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
this._selTimer.remove();
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
},transitionTo:function(_18,_19,url,_1a){
var _1b=(_18&&typeof (_18)==="object")?_18:{moveTo:_18,href:_19,url:url,scene:_1a,transition:this.transition,transitionDir:this.transitionDir};
new _b(this.domNode,_1b).dispatch();
},_setIconAttr:function(_1c){
if(!this._isOnLine){
this._pendingIcon=_1c;
return;
}
this._set("icon",_1c);
this.iconNode=_c.setIcon(_1c,this.iconPos,this.iconNode,this.alt,this.iconParentNode,this.refNode,this.position);
},_setLabelAttr:function(_1d){
this._set("label",_1d);
this.labelNode.innerHTML=this._cv?this._cv(_1d):_1d;
},_setSelectedAttr:function(_1e){
if(_1e){
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
this._set("selected",_1e);
}});
return _d("dojo-bidi")?_2("dojox.mobile._ItemBase",[_10,_f]):_10;
});
