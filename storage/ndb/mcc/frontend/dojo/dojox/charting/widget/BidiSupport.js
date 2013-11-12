//>>built
define("dojox/charting/widget/BidiSupport",["dojo/dom","dojo/_base/lang","dojo/_base/html","dojo/_base/array","dojo/_base/connect","dojo/query","dijit/_BidiSupport","../BidiSupport","dijit/registry","./Chart","./Legend"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
if(_b){
_2.extend(_b,{postMixInProperties:function(){
if(!this.chart){
if(!this.chartRef){
return;
}
var _c=_9.byId(this.chartRef);
if(!_c){
var _d=_1.byId(this.chartRef);
if(_d){
_c=_9.byNode(_d);
}else{
return;
}
}
this.textDir=_c.chart.textDir;
_5.connect(_c.chart,"setTextDir",this,"_setTextDirAttr");
}else{
this.textDir=this.chart.textDir;
_5.connect(this.chart,"setTextDir",this,"_setTextDirAttr");
}
},_setTextDirAttr:function(_e){
if(_f(_e)!=null){
if(this.textDir!=_e){
this._set("textDir",_e);
var _10=_6(".dojoxLegendText",this._tr);
_4.forEach(_10,function(_11){
_11.dir=this.getTextDir(_11.innerHTML,_11.dir);
},this);
}
}
}});
}
if(_a){
_2.extend(_a,{postMixInProperties:function(){
this.textDir=this.params["textDir"]?this.params["textDir"]:this.params["dir"];
},_setTextDirAttr:function(_12){
if(_f(_12)!=null){
this._set("textDir",_12);
this.chart.setTextDir(_12);
}
}});
}
function _f(_13){
return /^(ltr|rtl|auto)$/.test(_13)?_13:null;
};
});
