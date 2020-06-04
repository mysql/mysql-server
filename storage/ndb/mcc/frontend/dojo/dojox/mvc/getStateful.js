//>>built
define("dojox/mvc/getStateful",["dojo/_base/array","dojo/_base/lang","dojo/Stateful","./StatefulArray"],function(_1,_2,_3,_4){
var _5={getType:function(v){
return _2.isArray(v)?"array":v!=null&&{}.toString.call(v)=="[object Object]"?"object":"value";
},getStatefulArray:function(a){
return new _4(_1.map(a,function(_6){
return _7(_6,this);
},this));
},getStatefulObject:function(o){
var _8=new _3();
for(var s in o){
_8[s]=_7(o[s],this);
}
return _8;
},getStatefulValue:function(v){
return v;
}};
var _7=function(_9,_a){
return (_a||_7)["getStateful"+(_a||_7).getType(_9).replace(/^[a-z]/,function(c){
return c.toUpperCase();
})](_9);
};
return _2.setObject("dojox.mvc.getStateful",_2.mixin(_7,_5));
});
