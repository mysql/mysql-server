/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/throttle",[],function(){
return function(cb,_1){
var _2=true;
return function(){
if(!_2){
return;
}
_2=false;
cb.apply(this,arguments);
setTimeout(function(){
_2=true;
},_1);
};
};
});
