//>>built
define("dojox/validate/creditCard",["dojo/_base/lang","./_base"],function(_1,_2){
_2._cardInfo={"mc":"5[1-5][0-9]{14}","ec":"5[1-5][0-9]{14}","vi":"4(?:[0-9]{12}|[0-9]{15})","ax":"3[47][0-9]{13}","dc":"3(?:0[0-5][0-9]{11}|[68][0-9]{12})","bl":"3(?:0[0-5][0-9]{11}|[68][0-9]{12})","di":"6011[0-9]{12}","jcb":"(?:3[0-9]{15}|(2131|1800)[0-9]{11})","er":"2(?:014|149)[0-9]{11}"};
_2.isValidCreditCard=function(_3,_4){
return ((_4.toLowerCase()=="er"||_2.isValidLuhn(_3))&&_2.isValidCreditCardNumber(_3,_4.toLowerCase()));
};
_2.isValidCreditCardNumber=function(_5,_6){
_5=String(_5).replace(/[- ]/g,"");
var _7=_2._cardInfo,_8=[];
if(_6){
var _9="^"+_7[_6.toLowerCase()]+"$";
return _9?!!_5.match(_9):false;
}
for(var p in _7){
if(_5.match("^"+_7[p]+"$")){
_8.push(p);
}
}
return _8.length?_8.join("|"):false;
};
_2.isValidCvv=function(_a,_b){
if(!_1.isString(_a)){
_a=String(_a);
}
var _c;
switch(_b.toLowerCase()){
case "mc":
case "ec":
case "vi":
case "di":
_c="###";
break;
case "ax":
_c="####";
break;
}
return !!_c&&_a.length&&_2.isNumberFormat(_a,{format:_c});
};
return _2;
});
