//>>built
define("dojox/charting/Series",["dojo/_base/lang","dojo/_base/declare","./Element"],function(_1,_2,_3){
return _2("dojox.charting.Series",_3,{constructor:function(_4,_5,_6){
_1.mixin(this,_6);
if(typeof this.plot!="string"){
this.plot="default";
}
this.update(_5);
},clear:function(){
this.dyn={};
},update:function(_7){
if(_1.isArray(_7)){
this.data=_7;
}else{
this.source=_7;
this.data=this.source.data;
if(this.source.setSeriesObject){
this.source.setSeriesObject(this);
}
}
this.dirty=true;
this.clear();
}});
});
