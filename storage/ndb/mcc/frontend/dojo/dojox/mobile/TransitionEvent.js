//>>built
define("dojox/mobile/TransitionEvent",["dojo/_base/declare","dojo/on"],function(_1,on){
return _1("dojox.mobile.TransitionEvent",null,{constructor:function(_2,_3,_4){
this.transitionOptions=_3;
this.target=_2;
this.triggerEvent=_4||null;
},dispatch:function(){
var _5={bubbles:true,cancelable:true,detail:this.transitionOptions,triggerEvent:this.triggerEvent};
var _6=on.emit(this.target,"startTransition",_5);
}});
});
