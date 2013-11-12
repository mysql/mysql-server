//>>built
define("dojox/io/xhrWindowNamePlugin",["dojo/_base/kernel","dojo/_base/json","dojo/_base/xhr","dojox/io/xhrPlugins","dojox/io/windowName","dojox/io/httpParse","dojox/secure/capability"],function(_1,_2,_3,_4,_5,_6,_7){
_1.getObject("io.xhrWindowNamePlugin",true,dojox);
dojox.io.xhrWindowNamePlugin=function(_8,_9,_a){
_4.register("windowName",function(_b,_c){
return _c.sync!==true&&(_b=="GET"||_b=="POST"||_9)&&(_c.url.substring(0,_8.length)==_8);
},function(_d,_e,_f){
var _10=_5.send;
var _11=_e.load;
_e.load=undefined;
var dfd=(_9?_9(_10,true):_10)(_d,_e,_f);
dfd.addCallback(function(_12){
var _13=dfd.ioArgs;
_13.xhr={getResponseHeader:function(_14){
return _1.queryToObject(_13.hash.match(/[^#]*$/)[0])[_14];
}};
if(_13.handleAs=="json"){
if(!_a){
_7.validate(_12,["Date"],{});
}
return _1.fromJson(_12);
}
return _1._contentHandlers[_13.handleAs||"text"]({responseText:_12});
});
_e.load=_11;
if(_11){
dfd.addCallback(_11);
}
return dfd;
});
};
return dojox.io.xhrWindowNamePlugin;
});
