//>>built
define("dijit/_base/manager",["dojo/_base/array","dojo/_base/config","../registry",".."],function(_1,_2,_3,_4){
_1.forEach(["byId","getUniqueId","findWidgets","_destroyAll","byNode","getEnclosingWidget"],function(_5){
_4[_5]=_3[_5];
});
_4.defaultDuration=_2["defaultDuration"]||200;
return _4;
});
