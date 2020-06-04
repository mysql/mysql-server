//>>built
define("dojox/charting/bidi/action2d/ZoomAndPan",["dojo/_base/declare"],function(_1){
return _1(null,{_getDelta:function(_2){
var _3=this.inherited(arguments);
return _3*(this.chart.isRightToLeft()?-1:1);
}});
});
