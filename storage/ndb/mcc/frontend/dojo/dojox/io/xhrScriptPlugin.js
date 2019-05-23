//>>built
define("dojox/io/xhrScriptPlugin",["dojo/_base/kernel","dojo/_base/window","dojo/io/script","dojox/io/xhrPlugins","dojox/io/scriptFrame"],function(_1,_2,_3,_4,_5){
_1.getObject("io.xhrScriptPlugin",true,dojox);
dojox.io.xhrScriptPlugin=function(_6,_7,_8){
_4.register("script",function(_9,_a){
return _a.sync!==true&&(_9=="GET"||_8)&&(_a.url.substring(0,_6.length)==_6);
},function(_b,_c,_d){
var _e=function(){
_c.callbackParamName=_7;
if(_1.body()){
_c.frameDoc="frame"+Math.random();
}
return _3.get(_c);
};
return (_8?_8(_e,true):_e)(_b,_c,_d);
});
};
return dojox.io.xhrScriptPlugin;
});
