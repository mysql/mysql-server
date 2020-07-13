//>>built
define("dojox/charting/action2d/PlotAction",["dojo/_base/connect","dojo/_base/declare","./Base","dojo/fx/easing","dojox/lang/functional"],function(_1,_2,_3,_4,df){
var _5=400,_6=_4.backOut;
return _2("dojox.charting.action2d.PlotAction",_3,{overOutEvents:{onmouseover:1,onmouseout:1},constructor:function(_7,_8,_9){
this.anim={};
if(!_9){
_9={};
}
this.duration=_9.duration?_9.duration:_5;
this.easing=_9.easing?_9.easing:_6;
},connect:function(){
this.handle=this.chart.connectToPlot(this.plot.name,this,"process");
},disconnect:function(){
if(this.handle){
_1.disconnect(this.handle);
this.handle=null;
}
},reset:function(){
},destroy:function(){
this.inherited(arguments);
df.forIn(this.anim,function(o){
df.forIn(o,function(_a){
_a.action.stop(true);
});
});
this.anim={};
}});
});
