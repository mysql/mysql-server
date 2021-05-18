//>>built
define("dojox/charting/bidi/widget/Legend",["dojo/_base/declare","dojo/dom","dijit/registry","dojo/_base/connect","dojo/_base/array","dojo/query"],function(_1,_2,_3,_4,_5,_6){
function _7(_8){
return /^(ltr|rtl|auto)$/.test(_8)?_8:null;
};
return _1(null,{postMixInProperties:function(){
if(!this.chart){
if(!this.chartRef){
return;
}
var _9=_3.byId(this.chartRef);
if(!_9){
var _a=_2.byId(this.chartRef);
if(_a){
_9=_3.byNode(_a);
}else{
return;
}
}
this.textDir=_9.chart.textDir;
_4.connect(_9.chart,"setTextDir",this,"_setTextDirAttr");
}else{
this.textDir=this.chart.textDir;
_4.connect(this.chart,"setTextDir",this,"_setTextDirAttr");
}
},_setTextDirAttr:function(_b){
if(_7(_b)!=null){
if(this.textDir!=_b){
this._set("textDir",_b);
var _c=_6(".dojoxLegendText",this._tr);
_5.forEach(_c,function(_d){
_d.dir=this.getTextDir(_d.innerHTML,_d.dir);
},this);
}
}
}});
});
