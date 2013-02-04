//>>built
define("dojox/gfx3d/scheduler",["dojo/_base/lang","dojo/_base/array","dojo/_base/declare","./_base","./vector"],function(_1,_2,_3,_4,_5){
_4.scheduler={zOrder:function(_6,_7){
_7=_7?_7:_4.scheduler.order;
_6.sort(function(a,b){
return _7(b)-_7(a);
});
return _6;
},bsp:function(_8,_9){
_9=_9?_9:_4.scheduler.outline;
var p=new _4.scheduler.BinarySearchTree(_8[0],_9);
_2.forEach(_8.slice(1),function(_a){
p.add(_a,_9);
});
return p.iterate(_9);
},order:function(it){
return it.getZOrder();
},outline:function(it){
return it.getOutline();
}};
var _b=_3("dojox.gfx3d.scheduler.BinarySearchTree",null,{constructor:function(_c,_d){
this.plus=null;
this.minus=null;
this.object=_c;
var o=_d(_c);
this.orient=o[0];
this.normal=_5.normalize(o);
},add:function(_e,_f){
var _10=0.5,o=_f(_e),v=_5,n=this.normal,a=this.orient,_b=_4.scheduler.BinarySearchTree;
if(_2.every(o,function(_11){
return Math.floor(_10+v.dotProduct(n,v.substract(_11,a)))<=0;
})){
if(this.minus){
this.minus.add(_e,_f);
}else{
this.minus=new _b(_e,_f);
}
}else{
if(_2.every(o,function(_12){
return Math.floor(_10+v.dotProduct(n,v.substract(_12,a)))>=0;
})){
if(this.plus){
this.plus.add(_e,_f);
}else{
this.plus=new _b(_e,_f);
}
}else{
throw "The case: polygon cross siblings' plate is not implemented yet";
}
}
},iterate:function(_13){
var _14=0.5;
var v=_5;
var _15=[];
var _16=null;
var _17={x:0,y:0,z:-10000};
if(Math.floor(_14+v.dotProduct(this.normal,v.substract(_17,this.orient)))<=0){
_16=[this.plus,this.minus];
}else{
_16=[this.minus,this.plus];
}
if(_16[0]){
_15=_15.concat(_16[0].iterate());
}
_15.push(this.object);
if(_16[1]){
_15=_15.concat(_16[1].iterate());
}
return _15;
}});
_4.drawer={conservative:function(_18,_19,_1a){
_2.forEach(this.objects,function(_1b){
_1b.destroy();
});
_2.forEach(_19,function(_1c){
_1c.draw(_1a.lighting);
});
},chart:function(_1d,_1e,_1f){
_2.forEach(this.todos,function(_20){
_20.draw(_1f.lighting);
});
}};
var api={scheduler:_4.scheduler,drawer:_4.drawer,BinarySearchTree:_b};
return api;
});
