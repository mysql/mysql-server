/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/on/debounce",["../debounce","../on","./asyncEventListener"],function(_1,on,_2){
return function(_3,_4){
return function(_5,_6){
return on(_5,_3,_2(_1(_6,_4)));
};
};
});
