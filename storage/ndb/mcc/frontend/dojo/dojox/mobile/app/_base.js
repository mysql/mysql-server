//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/_base,dijit/_WidgetBase,dojox/mobile,dojox/mobile/parser,dojox/mobile/Button,dojox/mobile/app/_event,dojox/mobile/app/_Widget,dojox/mobile/app/StageController,dojox/mobile/app/SceneController,dojox/mobile/app/SceneAssistant,dojox/mobile/app/AlertDialog,dojox/mobile/app/List,dojox/mobile/app/ListSelector,dojox/mobile/app/TextBox,dojox/mobile/app/ImageView,dojox/mobile/app/ImageThumbView"],function(_1,_2,_3){
_2.provide("dojox.mobile.app._base");
_2.experimental("dojox.mobile.app._base");
_2.require("dijit._base");
_2.require("dijit._WidgetBase");
_2.require("dojox.mobile");
_2.require("dojox.mobile.parser");
_2.require("dojox.mobile.Button");
_2.require("dojox.mobile.app._event");
_2.require("dojox.mobile.app._Widget");
_2.require("dojox.mobile.app.StageController");
_2.require("dojox.mobile.app.SceneController");
_2.require("dojox.mobile.app.SceneAssistant");
_2.require("dojox.mobile.app.AlertDialog");
_2.require("dojox.mobile.app.List");
_2.require("dojox.mobile.app.ListSelector");
_2.require("dojox.mobile.app.TextBox");
_2.require("dojox.mobile.app.ImageView");
_2.require("dojox.mobile.app.ImageThumbView");
(function(){
var _4;
var _5;
var _6=["dojox.mobile","dojox.mobile.parser"];
var _7={};
var _8;
var _9;
var _a=[];
function _b(_c,_d){
var _e;
var _f;
do{
_e=_c.pop();
if(_e.source){
_f=_e.source;
}else{
if(_e.module){
_f=_2.moduleUrl(_e.module)+".js";
}else{
return;
}
}
}while(_c.length>0&&_7[_f]);
if(_c.length<1&&_7[_f]){
_d();
return;
}
_2.xhrGet({url:_f,sync:false}).addCallbacks(function(_10){
_2["eval"](_10);
_7[_f]=true;
if(_c.length>0){
_b(_c,_d);
}else{
_d();
}
},function(){
});
};
var _11=function(){
_4=new _3.mobile.app.StageController(_9);
var _12={id:"com.test.app",version:"1.0.0",initialScene:"main"};
if(_2.global["appInfo"]){
_2.mixin(_12,_2.global["appInfo"]);
}
_5=_3.mobile.app.info=_12;
if(_5.title){
var _13=_2.query("head title")[0]||_2.create("title",{},_2.query("head")[0]);
document.title=_5.title;
}
_4.pushScene(_5.initialScene);
};
var _14=function(){
var _15=false;
if(_2.global.BackButton){
BackButton.override();
_2.connect(document,"backKeyDown",function(e){
_2.publish("/dojox/mobile/app/goback");
});
_15=true;
}else{
if(_2.global.Mojo){
}
}
if(_15){
_2.addClass(_2.body(),"mblNativeBack");
}
};
_2.mixin(_3.mobile.app,{init:function(_16){
_9=_16||_2.body();
_3.mobile.app.STAGE_CONTROLLER_ACTIVE=true;
_2.subscribe("/dojox/mobile/app/goback",function(){
_4.popScene();
});
_2.subscribe("/dojox/mobile/app/alert",function(_17){
_3.mobile.app.getActiveSceneController().showAlertDialog(_17);
});
_2.subscribe("/dojox/mobile/app/pushScene",function(_18,_19){
_4.pushScene(_18,_19||{});
});
_2.xhrGet({url:"view-resources.json",load:function(_1a){
var _1b=[];
if(_1a){
_a=_1a=_2.fromJson(_1a);
for(var i=0;i<_1a.length;i++){
if(!_1a[i].scene){
_1b.push(_1a[i]);
}
}
}
if(_1b.length>0){
_b(_1b,_11);
}else{
_11();
}
},error:_11});
_14();
},getActiveSceneController:function(){
return _4.getActiveSceneController();
},getStageController:function(){
return _4;
},loadResources:function(_1c,_1d){
_b(_1c,_1d);
},loadResourcesForScene:function(_1e,_1f){
var _20=[];
for(var i=0;i<_a.length;i++){
if(_a[i].scene==_1e){
_20.push(_a[i]);
}
}
if(_20.length>0){
_b(_20,_1f);
}else{
_1f();
}
},resolveTemplate:function(_21){
return "app/views/"+_21+"/"+_21+"-scene.html";
},resolveAssistant:function(_22){
return "app/assistants/"+_22+"-assistant.js";
}});
})();
});
