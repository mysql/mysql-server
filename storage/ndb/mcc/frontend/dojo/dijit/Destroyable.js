//>>built
define("dijit/Destroyable",["dojo/_base/array","dojo/aspect","dojo/_base/declare"],function(_1,_2,_3){
return _3("dijit.Destroyable",null,{destroy:function(_4){
this._destroyed=true;
},own:function(){
var _5=["destroyRecursive","destroy","remove"];
_1.forEach(arguments,function(_6){
var _7;
var _8=_2.before(this,"destroy",function(_9){
_6[_7](_9);
});
var _a=[];
function _b(){
_8.remove();
_1.forEach(_a,function(_c){
_c.remove();
});
};
if(_6.then){
_7="cancel";
_6.then(_b,_b);
}else{
_1.forEach(_5,function(_d){
if(typeof _6[_d]==="function"){
if(!_7){
_7=_d;
}
_a.push(_2.after(_6,_d,_b,true));
}
});
}
},this);
return arguments;
}});
});
