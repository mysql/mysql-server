//>>built
define("dojox/fx/style",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/fx","dojo/fx","./_base","dojo/_base/array","dojo/dom","dojo/dom-style","dojo/dom-class","dojo/_base/connect"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a){
_1.experimental("dojox.fx.style");
var _b=function(_c){
return _6.map(_5._allowedProperties,function(_d){
return _c[_d];
});
};
var _e=function(_f,_10,_11){
_f=_7.byId(_f);
var cs=_8.getComputedStyle(_f);
var _12=_b(cs);
_1[(_11?"addClass":"removeClass")](_f,_10);
var _13=_b(cs);
_1[(_11?"removeClass":"addClass")](_f,_10);
var _14={},i=0;
_6.forEach(_5._allowedProperties,function(_15){
if(_12[i]!=_13[i]){
_14[_15]=parseInt(_13[i]);
}
i++;
});
return _14;
};
var _16={addClass:function(_17,_18,_19){
_17=_7.byId(_17);
var _1a=(function(n){
return function(){
_9.add(n,_18);
n.style.cssText=_1b;
};
})(_17);
var _1c=_e(_17,_18,true);
var _1b=_17.style.cssText;
var _1d=_3.animateProperty(_2.mixin({node:_17,properties:_1c},_19));
_a.connect(_1d,"onEnd",_1d,_1a);
return _1d;
},removeClass:function(_1e,_1f,_20){
_1e=_7.byId(_1e);
var _21=(function(n){
return function(){
_9.remove(n,_1f);
n.style.cssText=_22;
};
})(_1e);
var _23=_e(_1e,_1f);
var _22=_1e.style.cssText;
var _24=_3.animateProperty(_2.mixin({node:_1e,properties:_23},_20));
_a.connect(_24,"onEnd",_24,_21);
return _24;
},toggleClass:function(_25,_26,_27,_28){
if(typeof _27=="undefined"){
_27=!_9.contains(_25,_26);
}
return _5[(_27?"addClass":"removeClass")](_25,_26,_28);
},_allowedProperties:["width","height","left","top","backgroundColor","color","borderBottomWidth","borderTopWidth","borderLeftWidth","borderRightWidth","paddingLeft","paddingRight","paddingTop","paddingBottom","marginLeft","marginTop","marginRight","marginBottom","lineHeight","letterSpacing","fontSize"]};
_2.mixin(_5,_16);
return _16;
});
