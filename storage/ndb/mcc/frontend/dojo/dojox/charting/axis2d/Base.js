//>>built
define("dojox/charting/axis2d/Base",["dojo/_base/declare","../Element"],function(_1,_2){
return _1("dojox.charting.axis2d.Base",_2,{constructor:function(_3,_4){
this.vertical=_4&&_4.vertical;
this.opt={};
this.opt.min=_4&&_4.min;
this.opt.max=_4&&_4.max;
},clear:function(){
return this;
},initialized:function(){
return false;
},calculate:function(_5,_6,_7){
return this;
},getScaler:function(){
return null;
},getTicks:function(){
return null;
},getOffsets:function(){
return {l:0,r:0,t:0,b:0};
},render:function(_8,_9){
this.dirty=false;
return this;
}});
});
