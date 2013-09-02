//>>built
define("dojox/app/module/history",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/declare","dojo/on"],function(_1,_2,_3,_4){
return _3(null,{postCreate:function(_5,_6){
this.inherited(arguments);
var _7=window.location.hash;
this._startView=((_7&&_7.charAt(0)=="#")?_7.substr(1):_7)||this.defaultView;
_4(this.domNode,"startTransition",_1.hitch(this,"onStartTransition"));
_4(window,"popstate",_1.hitch(this,"onPopState"));
},startup:function(){
this.inherited(arguments);
},onStartTransition:function(_8){
if(_8.preventDefault){
_8.preventDefault();
}
var _9=_8.detail.target;
var _a=/#(.+)/;
if(!_9&&_a.test(_8.detail.href)){
_9=_8.detail.href.match(_a)[1];
}
_8.cancelBubble=true;
if(_8.stopPropagation){
_8.stopPropagation();
}
_1.when(this.transition(_9,_1.mixin({reverse:false},_8.detail)),_1.hitch(this,function(){
history.pushState(_8.detail,_8.detail.href,_8.detail.url);
}));
},onPopState:function(_b){
if(this.getStatus()!==this.lifecycle.STARTED){
return;
}
var _c=_b.state;
if(!_c){
if(!this._startView&&window.location.hash){
_c={target:(location.hash&&location.hash.charAt(0)=="#")?location.hash.substr(1):location.hash,url:location.hash};
}else{
_c={};
}
}
var _d=_c.target||this._startView||this.defaultView;
if(this._startView){
this._startView=null;
}
var _e=_c.title||null;
var _f=_c.url||null;
if(_b._sim){
history.replaceState(_c,_e,_f);
}
var _10=history.state;
this.transition(_d,_1.mixin({reverse:true},_c));
}});
});
