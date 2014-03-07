//>>built
define("dojox/charting/action2d/PlotAction",["dojo/_base/connect","dojo/_base/declare","./Base","dojo/fx/easing","dojox/lang/functional","dojox/lang/functional/object"],function(_1,_2,_3,_4,df,_5){
var _6=400,_7=_4.backOut;
return _2("dojox.charting.action2d.PlotAction",_3,{overOutEvents:{onmouseover:1,onmouseout:1},constructor:function(_8,_9,_a){
this.anim={};
if(!_a){
_a={};
}
this.duration=_a.duration?_a.duration:_6;
this.easing=_a.easing?_a.easing:_7;
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
df.forIn(o,function(_b){
_b.action.stop(true);
});
});
this.anim={};
}});
});
