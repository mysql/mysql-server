//>>built
define("dijit/_base/popup",["dojo/dom-class","../popup","../BackgroundIframe"],function(_1,_2){
var _3=_2._createWrapper;
_2._createWrapper=function(_4){
if(!_4.declaredClass){
_4={_popupWrapper:(_4.parentNode&&_1.contains(_4.parentNode,"dijitPopup"))?_4.parentNode:null,domNode:_4,destroy:function(){
}};
}
return _3.call(this,_4);
};
var _5=_2.open;
_2.open=function(_6){
if(_6.orient&&typeof _6.orient!="string"&&!("length" in _6.orient)){
var _7=[];
for(var _8 in _6.orient){
_7.push({aroundCorner:_8,corner:_6.orient[_8]});
}
_6.orient=_7;
}
return _5.call(this,_6);
};
return _2;
});
