//>>built
define("dojox/mvc/getPlainValue",["dojo/_base/array","dojo/_base/lang","dojo/Stateful"],function(_1,_2,_3){
var _4={getType:function(v){
return _2.isArray(v)?"array":v!=null&&{}.toString.call(v)=="[object Object]"?"object":"value";
},getPlainArray:function(a){
return _1.map(a,function(_5){
return _6(_5,this);
},this);
},getPlainObject:function(o){
var _7={};
for(var s in o){
if(!(s in _3.prototype)&&s!="_watchCallbacks"){
_7[s]=_6(o[s],this);
}
}
return _7;
},getPlainValue:function(v){
return v;
}};
var _6=function(_8,_9){
return (_9||_6)["getPlain"+(_9||_6).getType(_8).replace(/^[a-z]/,function(c){
return c.toUpperCase();
})](_8);
};
return _2.setObject("dojox.mvc.getPlainValue",_2.mixin(_6,_4));
});
