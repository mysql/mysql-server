//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/mobile/_base"],function(_1,_2,_3){
_2.provide("dojox.mobile.app.SceneController");
_2.experimental("dojox.mobile.app.SceneController");
_2.require("dojox.mobile._base");
(function(){
var _4=_3.mobile.app;
var _5={};
_2.declare("dojox.mobile.app.SceneController",_3.mobile.View,{stageController:null,keepScrollPos:false,init:function(_6,_7){
this.sceneName=_6;
this.params=_7;
var _8=_4.resolveTemplate(_6);
this._deferredInit=new _2.Deferred();
if(_5[_6]){
this._setContents(_5[_6]);
}else{
_2.xhrGet({url:_8,handleAs:"text"}).addCallback(_2.hitch(this,this._setContents));
}
return this._deferredInit;
},_setContents:function(_9){
_5[this.sceneName]=_9;
this.domNode.innerHTML="<div>"+_9+"</div>";
var _a="";
var _b=this.sceneName.split("-");
for(var i=0;i<_b.length;i++){
_a+=_b[i].substring(0,1).toUpperCase()+_b[i].substring(1);
}
_a+="Assistant";
this.sceneAssistantName=_a;
var _c=this;
_3.mobile.app.loadResourcesForScene(this.sceneName,function(){
var _d;
if(typeof (_2.global[_a])!="undefined"){
_c._initAssistant();
}else{
var _e=_4.resolveAssistant(_c.sceneName);
_2.xhrGet({url:_e,handleAs:"text"}).addCallback(function(_f){
try{
_2.eval(_f);
}
catch(e){
throw e;
}
_c._initAssistant();
});
}
});
},_initAssistant:function(){
var cls=_2.getObject(this.sceneAssistantName);
if(!cls){
throw Error("Unable to resolve scene assistant "+this.sceneAssistantName);
}
this.assistant=new cls(this.params);
this.assistant.controller=this;
this.assistant.domNode=this.domNode.firstChild;
this.assistant.setup();
this._deferredInit.callback();
},query:function(_10,_11){
return _2.query(_10,_11||this.domNode);
},parse:function(_12){
var _13=this._widgets=_3.mobile.parser.parse(_12||this.domNode,{controller:this});
for(var i=0;i<_13.length;i++){
_13[i].set("controller",this);
}
},getWindowSize:function(){
return {w:_2.global.innerWidth,h:_2.global.innerHeight};
},showAlertDialog:function(_14){
var _15=_2.marginBox(this.assistant.domNode);
var _16=new _3.mobile.app.AlertDialog(_2.mixin(_14,{controller:this}));
this.assistant.domNode.appendChild(_16.domNode);
_16.show();
},popupSubMenu:function(_17){
var _18=new _3.mobile.app.ListSelector({controller:this,destroyOnHide:true,onChoose:_17.onChoose});
this.assistant.domNode.appendChild(_18.domNode);
_18.set("data",_17.choices);
_18.show(_17.fromNode);
}});
})();
});
