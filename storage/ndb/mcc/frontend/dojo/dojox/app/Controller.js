//>>built
define("dojox/app/Controller",["dojo/_base/lang","dojo/_base/declare","dojo/on"],function(_1,_2,on){
return _2("dojox.app.Controller",null,{constructor:function(_3,_4){
this.events=this.events||_4;
this._boundEvents=[];
this.app=_3;
if(this.events){
for(var _5 in this.events){
if(_5.charAt(0)!=="_"){
this.bind(this.app.domNode,_5,_1.hitch(this,this.events[_5]));
}
}
}
},bind:function(_6,_7,_8){
if(!_8){
console.warn("bind event '"+_7+"' without callback function.");
}
var _9=on(_6,_7,_8);
this._boundEvents.push({"event":_7,"evented":_6,"signal":_9});
},unbind:function(_a,_b){
var _c=this._boundEvents.length;
for(var i=0;i<_c;i++){
if((this._boundEvents[i]["event"]==_b)&&(this._boundEvents[i]["evented"]==_a)){
this._boundEvents[i]["signal"].remove();
this._boundEvents.splice(i,1);
return;
}
}
console.warn("event '"+_b+"' not bind on ",_a);
}});
});
