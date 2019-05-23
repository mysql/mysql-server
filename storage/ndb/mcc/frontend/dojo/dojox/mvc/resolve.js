//>>built
define("dojox/mvc/resolve",["dojo/_base/lang","dijit/registry","dojo/Stateful"],function(_1,_2){
var _3=function(_4,_5){
if(typeof _4=="string"){
var _6=_4.match(/^(expr|rel|widget):(.*)$/)||[];
try{
if(_6[1]=="rel"){
_4=_1.getObject(_6[2]||"",false,_5);
}else{
if(_6[1]=="widget"){
_4=_2.byId(_6[2]);
}else{
_4=_1.getObject(_6[2]||_4,false,_5);
}
}
}
catch(e){
}
}
return _4;
};
return _1.setObject("dojox.mvc.resolve",_3);
});
