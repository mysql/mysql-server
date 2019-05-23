//>>built
define("dojox/charting/widget/BidiSupport",["dojo/dom","dojo/_base/lang","dojo/_base/array","dojo/_base/connect","dojo/query","dijit/_BidiSupport","../BidiSupport","dijit/registry","./Chart","./Legend"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_2.extend(_a,{postMixInProperties:function(){
if(!this.chart){
if(!this.chartRef){
return;
}
var _b=_8.byId(this.chartRef);
if(!_b){
var _c=_1.byId(this.chartRef);
if(_c){
_b=_8.byNode(_c);
}else{
return;
}
}
this.textDir=_b.chart.textDir;
_4.connect(_b.chart,"setTextDir",this,"_setTextDirAttr");
}else{
this.textDir=this.chart.textDir;
_4.connect(this.chart,"setTextDir",this,"_setTextDirAttr");
}
},_setTextDirAttr:function(_d){
if(_e(_d)!=null){
if(this.textDir!=_d){
this._set("textDir",_d);
var _f=_5(".dojoxLegendText",this._tr);
_3.forEach(_f,function(_10){
_10.dir=this.getTextDir(_10.innerHTML,_10.dir);
},this);
}
}
}});
_2.extend(_9,{postMixInProperties:function(){
this.textDir=this.params["textDir"]?this.params["textDir"]:this.params["dir"];
},_setTextDirAttr:function(_11){
if(_e(_11)!=null){
this._set("textDir",_11);
this.chart.setTextDir(_11);
}
}});
function _e(_12){
return /^(ltr|rtl|auto)$/.test(_12)?_12:null;
};
return _9;
});
