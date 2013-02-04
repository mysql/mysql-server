//>>built
define("dojox/atom/io/Connection",["dojo/_base/kernel","dojo/_base/xhr","dojo/_base/window","./model","dojo/_base/declare"],function(_1,_2,_3,_4){
return _1.declare("dojox.atom.io.Connection",null,{constructor:function(_5,_6){
this.sync=_5;
this.preventCache=_6;
},preventCache:false,alertsEnabled:false,getFeed:function(_7,_8,_9,_a){
this._getXmlDoc(_7,"feed",new _4.Feed(),_4._Constants.ATOM_NS,_8,_9,_a);
},getService:function(_b,_c,_d,_e){
this._getXmlDoc(_b,"service",new _4.Service(_b),_4._Constants.APP_NS,_c,_d,_e);
},getEntry:function(_f,_10,_11,_12){
this._getXmlDoc(_f,"entry",new _4.Entry(),_4._Constants.ATOM_NS,_10,_11,_12);
},_getXmlDoc:function(url,_13,_14,_15,_16,_17,_18){
if(!_18){
_18=_3.global;
}
var ae=this.alertsEnabled;
var _19={url:url,handleAs:"xml",sync:this.sync,preventCache:this.preventCache,load:function(_1a,_1b){
var _1c=null;
var _1d=_1a;
var _1e;
if(_1d){
if(typeof (_1d.getElementsByTagNameNS)!="undefined"){
_1e=_1d.getElementsByTagNameNS(_15,_13);
if(_1e&&_1e.length>0){
_1c=_1e.item(0);
}else{
if(_1d.lastChild){
_1c=_1d.lastChild;
}
}
}else{
if(typeof (_1d.getElementsByTagName)!="undefined"){
_1e=_1d.getElementsByTagName(_13);
if(_1e&&_1e.length>0){
for(var i=0;i<_1e.length;i++){
if(_1e[i].namespaceURI==_15){
_1c=_1e[i];
break;
}
}
}else{
if(_1d.lastChild){
_1c=_1d.lastChild;
}
}
}else{
if(_1d.lastChild){
_1c=_1d.lastChild;
}else{
_16.call(_18,null,null,_1b);
return;
}
}
}
_14.buildFromDom(_1c);
if(_16){
_16.call(_18,_14,_1d,_1b);
}else{
if(ae){
throw new Error("The callback value does not exist.");
}
}
}else{
_16.call(_18,null,null,_1b);
}
}};
if(this.user&&this.user!==null){
_19.user=this.user;
}
if(this.password&&this.password!==null){
_19.password=this.password;
}
if(_17){
_19.error=function(_1f,_20){
_17.call(_18,_1f,_20);
};
}else{
_19.error=function(){
throw new Error("The URL requested cannot be accessed");
};
}
_2.get(_19);
},updateEntry:function(_21,_22,_23,_24,_25,_26){
if(!_26){
_26=_3.global;
}
_21.updated=new Date();
var url=_21.getEditHref();
if(!url){
throw new Error("A URL has not been specified for editing this entry.");
}
var _27=this;
var ae=this.alertsEnabled;
var _28={url:url,handleAs:"text",contentType:"text/xml",sync:this.sync,preventCache:this.preventCache,load:function(_29,_2a){
var _2b=null;
if(_24){
_2b=_2a.xhr.getResponseHeader("Location");
if(!_2b){
_2b=url;
}
var _2c=function(_2d,dom,_2e){
if(_22){
_22.call(_26,_2d,_2b,_2e);
}else{
if(ae){
throw new Error("The callback value does not exist.");
}
}
};
_27.getEntry(_2b,_2c);
}else{
if(_22){
_22.call(_26,_21,_2a.xhr.getResponseHeader("Location"),_2a);
}else{
if(ae){
throw new Error("The callback value does not exist.");
}
}
}
return _29;
}};
if(this.user&&this.user!==null){
_28.user=this.user;
}
if(this.password&&this.password!==null){
_28.password=this.password;
}
if(_23){
_28.error=function(_2f,_30){
_23.call(_26,_2f,_30);
};
}else{
_28.error=function(){
throw new Error("The URL requested cannot be accessed");
};
}
if(_25){
_28.postData=_21.toString(true);
_28.headers={"X-Method-Override":"PUT"};
_2.post(_28);
}else{
_28.putData=_21.toString(true);
var xhr=_2.put(_28);
}
},addEntry:function(_31,url,_32,_33,_34,_35){
if(!_35){
_35=_3.global;
}
_31.published=new Date();
_31.updated=new Date();
var _36=_31.feedUrl;
var ae=this.alertsEnabled;
if(!url&&_36){
url=_36;
}
if(!url){
if(ae){
throw new Error("The request cannot be processed because the URL parameter is missing.");
}
return;
}
var _37=this;
var _38={url:url,handleAs:"text",contentType:"text/xml",sync:this.sync,preventCache:this.preventCache,postData:_31.toString(true),load:function(_39,_3a){
var _3b=_3a.xhr.getResponseHeader("Location");
if(!_3b){
_3b=url;
}
if(!_3a.retrieveEntry){
if(_32){
_32.call(_35,_31,_3b,_3a);
}else{
if(ae){
throw new Error("The callback value does not exist.");
}
}
}else{
var _3c=function(_3d,dom,_3e){
if(_32){
_32.call(_35,_3d,_3b,_3e);
}else{
if(ae){
throw new Error("The callback value does not exist.");
}
}
};
_37.getEntry(_3b,_3c);
}
return _39;
}};
if(this.user&&this.user!==null){
_38.user=this.user;
}
if(this.password&&this.password!==null){
_38.password=this.password;
}
if(_33){
_38.error=function(_3f,_40){
_33.call(_35,_3f,_40);
};
}else{
_38.error=function(){
throw new Error("The URL requested cannot be accessed");
};
}
_2.post(_38);
},deleteEntry:function(_41,_42,_43,_44,_45){
if(!_45){
_45=_3.global;
}
var url=null;
if(typeof (_41)=="string"){
url=_41;
}else{
url=_41.getEditHref();
}
if(!url){
_42.call(_45,false,null);
throw new Error("The request cannot be processed because the URL parameter is missing.");
}
var _46={url:url,handleAs:"text",sync:this.sync,preventCache:this.preventCache,load:function(_47,_48){
_42.call(_45,_48);
return _47;
}};
if(this.user&&this.user!==null){
_46.user=this.user;
}
if(this.password&&this.password!==null){
_46.password=this.password;
}
if(_43){
_46.error=function(_49,_4a){
_43.call(_45,_49,_4a);
};
}else{
_46.error=function(){
throw new Error("The URL requested cannot be accessed");
};
}
if(_44){
_46.headers={"X-Method-Override":"DELETE"};
dhxr.post(_46);
}else{
_2.del(_46);
}
}});
});
