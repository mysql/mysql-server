//>>built
define("dojox/color/NeutralColorModel",["dojo/_base/array","dojo/_base/declare","./SimpleColorModel"],function(_1,_2,_3){
return _2("dojox.color.NeutralColorModel",_3,{_min:0,_max:0,_e:0,constructor:function(_4,_5){
},initialize:function(_6,_7){
var _8=[];
var _9=0;
var _a=100000000;
var _b=-_a;
_1.forEach(_6,function(_c){
var _d=_7(_c);
_a=Math.min(_a,_d);
_b=Math.max(_b,_d);
_9+=_d;
_8.push(_d);
});
_8.sort(function(a,b){
return a-b;
});
var _e=this.computeNeutral(_a,_b,_9,_8);
this._min=_a;
this._max=_b;
if(this._min==this._max||_e==this._min){
this._e=-1;
}else{
this._e=Math.log(0.5)/Math.log((_e-this._min)/(this._max-this._min));
}
},computeNeutral:function(_f,max,sum,_10){
},getNormalizedValue:function(_11){
if(this._e<0){
return 0;
}
_11=(_11-this._min)/(this._max-this._min);
return Math.pow(_11,this._e);
}});
});
