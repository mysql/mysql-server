//>>built
define("dojox/mobile/i18n",["dojo/_base/lang","dojo/i18n","dijit/_WidgetBase"],function(_1,_2,_3){
var _4=_1.getObject("dojox.mobile.i18n",true);
_4.load=function(_5,_6,_7){
return _4.registerBundle(_2.getLocalization(_5,_6,_7));
};
_4.registerBundle=function(_8){
if(!_4.bundle){
_4.bundle=[];
}
return _1.mixin(_4.bundle,_8);
};
_1.extend(_3,{mblNoConv:false,_cv:function(s){
if(this.mblNoConv||!_4.bundle){
return s;
}
return _4.bundle[_1.trim(s)]||s;
}});
return _4;
});
