//>>built
define("dojox/lang/oo/mixin",["dojo","dijit","dojox","dojo/require!dojox/lang/oo/Filter,dojox/lang/oo/Decorator"],function(_1,_2,_3){
_1.provide("dojox.lang.oo.mixin");
_1.experimental("dojox.lang.oo.mixin");
_1.require("dojox.lang.oo.Filter");
_1.require("dojox.lang.oo.Decorator");
(function(){
var oo=_3.lang.oo,_4=oo.Filter,_5=oo.Decorator,_6={},_7=function(_8){
return _8;
},_9=function(_a,_b,_c){
return _b;
},_d=function(_e,_f,_10,_11){
_e[_f]=_10;
},_12={},_13=_1._extraNames,_14=_13.length,_15=oo.applyDecorator=function(_16,_17,_18,_19){
if(_18 instanceof _5){
var d=_18.decorator;
_18=_15(_16,_17,_18.value,_19);
return d(_17,_18,_19);
}
return _16(_17,_18,_19);
};
oo.__mixin=function(_1a,_1b,_1c,_1d,_1e){
var _1f,_20,_21,_22,_23,i;
for(_1f in _1b){
_21=_1b[_1f];
if(!(_1f in _6)||_6[_1f]!==_21){
_20=_1d(_1f,_1a,_1b,_21);
if(_20&&(!(_20 in _1a)||!(_20 in _6)||_6[_20]!==_21)){
_23=_1a[_20];
_22=_15(_1c,_20,_21,_23);
if(_23!==_22){
_1e(_1a,_20,_22,_23);
}
}
}
}
if(_14){
for(i=0;i<_14;++i){
_1f=_13[i];
_21=_1b[_1f];
if(!(_1f in _6)||_6[_1f]!==_21){
_20=_1d(_1f,_1a,_1b,_21);
if(_20&&(!(_20 in _1a)||!(_20 in _6)||_6[_20]!==_21)){
_23=_1a[_20];
_22=_15(_1c,_20,_21,_23);
if(_23!==_22){
_1e(_1a,_20,_22,_23);
}
}
}
}
}
return _1a;
};
oo.mixin=function(_24,_25){
var _26,_27,i=1,l=arguments.length;
for(;i<l;++i){
_25=arguments[i];
if(_25 instanceof _4){
_27=_25.filter;
_25=_25.bag;
}else{
_27=_7;
}
if(_25 instanceof _5){
_26=_25.decorator;
_25=_25.value;
}else{
_26=_9;
}
oo.__mixin(_24,_25,_26,_27,_d);
}
return _24;
};
})();
});
