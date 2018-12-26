//>>built
define("dojox/grid/util",["../main","dojo/_base/lang","dojo/dom"],function(_1,_2,_3){
var _4=_2.getObject("grid.util",true,_1);
_4.na="...";
_4.rowIndexTag="gridRowIndex";
_4.gridViewTag="gridView";
_4.fire=function(ob,ev,_5){
var fn=ob&&ev&&ob[ev];
return fn&&(_5?fn.apply(ob,_5):ob[ev]());
};
_4.setStyleHeightPx=function(_6,_7){
if(_7>=0){
var s=_6.style;
var v=_7+"px";
if(_6&&s["height"]!=v){
s["height"]=v;
}
}
};
_4.mouseEvents=["mouseover","mouseout","mousedown","mouseup","click","dblclick","contextmenu"];
_4.keyEvents=["keyup","keydown","keypress"];
_4.funnelEvents=function(_8,_9,_a,_b){
var _c=(_b?_b:_4.mouseEvents.concat(_4.keyEvents));
for(var i=0,l=_c.length;i<l;i++){
_9.connect(_8,"on"+_c[i],_a);
}
};
_4.removeNode=function(_d){
_d=_3.byId(_d);
_d&&_d.parentNode&&_d.parentNode.removeChild(_d);
return _d;
};
_4.arrayCompare=function(_e,_f){
for(var i=0,l=_e.length;i<l;i++){
if(_e[i]!=_f[i]){
return false;
}
}
return (_e.length==_f.length);
};
_4.arrayInsert=function(_10,_11,_12){
if(_10.length<=_11){
_10[_11]=_12;
}else{
_10.splice(_11,0,_12);
}
};
_4.arrayRemove=function(_13,_14){
_13.splice(_14,1);
};
_4.arraySwap=function(_15,inI,inJ){
var _16=_15[inI];
_15[inI]=_15[inJ];
_15[inJ]=_16;
};
return _1.grid.util;
});
