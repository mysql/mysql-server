//>>built
define("dojox/grid/util",["../main","dojo/_base/lang","dojo/dom","dojo/_base/sniff"],function(_1,_2,_3,_4){
var _5=_2.getObject("grid.util",true,_1);
_5.na="...";
_5.rowIndexTag="gridRowIndex";
_5.gridViewTag="gridView";
_5.fire=function(ob,ev,_6){
function _7(_8,_9){
if(_8==null){
return null;
}
var _a=_9?"Width":"Height";
if(_8["scroll"+_a]>_8["client"+_a]){
return _8;
}else{
return _7(_8.parentNode,_9);
}
};
var _b,_c,_d,_e,_f;
if(_4("webkit")&&(ev=="focus")){
_f=ob.domNode?ob.domNode:ob;
_b=_7(_f,false);
if(_b){
_d=_b.scrollTop;
}
_c=_7(_f,true);
if(_c){
_e=_c.scrollLeft;
}
}
var fn=ob&&ev&&ob[ev];
var _10=fn&&(_6?fn.apply(ob,_6):ob[ev]());
if(_4("webkit")&&(ev=="focus")){
if(_b){
_b.scrollTop=_d;
}
if(_c){
_c.scrollLeft=_e;
}
}
return _10;
};
_5.setStyleHeightPx=function(_11,_12){
if(_12>=0){
var s=_11.style;
var v=_12+"px";
if(_11&&s["height"]!=v){
s["height"]=v;
}
}
};
_5.mouseEvents=["mouseover","mouseout","mousedown","mouseup","click","dblclick","contextmenu"];
_5.keyEvents=["keyup","keydown","keypress"];
_5.funnelEvents=function(_13,_14,_15,_16){
var _17=(_16?_16:_5.mouseEvents.concat(_5.keyEvents));
for(var i=0,l=_17.length;i<l;i++){
_14.connect(_13,"on"+_17[i],_15);
}
};
_5.removeNode=function(_18){
_18=_3.byId(_18);
_18&&_18.parentNode&&_18.parentNode.removeChild(_18);
return _18;
};
_5.arrayCompare=function(inA,inB){
for(var i=0,l=inA.length;i<l;i++){
if(inA[i]!=inB[i]){
return false;
}
}
return (inA.length==inB.length);
};
_5.arrayInsert=function(_19,_1a,_1b){
if(_19.length<=_1a){
_19[_1a]=_1b;
}else{
_19.splice(_1a,0,_1b);
}
};
_5.arrayRemove=function(_1c,_1d){
_1c.splice(_1d,1);
};
_5.arraySwap=function(_1e,inI,inJ){
var _1f=_1e[inI];
_1e[inI]=_1e[inJ];
_1e[inJ]=_1f;
};
return _5;
});
