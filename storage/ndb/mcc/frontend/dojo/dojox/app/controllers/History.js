//>>built
define("dojox/app/controllers/History",["dojo/_base/lang","dojo/_base/declare","dojo/on","../Controller","../utils/hash","dojo/topic"],function(_1,_2,on,_3,_4,_5){
return _2("dojox.app.controllers.History",_3,{_currentPosition:0,currentState:{},constructor:function(){
this.events={"app-domNode":this.onDomNodeChange};
if(this.app.domNode){
this.onDomNodeChange({oldNode:null,newNode:this.app.domNode});
}
this.bind(window,"popstate",_1.hitch(this,this.onPopState));
},onDomNodeChange:function(_6){
if(_6.oldNode!=null){
this.unbind(_6.oldNode,"startTransition");
}
this.bind(_6.newNode,"startTransition",_1.hitch(this,this.onStartTransition));
},onStartTransition:function(_7){
var _8=window.location.hash;
var _9=_4.getTarget(_8,this.app.defaultView);
var _a=_4.getParams(_8);
var _b=_1.clone(_7.detail);
_b.target=_b.title=_9;
_b.url=_8;
_b.params=_a;
_b.id=this._currentPosition;
if(history.length==1){
history.pushState(_b,_b.href,_8);
}
_b.bwdTransition=_b.transition;
_1.mixin(this.currentState,_b);
history.replaceState(this.currentState,this.currentState.href,_8);
this._currentPosition+=1;
_7.detail.id=this._currentPosition;
var _c=_7.detail.url||"#"+_7.detail.target;
if(_7.detail.params){
_c=_4.buildWithParams(_c,_7.detail.params);
}
_7.detail.fwdTransition=_7.detail.transition;
history.pushState(_7.detail,_7.detail.href,_c);
this.currentState=_1.clone(_7.detail);
_5.publish("/app/history/pushState",_7.detail.target);
},onPopState:function(_d){
if((this.app.getStatus()!==this.app.lifecycle.STARTED)||!_d.state){
return;
}
var _e=_d.state.id<this._currentPosition;
_e?this._currentPosition-=1:this._currentPosition+=1;
var _f=_1.mixin({reverse:_e?true:false},_d.state);
_f.transition=_e?_f.bwdTransition:_f.fwdTransition;
this.app.emit("app-transition",{viewId:_d.state.target,opts:_f});
_5.publish("/app/history/popState",_d.state.target);
}});
});
