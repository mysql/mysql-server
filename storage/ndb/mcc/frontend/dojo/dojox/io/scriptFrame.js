//>>built
define("dojox/io/scriptFrame",["dojo/main","dojo/io/script","dojo/io/iframe"],function(_1,_2,_3){
_1.deprecated("dojox.io.scriptFrame","dojo.io.script now supports parallel requests without dojox.io.scriptFrame","2.0");
_1.getObject("io.scriptFrame",true,dojox);
dojox.io.scriptFrame={_waiters:{},_loadedIds:{},_getWaiters:function(_4){
return this._waiters[_4]||(this._waiters[_4]=[]);
},_fixAttachUrl:function(_5){
},_loaded:function(_6){
var _7=this._getWaiters(_6);
this._loadedIds[_6]=true;
this._waiters[_6]=null;
for(var i=0;i<_7.length;i++){
var _8=_7[i];
_8.frameDoc=_3.doc(_1.byId(_6));
_2.attach(_8.id,_8.url,_8.frameDoc);
}
}};
var _9=_2._canAttach;
var _a=dojox.io.scriptFrame;
_2._canAttach=function(_b){
var _c=_b.args.frameDoc;
if(_c&&_1.isString(_c)){
var _d=_1.byId(_c);
var _e=_a._getWaiters(_c);
if(!_d){
_e.push(_b);
_3.create(_c,dojox._scopeName+".io.scriptFrame._loaded('"+_c+"');");
}else{
if(_a._loadedIds[_c]){
_b.frameDoc=_3.doc(_d);
this.attach(_b.id,_b.url,_b.frameDoc);
}else{
_e.push(_b);
}
}
return false;
}else{
return _9.apply(this,arguments);
}
};
return dojox.io.scriptFrame;
});
