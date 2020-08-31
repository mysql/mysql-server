//>>built
define("dojox/charting/bidi/widget/Chart",["dojo/_base/declare"],function(_1){
function _2(_3){
return /^(ltr|rtl|auto)$/.test(_3)?_3:null;
};
return _1(null,{postMixInProperties:function(){
this.textDir=this.params["textDir"]?this.params["textDir"]:this.params["dir"];
},_setTextDirAttr:function(_4){
if(_2(_4)!=null){
this._set("textDir",_4);
this.chart.setTextDir(_4);
}
},_setDirAttr:function(_5){
this._set("dir",_5);
this.chart.setDir(_5);
}});
});
