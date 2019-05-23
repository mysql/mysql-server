/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/node",["./_base/kernel","./has","require"],function(_1,_2,_3){
var _4=_1.global.require&&_1.global.require.nodeRequire;
if(!_4){
throw new Error("Cannot find the Node.js require");
}
var _5=_4("module");
return {load:function(id,_6,_7){
if(_5._findPath&&_5._nodeModulePaths){
var _8=_5._findPath(id,_5._nodeModulePaths(_6.toUrl(".")));
if(_8!==false){
id=_8;
}
}
var _9=define,_a;
define=undefined;
try{
_a=_4(id);
}
finally{
define=_9;
}
_7(_a);
},normalize:function(id,_b){
if(id.charAt(0)==="."){
id=_3.toUrl(_b("./"+id));
}
return id;
}};
});
