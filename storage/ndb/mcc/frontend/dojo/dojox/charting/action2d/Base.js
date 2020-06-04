//>>built
define("dojox/charting/action2d/Base",["dojo/_base/lang","dojo/_base/declare","dojo/Evented"],function(_1,_2,_3){
return _2("dojox.charting.action2d.Base",_3,{constructor:function(_4,_5){
this.chart=_4;
this.plot=_5?(_1.isString(_5)?this.chart.getPlot(_5):_5):this.chart.getPlot("default");
},connect:function(){
},disconnect:function(){
},destroy:function(){
this.disconnect();
}});
});
