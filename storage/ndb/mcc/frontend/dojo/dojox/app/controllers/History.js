//>>built
define("dojox/app/controllers/History",["dojo/_base/lang","dojo/_base/declare","dojo/on","../Controller"],function(_1,_2,on,_3){
return _2("dojox.app.controllers.History",_3,{constructor:function(_4){
this.events={"startTransition":this.onStartTransition};
this.inherited(arguments);
this.bind(window,"popstate",_1.hitch(this,this.onPopState));
},_buildHashWithParams:function(_5,_6){
if(_5.charAt(0)!=="#"){
_5="#"+_5;
}
for(var _7 in _6){
var _8=_6[_7];
if(_7&&_8!=null){
_5=_5+"&"+_7+"="+_6[_7];
}
}
return _5;
},onStartTransition:function(_9){
var _a=_9.detail.target;
var _b=/#(.+)/;
if(!_a&&_b.test(_9.detail.href)){
_a=_9.detail.href.match(_b)[1];
}
var _c=_9.detail.url||"#"+_9.detail.target;
if(_9.detail.params){
_c=this._buildHashWithParams(_c,_9.detail.params);
}
history.pushState(_9.detail,_9.detail.href,_c);
},onPopState:function(_d){
if(this.app.getStatus()!==this.app.lifecycle.STARTED){
return;
}
var _e=_d.state;
if(!_e){
if(!this.app._startView&&window.location.hash){
_e={target:((location.hash&&location.hash.charAt(0)=="#")?location.hash.substr(1):location.hash).split("&")[0],url:location.hash,params:this.app.getParamsFromHash(location.hash)||this.defaultParams||{}};
}else{
_e={};
}
}
var _f=_e.target||this.app._startView||this.app.defaultView;
var _10=_e.params||this.app._startParams||this.app.defaultParams||{};
if(this.app._startView){
this.app._startView=null;
}
var _11=_e.title||null;
var _12=_e.url||null;
if(_d._sim){
history.replaceState(_e,_11,_12);
}
this.app.trigger("transition",{"viewId":_f,"opts":_1.mixin({reverse:true},_d.detail,{"params":_10})});
}});
});
