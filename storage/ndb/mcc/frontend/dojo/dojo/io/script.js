/*
	Copyright (c) 2004-2011, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/io/script",["../main"],function(_1){
_1.getObject("io",true,_1);
var _2=_1.isIE?"onreadystatechange":"load",_3=/complete|loaded/;
_1.io.script={get:function(_4){
var _5=this._makeScriptDeferred(_4);
var _6=_5.ioArgs;
_1._ioAddQueryToUrl(_6);
_1._ioNotifyStart(_5);
if(this._canAttach(_6)){
var _7=this.attach(_6.id,_6.url,_4.frameDoc);
if(!_6.jsonp&&!_6.args.checkString){
var _8=_1.connect(_7,_2,function(_9){
if(_9.type=="load"||_3.test(_7.readyState)){
_1.disconnect(_8);
_6.scriptLoaded=_9;
}
});
}
}
_1._ioWatch(_5,this._validCheck,this._ioCheck,this._resHandle);
return _5;
},attach:function(id,_a,_b){
var _c=(_b||_1.doc);
var _d=_c.createElement("script");
_d.type="text/javascript";
_d.src=_a;
_d.id=id;
_d.async=true;
_d.charset="utf-8";
return _c.getElementsByTagName("head")[0].appendChild(_d);
},remove:function(id,_e){
_1.destroy(_1.byId(id,_e));
if(this["jsonp_"+id]){
delete this["jsonp_"+id];
}
},_makeScriptDeferred:function(_f){
var dfd=_1._ioSetArgs(_f,this._deferredCancel,this._deferredOk,this._deferredError);
var _10=dfd.ioArgs;
_10.id=_1._scopeName+"IoScript"+(this._counter++);
_10.canDelete=false;
_10.jsonp=_f.callbackParamName||_f.jsonp;
if(_10.jsonp){
_10.query=_10.query||"";
if(_10.query.length>0){
_10.query+="&";
}
_10.query+=_10.jsonp+"="+(_f.frameDoc?"parent.":"")+_1._scopeName+".io.script.jsonp_"+_10.id+"._jsonpCallback";
_10.frameDoc=_f.frameDoc;
_10.canDelete=true;
dfd._jsonpCallback=this._jsonpCallback;
this["jsonp_"+_10.id]=dfd;
}
return dfd;
},_deferredCancel:function(dfd){
dfd.canceled=true;
if(dfd.ioArgs.canDelete){
_1.io.script._addDeadScript(dfd.ioArgs);
}
},_deferredOk:function(dfd){
var _11=dfd.ioArgs;
if(_11.canDelete){
_1.io.script._addDeadScript(_11);
}
return _11.json||_11.scriptLoaded||_11;
},_deferredError:function(_12,dfd){
if(dfd.ioArgs.canDelete){
if(_12.dojoType=="timeout"){
_1.io.script.remove(dfd.ioArgs.id,dfd.ioArgs.frameDoc);
}else{
_1.io.script._addDeadScript(dfd.ioArgs);
}
}
return _12;
},_deadScripts:[],_counter:1,_addDeadScript:function(_13){
_1.io.script._deadScripts.push({id:_13.id,frameDoc:_13.frameDoc});
_13.frameDoc=null;
},_validCheck:function(dfd){
var _14=_1.io.script;
var _15=_14._deadScripts;
if(_15&&_15.length>0){
for(var i=0;i<_15.length;i++){
_14.remove(_15[i].id,_15[i].frameDoc);
_15[i].frameDoc=null;
}
_1.io.script._deadScripts=[];
}
return true;
},_ioCheck:function(dfd){
var _16=dfd.ioArgs;
if(_16.json||(_16.scriptLoaded&&!_16.args.checkString)){
return true;
}
var _17=_16.args.checkString;
return _17&&eval("typeof("+_17+") != 'undefined'");
},_resHandle:function(dfd){
if(_1.io.script._ioCheck(dfd)){
dfd.callback(dfd);
}else{
dfd.errback(new Error("inconceivable dojo.io.script._resHandle error"));
}
},_canAttach:function(_18){
return true;
},_jsonpCallback:function(_19){
this.ioArgs.json=_19;
}};
return _1.io.script;
});
