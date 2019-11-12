/*
	Copyright (c) 2004-2012, The Dojo Foundation All Rights Reserved.
	Available via Academic Free License >= 2.1 OR the modified BSD license.
	see: http://dojotoolkit.org/license for details
*/

//>>built
define("dojo/request/script",["module","./watch","./util","../_base/kernel","../_base/array","../_base/lang","../on","../dom","../dom-construct","../has","../_base/window"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9,_a){
_9.add("script-readystatechange",function(_b,_c){
var _d=_c.createElement("script");
return typeof _d["onreadystatechange"]!=="undefined"&&(typeof _b["opera"]==="undefined"||_b["opera"].toString()!=="[object Opera]");
});
var _e=_1.id.replace(/[\/\.\-]/g,"_"),_f=0,_10=_9("script-readystatechange")?"readystatechange":"load",_11=/complete|loaded/,_12=_4.global[_e+"_callbacks"]={},_13=[];
function _14(id,url,_15){
var doc=(_15||_a.doc),_16=doc.createElement("script");
_16.type="text/javascript";
_16.src=url;
_16.id=id;
_16.async=true;
_16.charset="utf-8";
return doc.getElementsByTagName("head")[0].appendChild(_16);
};
function _17(id,_18,_19){
_8.destroy(_7.byId(id,_18));
if(_12[id]){
if(_19){
_12[id]=function(){
delete _12[id];
};
}else{
delete _12[id];
}
}
};
function _1a(dfd){
var _1b=dfd.response.options,_1c=_1b.ioArgs?_1b.ioArgs.frameDoc:_1b.frameDoc;
_13.push({id:dfd.id,frameDoc:_1c});
if(_1b.ioArgs){
_1b.ioArgs.frameDoc=null;
}
_1b.frameDoc=null;
};
function _1d(dfd,_1e){
if(dfd.canDelete){
_1f._remove(dfd.id,_1e.options.frameDoc,true);
}
};
function _20(_21){
if(_13&&_13.length){
_5.forEach(_13,function(_22){
_1f._remove(_22.id,_22.frameDoc);
_22.frameDoc=null;
});
_13=[];
}
return _21.options.jsonp?!_21.data:true;
};
function _23(_24){
return !!this.scriptLoaded;
};
function _25(_26){
var _27=_26.options.checkString;
return _27&&eval("typeof("+_27+") !== \"undefined\"");
};
function _28(_29,_2a){
if(this.canDelete){
_1a(this);
}
if(_2a){
this.reject(_2a);
}else{
this.resolve(_29);
}
};
function _1f(url,_2b,_2c){
var _2d=_3.parseArgs(url,_3.deepCopy({},_2b));
url=_2d.url;
_2b=_2d.options;
var dfd=_3.deferred(_2d,_1d,_20,_2b.jsonp?null:(_2b.checkString?_25:_23),_28);
_6.mixin(dfd,{id:_e+(_f++),canDelete:false});
if(_2b.jsonp){
var _2e=new RegExp("[?&]"+_2b.jsonp+"=");
if(!_2e.test(url)){
url+=(~url.indexOf("?")?"&":"?")+_2b.jsonp+"="+(_2b.frameDoc?"parent.":"")+_e+"_callbacks."+dfd.id;
}
dfd.canDelete=true;
_12[dfd.id]=function(_2f){
_2d.data=_2f;
dfd.handleResponse(_2d);
};
}
if(_3.notify){
_3.notify.emit("send",_2d,dfd.promise.cancel);
}
if(!_2b.canAttach||_2b.canAttach(dfd)){
var _30=_1f._attach(dfd.id,url,_2b.frameDoc);
if(!_2b.jsonp&&!_2b.checkString){
var _31=on(_30,_10,function(evt){
if(evt.type==="load"||_11.test(_30.readyState)){
_31.remove();
dfd.scriptLoaded=evt;
}
});
}
}
_2(dfd);
return _2c?dfd:dfd.promise;
};
_1f.get=_1f;
_1f._attach=_14;
_1f._remove=_17;
_1f._callbacksProperty=_e+"_callbacks";
return _1f;
});
