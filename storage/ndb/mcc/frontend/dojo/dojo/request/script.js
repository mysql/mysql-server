/*
	Copyright (c) 2004-2016, The JS Foundation All Rights Reserved.
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
function _14(id,url,_15,_16){
var doc=(_15||_a.doc),_17=doc.createElement("script");
if(_16){
on.once(_17,"error",_16);
}
_17.type="text/javascript";
try{
_17.src=url;
}
catch(err){
_16&&_16(_17);
}
_17.id=id;
_17.async=true;
_17.charset="utf-8";
return doc.getElementsByTagName("head")[0].appendChild(_17);
};
function _18(id,_19,_1a){
_8.destroy(_7.byId(id,_19));
if(_12[id]){
if(_1a){
_12[id]=function(){
delete _12[id];
};
}else{
delete _12[id];
}
}
};
function _1b(dfd){
var _1c=dfd.response.options,_1d=_1c.ioArgs?_1c.ioArgs.frameDoc:_1c.frameDoc;
_13.push({id:dfd.id,frameDoc:_1d});
if(_1c.ioArgs){
_1c.ioArgs.frameDoc=null;
}
_1c.frameDoc=null;
};
function _1e(dfd,_1f){
if(dfd.canDelete){
_20._remove(dfd.id,_1f.options.frameDoc,true);
}
};
function _21(_22){
if(_13&&_13.length){
_5.forEach(_13,function(_23){
_20._remove(_23.id,_23.frameDoc);
_23.frameDoc=null;
});
_13=[];
}
return _22.options.jsonp?!_22.data:true;
};
function _24(_25){
return !!this.scriptLoaded;
};
function _26(_27){
var _28=_27.options.checkString;
return _28&&eval("typeof("+_28+") !== \"undefined\"");
};
function _29(_2a,_2b){
if(this.canDelete){
_1b(this);
}
if(_2b){
this.reject(_2b);
}else{
this.resolve(_2a);
}
};
function _20(url,_2c,_2d){
var _2e=_3.parseArgs(url,_3.deepCopy({},_2c));
url=_2e.url;
_2c=_2e.options;
var dfd=_3.deferred(_2e,_1e,_21,_2c.jsonp?null:(_2c.checkString?_26:_24),_29);
_6.mixin(dfd,{id:_e+(_f++),canDelete:false});
if(_2c.jsonp){
var _2f=new RegExp("[?&]"+_2c.jsonp+"=");
if(!_2f.test(url)){
url+=(~url.indexOf("?")?"&":"?")+_2c.jsonp+"="+(_2c.frameDoc?"parent.":"")+_e+"_callbacks."+dfd.id;
}
dfd.canDelete=true;
_12[dfd.id]=function(_30){
_2e.data=_30;
dfd.handleResponse(_2e);
};
}
if(_3.notify){
_3.notify.emit("send",_2e,dfd.promise.cancel);
}
if(!_2c.canAttach||_2c.canAttach(dfd)){
var _31=_20._attach(dfd.id,url,_2c.frameDoc,function(_32){
if(!(_32 instanceof Error)){
var _33=new Error("Error loading "+(_32.target?_32.target.src:"script"));
_33.source=_32;
_32=_33;
}
dfd.reject(_32);
_20._remove(dfd.id,_2c.frameDoc,true);
});
if(!_2c.jsonp&&!_2c.checkString){
var _34=on(_31,_10,function(evt){
if(evt.type==="load"||_11.test(_31.readyState)){
_34.remove();
dfd.scriptLoaded=evt;
}
});
}
}
_2(dfd);
return _2d?dfd:dfd.promise;
};
_20.get=_20;
_20._attach=_14;
_20._remove=_18;
_20._callbacksProperty=_e+"_callbacks";
return _20;
});
