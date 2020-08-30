//>>built
define("dojox/validate/_base",["dojo/_base/lang","dojo/regexp","dojo/number","./regexp"],function(_1,_2,_3,_4){
var _5=_1.getObject("dojox.validate",true);
_5.isText=function(_6,_7){
_7=(typeof _7=="object")?_7:{};
if(/^\s*$/.test(_6)){
return false;
}
if(typeof _7.length=="number"&&_7.length!=_6.length){
return false;
}
if(typeof _7.minlength=="number"&&_7.minlength>_6.length){
return false;
}
if(typeof _7.maxlength=="number"&&_7.maxlength<_6.length){
return false;
}
return true;
};
_5._isInRangeCache={};
_5.isInRange=function(_8,_9){
_8=_3.parse(_8,_9);
if(isNaN(_8)){
return false;
}
_9=(typeof _9=="object")?_9:{};
var _a=(typeof _9.max=="number")?_9.max:Infinity,_b=(typeof _9.min=="number")?_9.min:-Infinity,_c=(typeof _9.decimal=="string")?_9.decimal:".",_d=_5._isInRangeCache,_e=_8+"max"+_a+"min"+_b+"dec"+_c;
if(typeof _d[_e]!="undefined"){
return _d[_e];
}
_d[_e]=!(_8<_b||_8>_a);
return _d[_e];
};
_5.isNumberFormat=function(_f,_10){
var re=new RegExp("^"+_4.numberFormat(_10)+"$","i");
return re.test(_f);
};
_5.isValidLuhn=function(_11){
var sum=0,_12,_13;
if(!_1.isString(_11)){
_11=String(_11);
}
_11=_11.replace(/[- ]/g,"");
_12=_11.length%2;
for(var i=0;i<_11.length;i++){
_13=parseInt(_11.charAt(i));
if(i%2==_12){
_13*=2;
}
if(_13>9){
_13-=9;
}
sum+=_13;
}
return !(sum%10);
};
return _5;
});
