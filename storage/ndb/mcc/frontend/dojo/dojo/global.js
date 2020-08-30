/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/global",function(){
if(typeof global!=="undefined"&&typeof global!=="function"){
return global;
}else{
if(typeof window!=="undefined"){
return window;
}else{
if(typeof self!=="undefined"){
return self;
}
}
}
return this;
});
