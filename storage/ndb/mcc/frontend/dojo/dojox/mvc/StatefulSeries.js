//>>built
define("dojox/mvc/StatefulSeries",["dojo/_base/array","dojo/_base/declare","dojo/_base/lang","dojox/mvc/at"],function(_1,_2,_3,at){
return _2("dojox.mvc.StatefulSeries",null,{constructor:function(_4){
var _5=this;
function _6(){
if(_5.series){
_5.series.chart.updateSeries(_5.series.name,_5);
_5.series.chart.delayedRender();
}
};
this._handles=[];
this.data=_1.map(_4,function(_7,_8){
if((_7||{}).atsignature=="dojox.mvc.at"){
var _9=_7.target,_a=_7.targetProp;
if(_3.isString(_9)){
throw new Error("Literal-based dojox/mvc/at is not supported in dojox/mvc/StatefulSeries.");
}
if(_7.bindDirection&&!(_7.bindDirection&at.from)){
console.warn("Data binding bindDirection option is ignored in dojox/mvc/StatefulSeries.");
}
if(_a&&_3.isFunction(_9.set)&&_3.isFunction(_9.watch)){
var _b=_7.converter,_c=(_b||{}).format&&_3.hitch({target:_9,source:this},_b.format);
this._handles.push(_9.watch(_a,function(_d,_e,_f){
_5.data[_8]=_c?_c(_f):_f;
_6();
}));
}
return !_a?_9:_3.isFunction(_9.get)?_9.get(_a):_9[_a];
}else{
return _7;
}
},this);
_6();
},destroy:function(){
for(var h=null;h=this._handles.pop();){
h.unwatch();
}
},setSeriesObject:function(_10){
this.series=_10;
}});
});
