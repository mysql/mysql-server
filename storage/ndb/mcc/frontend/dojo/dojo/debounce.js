/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/debounce",[],function(){
return function(cb,_1){
var _2;
return function(){
if(_2){
clearTimeout(_2);
}
var _3=this;
var a=arguments;
_2=setTimeout(function(){
cb.apply(_3,a);
},_1);
};
};
});
