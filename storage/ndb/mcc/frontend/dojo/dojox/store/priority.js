//>>built
define("dojox/store/priority",["dojo/_base/lang","dojo/Deferred","dojo/when"],function(_1,_2,_3){
var _4=[];
var _5=0;
function _6(){
for(var _7=_4.length-1;_7>=_5;_7--){
var _8=_4[_7];
var _9=_8&&_8[_8.length-1];
if(_9){
_8.pop();
_5++;
try{
_9.executor(function(){
_5--;
_6();
});
}
catch(e){
_9.def.reject(e);
_6();
}
}
}
};
function _a(){
var _b=new _2();
return {promise:{total:{then:function(_c,_d){
return _b.then(function(_e){
return _e.results.total;
}).then(_c,_d);
}},forEach:function(_f,_10){
return _b.then(function(_11){
return _11.results.forEach(_f,_10);
});
},map:function(_12,_13){
return _b.then(function(_14){
return _14.results.map(_12,_13);
});
},filter:function(_15,_16){
return _b.then(function(_17){
return _17.results.filter(_15,_16);
});
},then:function(_18,_19){
return _b.then(function(_1a){
return _3(_1a.results,_18,_19);
});
}},resolve:_b.resolve,reject:_b.reject};
};
return function(_1b,_1c){
_1c=_1c||{};
var _1d=_1.delegate(_1b);
["add","put","query","remove","get"].forEach(function(_1e){
var _1f=_1b[_1e];
if(_1f){
_1d[_1e]=function(_20,_21){
_21=_21||{};
var def,_22;
if(_21.immediate){
return _1f.call(_1b,_20,_21);
}
_21.immediate=true;
if(_1e==="query"){
_22=function(_23){
var _24=_1f.call(_1b,_20,_21);
def.resolve({results:_24});
_3(_24,_23,_23);
};
def=_a();
}else{
_22=function(_25){
_3(_1f.call(_1b,_20,_21),function(_26){
def.resolve(_26);
_25();
},function(_27){
def.reject(_27);
_25();
});
};
def=new _2();
}
var _28=_21.priority>-1?_21.priority:_1c.priority>-1?_1c.priority:4;
(_4[_28]||(_4[_28]=[])).push({executor:_22,def:def});
_6();
return def.promise;
};
}
});
return _1d;
};
});
