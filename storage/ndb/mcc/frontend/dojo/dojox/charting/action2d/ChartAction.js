//>>built
define("dojox/charting/action2d/ChartAction",["dojo/_base/connect","dojo/_base/declare","./Base"],function(_1,_2,_3){
return _2("dojox.charting.action2d.ChartAction",_3,{constructor:function(_4,_5){
},connect:function(){
for(var i=0;i<this._listeners.length;++i){
this._listeners[i].handle=_1.connect(this.chart.node,this._listeners[i].eventName,this,this._listeners[i].methodName);
}
},disconnect:function(){
for(var i=0;i<this._listeners.length;++i){
_1.disconnect(this._listeners[i].handle);
delete this._listeners[i].handle;
}
}});
});
