/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/on/asyncEventListener",["../on","../has","../_base/window","../dom-construct","../domReady!"],function(on,_1,_2,_3){
var _4,_5,_6=false;
if(_3){
_4=_3.create("input",{type:"button"},_2.body()),on.once(_4,"click",function(e){
_5=e;
});
_4.click();
try{
_6=_5.clientX===undefined;
}
catch(e){
_6=true;
}
finally{
_3.destroy(_4);
}
}
_1.add("native-async-event-support",!_6);
function _7(_8){
var _9={},i;
for(i in _8){
_9[i]=_8[i];
}
return _9;
};
return function(_a){
if(_6){
return function(e){
_a.call(this,_7(e));
};
}
return _a;
};
});
