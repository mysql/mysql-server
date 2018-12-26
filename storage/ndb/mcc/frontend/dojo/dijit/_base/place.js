//>>built
define("dijit/_base/place",["dojo/_base/array","dojo/_base/lang","dojo/window","../place",".."],function(_1,_2,_3,_4,_5){
_5.getViewport=function(){
return _3.getBox();
};
_5.placeOnScreen=_4.at;
_5.placeOnScreenAroundElement=function(_6,_7,_8,_9){
var _a;
if(_2.isArray(_8)){
_a=_8;
}else{
_a=[];
for(var _b in _8){
_a.push({aroundCorner:_b,corner:_8[_b]});
}
}
return _4.around(_6,_7,_a,true,_9);
};
_5.placeOnScreenAroundNode=_5.placeOnScreenAroundElement;
_5.placeOnScreenAroundRectangle=_5.placeOnScreenAroundElement;
_5.getPopupAroundAlignment=function(_c,_d){
var _e={};
_1.forEach(_c,function(_f){
var ltr=_d;
switch(_f){
case "after":
_e[_d?"BR":"BL"]=_d?"BL":"BR";
break;
case "before":
_e[_d?"BL":"BR"]=_d?"BR":"BL";
break;
case "below-alt":
ltr=!ltr;
case "below":
_e[ltr?"BL":"BR"]=ltr?"TL":"TR";
_e[ltr?"BR":"BL"]=ltr?"TR":"TL";
break;
case "above-alt":
ltr=!ltr;
case "above":
default:
_e[ltr?"TL":"TR"]=ltr?"BL":"BR";
_e[ltr?"TR":"TL"]=ltr?"BR":"BL";
break;
}
});
return _e;
};
return _5;
});
