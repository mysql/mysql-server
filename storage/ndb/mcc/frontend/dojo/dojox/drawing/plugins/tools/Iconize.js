//>>built
define("dojox/drawing/plugins/tools/Iconize",["dojo","../../util/oo","../_Plugin","../../manager/_registry"],function(_1,oo,_2,_3){
var _4=oo.declare(_2,function(_5){
},{onClick:function(){
var _6;
for(var nm in this.stencils.stencils){
if(this.stencils.stencils[nm].shortType=="path"){
_6=this.stencils.stencils[nm];
break;
}
}
if(_6){
this.makeIcon(_6.points);
}
},makeIcon:function(p){
var _7=function(n){
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
var _8=0;
var _9=0;
p.forEach(function(pt){
if(pt.x!==undefined&&!isNaN(pt.x)){
pt.x=_7(pt.x-x);
pt.y=_7(pt.y-y);
_8=Math.max(_8,pt.x);
_9=Math.max(_9,pt.y);
}
});
var s=60;
var m=20;
p.forEach(function(pt){
pt.x=_7(pt.x/_8)*s+m;
pt.y=_7(pt.y/_9)*s+m;
});
var _a="[\n";
_1.forEach(p,function(pt,i){
_a+="{\t";
if(pt.t){
_a+="t:'"+pt.t+"'";
}
if(pt.x!==undefined&&!isNaN(pt.x)){
if(pt.t){
_a+=", ";
}
_a+="x:"+pt.x+",\t\ty:"+pt.y;
}
_a+="\t}";
if(i!=p.length-1){
_a+=",";
}
_a+="\n";
});
_a+="]";
var n=_1.byId("data");
if(n){
n.value=_a;
}
}});
_4.setup={name:"dojox.drawing.plugins.tools.Iconize",tooltip:"Iconize Tool",iconClass:"iconPan"};
_1.setObject("dojox.drawing.plugins.tools.Iconize",_4);
_3.register(_4.setup,"plugin");
return _4;
});
