//>>built
define("dojox/drawing/manager/_registry",[],function(){
var _1={tool:{},stencil:{},drawing:{},plugin:{},button:{}};
return {register:function(_2,_3){
if(_3=="drawing"){
_1.drawing[_2.id]=_2;
}else{
if(_3=="tool"){
_1.tool[_2.name]=_2;
}else{
if(_3=="stencil"){
_1.stencil[_2.name]=_2;
}else{
if(_3=="plugin"){
_1.plugin[_2.name]=_2;
}else{
if(_3=="button"){
_1.button[_2.toolType]=_2;
}
}
}
}
}
},getRegistered:function(_4,id){
return id?_1[_4][id]:_1[_4];
}};
});
