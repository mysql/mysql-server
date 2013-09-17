//>>built
define("dojox/mobile/TransitionEvent",["dojo/_base/declare","dojo/_base/Deferred","dojo/_base/lang","dojo/on","./transition"],function(_1,_2,_3,on,_4){
return _1("dojox.mobile.TransitionEvent",null,{constructor:function(_5,_6,_7){
this.transitionOptions=_6;
this.target=_5;
this.triggerEvent=_7||null;
},dispatch:function(){
var _8={bubbles:true,cancelable:true,detail:this.transitionOptions,triggerEvent:this.triggerEvent};
var _9=on.emit(this.target,"startTransition",_8);
if(_9){
_2.when(_4,_3.hitch(this,function(_a){
_2.when(_a.call(this,_9),_3.hitch(this,function(_b){
this.endTransition(_b);
}));
}));
}
},endTransition:function(_c){
on.emit(this.target,"endTransition",{detail:_c.transitionOptions});
}});
});
