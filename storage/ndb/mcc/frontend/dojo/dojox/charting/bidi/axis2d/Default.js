//>>built
define("dojox/charting/bidi/axis2d/Default",["dojo/_base/declare","dojo/dom-style"],function(_1,_2){
return _1(null,{labelTooltip:function(_3,_4,_5,_6,_7,_8){
var _9=(_2.get(_4.node,"direction")=="rtl");
var _a=(_4.getTextDir(_5)=="rtl");
if(_a&&!_9){
_5="<span dir='rtl'>"+_5+"</span>";
}
if(!_a&&_9){
_5="<span dir='ltr'>"+_5+"</span>";
}
this.inherited(arguments);
},_isRtl:function(){
return this.chart.isRightToLeft();
}});
});
