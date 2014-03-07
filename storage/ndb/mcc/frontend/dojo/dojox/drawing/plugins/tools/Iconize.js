//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/drawing/plugins/_Plugin"],function(_1,_2,_3){
_2.provide("dojox.drawing.plugins.tools.Iconize");
_2.require("dojox.drawing.plugins._Plugin");
_3.drawing.plugins.tools.Iconize=_3.drawing.util.oo.declare(_3.drawing.plugins._Plugin,function(_4){
},{onClick:function(){
var _5;
for(var nm in this.stencils.stencils){
if(this.stencils.stencils[nm].shortType=="path"){
_5=this.stencils.stencils[nm];
break;
}
}
if(_5){
this.makeIcon(_5.points);
}
},makeIcon:function(p){
var _6=function(n){
return Number(n.toFixed(1));
};
var x=10000;
var y=10000;
p.forEach(function(pt){
if(pt.x!==undefined&&!isNaN(pt.x)){
x=Math.min(x,pt.x);
y=Math.min(y,pt.y);
}
});
var _7=0;
var _8=0;
p.forEach(function(pt){
if(pt.x!==undefined&&!isNaN(pt.x)){
pt.x=_6(pt.x-x);
pt.y=_6(pt.y-y);
_7=Math.max(_7,pt.x);
_8=Math.max(_8,pt.y);
}
});
var s=60;
var m=20;
p.forEach(function(pt){
pt.x=_6(pt.x/_7)*s+m;
pt.y=_6(pt.y/_8)*s+m;
});
var _9="[\n";
_2.forEach(p,function(pt,i){
_9+="{\t";
if(pt.t){
_9+="t:'"+pt.t+"'";
}
if(pt.x!==undefined&&!isNaN(pt.x)){
if(pt.t){
_9+=", ";
}
_9+="x:"+pt.x+",\t\ty:"+pt.y;
}
_9+="\t}";
if(i!=p.length-1){
_9+=",";
}
_9+="\n";
});
_9+="]";
var n=_2.byId("data");
if(n){
n.value=_9;
}
}});
_3.drawing.plugins.tools.Iconize.setup={name:"dojox.drawing.plugins.tools.Iconize",tooltip:"Iconize Tool",iconClass:"iconPan"};
_3.drawing.register(_3.drawing.plugins.tools.Iconize.setup,"plugin");
});
