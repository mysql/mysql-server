//>>built
define("dojox/embed/Flash",["dojo/_base/lang","dojo/_base/unload","dojo/_base/array","dojo/query","dojo/has","dojo/dom","dojo/on","dojo/window","dojo/string"],function(_1,_2,_3,_4,_5,_6,on,_7,_8){
var _9,_a;
var _b=9;
var _c="dojox-embed-flash-",_d=0;
var _e={expressInstall:false,width:320,height:240,swLiveConnect:"true",allowScriptAccess:"sameDomain",allowNetworking:"all",style:null,redirect:null};
function _f(_10){
_10=_1.delegate(_e,_10);
if(!("path" in _10)){
console.error("dojox.embed.Flash(ctor):: no path reference to a Flash movie was provided.");
return null;
}
if(!("id" in _10)){
_10.id=(_c+_d++);
}
return _10;
};
if(_5("ie")){
_9=function(_11){
_11=_f(_11);
if(!_11){
return null;
}
var p;
var _12=_11.path;
if(_11.vars){
var a=[];
for(p in _11.vars){
a.push(encodeURIComponent(p)+"="+encodeURIComponent(_11.vars[p]));
}
_11.params.FlashVars=a.join("&");
delete _11.vars;
}
var s="<object id=\""+_8.escape(String(_11.id))+"\" "+"classid=\"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000\" "+"width=\""+_8.escape(String(_11.width))+"\" "+"height=\""+_8.escape(String(_11.height))+"\""+((_11.style)?" style=\""+_8.escape(String(_11.style))+"\"":"")+">"+"<param name=\"movie\" value=\""+_8.escape(String(_12))+"\" />";
if(_11.params){
for(p in _11.params){
s+="<param name=\""+_8.escape(p)+"\" value=\""+_8.escape(String(_11.params[p]))+"\" />";
}
}
s+="</object>";
return {id:_11.id,markup:s};
};
_a=(function(){
var _13=10,_14=null;
while(!_14&&_13>7){
try{
_14=new ActiveXObject("ShockwaveFlash.ShockwaveFlash."+_13--);
}
catch(e){
}
}
if(_14){
var v=_14.GetVariable("$version").split(" ")[1].split(",");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
_2.addOnWindowUnload(function(){
console.warn("***************UNLOAD");
var _15=function(){
};
var _16=_4("object").reverse().style("display","none").forEach(function(i){
for(var p in i){
if((p!="FlashVars")&&typeof i[p]=="function"){
try{
i[p]=_15;
}
catch(e){
}
}
}
});
});
}else{
_9=function(_17){
_17=_f(_17);
if(!_17){
return null;
}
var p;
var _18=_17.path;
if(_17.vars){
var a=[];
for(p in _17.vars){
a.push(encodeURIComponent(p)+"="+encodeURIComponent(_17.vars[p]));
}
_17.params.flashVars=a.join("&");
delete _17.vars;
}
var s="<embed type=\"application/x-shockwave-flash\" "+"src=\""+_8.escape(String(_18))+"\" "+"id=\""+_8.escape(String(_17.id))+"\" "+"width=\""+_8.escape(String(_17.width))+"\" "+"height=\""+_8.escape(String(_17.height))+"\""+((_17.style)?" style=\""+_8.escape(String(_17.style))+"\" ":"")+"pluginspage=\""+window.location.protocol+"//www.adobe.com/go/getflashplayer\" ";
if(_17.params){
for(p in _17.params){
s+=" "+_8.escape(p)+"=\""+_8.escape(String(_17.params[p]))+"\"";
}
}
s+=" />";
return {id:_17.id,markup:s};
};
_a=(function(){
var _19=navigator.plugins["Shockwave Flash"];
if(_19&&_19.description){
var v=_19.description.replace(/([a-zA-Z]|\s)+/,"").replace(/(\s+r|\s+b[0-9]+)/,".").split(".");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
}
var _1a=function(_1b,_1c){
if(location.href.toLowerCase().indexOf("file://")>-1){
throw new Error("dojox.embed.Flash can't be run directly from a file. To instatiate the required SWF correctly it must be run from a server, like localHost.");
}
this.available=_a.major;
this.minimumVersion=_1b.minimumVersion||_b;
this.id=null;
this.movie=null;
this.domNode=null;
if(_1c){
_1c=_6.byId(_1c);
}
setTimeout(_1.hitch(this,function(){
if(_1b.expressInstall||this.available&&this.available>=this.minimumVersion){
if(_1b&&_1c){
this.init(_1b,_1c);
}else{
this.onError("embed.Flash was not provided with the proper arguments.");
}
}else{
if(!this.available){
this.onError("Flash is not installed.");
}else{
this.onError("Flash version detected: "+this.available+" is out of date. Minimum required: "+this.minimumVersion);
}
}
}),100);
};
_1.extend(_1a,{onReady:function(_1d){
},onLoad:function(_1e){
},onError:function(msg){
},_onload:function(){
clearInterval(this._poller);
delete this._poller;
delete this._pollCount;
delete this._pollMax;
this.onLoad(this.movie);
},init:function(_1f,_20){
this.destroy();
_20=_6.byId(_20||this.domNode);
if(!_20){
throw new Error("dojox.embed.Flash: no domNode reference has been passed.");
}
var p=0,_21=false;
this._poller=null;
this._pollCount=0;
this._pollMax=15;
this.pollTime=100;
if(_1a.initialized){
this.id=_1a.place(_1f,_20);
this.domNode=_20;
setTimeout(_1.hitch(this,function(){
this.movie=this.byId(this.id,_1f.doc);
this.onReady(this.movie);
this._poller=setInterval(_1.hitch(this,function(){
try{
p=this.movie.PercentLoaded();
}
catch(e){
console.warn("this.movie.PercentLoaded() failed",e,this.movie);
}
if(p==100){
this._onload();
}else{
if(p==0&&this._pollCount++>this._pollMax){
clearInterval(this._poller);
throw new Error("Building SWF failed.");
}
}
}),this.pollTime);
}),1);
}
},_destroy:function(){
try{
this.domNode.removeChild(this.movie);
}
catch(e){
}
this.id=this.movie=this.domNode=null;
},destroy:function(){
if(!this.movie){
return;
}
var _22=_1.delegate({id:true,movie:true,domNode:true,onReady:true,onLoad:true});
for(var p in this){
if(!_22[p]){
delete this[p];
}
}
if(this._poller){
on(this,"Load",this,"_destroy");
}else{
this._destroy();
}
},byId:function(_23,doc){
doc=doc||document;
if(doc.embeds[_23]){
return doc.embeds[_23];
}
if(doc[_23]){
return doc[_23];
}
if(window[_23]){
return window[_23];
}
if(document[_23]){
return document[_23];
}
return null;
}});
_1.mixin(_1a,{minSupported:8,available:_a.major,supported:(_a.major>=_a.required),minimumRequired:_a.required,version:_a,initialized:false,onInitialize:function(){
_1a.initialized=true;
},__ie_markup__:function(_24){
return _9(_24);
},proxy:function(obj,_25){
_3.forEach((_25 instanceof Array?_25:[_25]),function(_26){
this[_26]=_1.hitch(this,function(){
return (function(){
return eval(this.movie.CallFunction("<invoke name=\""+_26+"\" returntype=\"javascript\">"+"<arguments>"+_3.map(arguments,function(_27){
return __flash__toXML(_27);
}).join("")+"</arguments>"+"</invoke>"));
}).apply(this,arguments||[]);
});
},obj);
}});
_1a.place=function(_28,_29){
var o=_9(_28);
_29=_6.byId(_29);
if(!_29){
_29=_7.doc.createElement("div");
_29.id=o.id+"-container";
_7.body().appendChild(_29);
}
if(o){
_29.innerHTML=o.markup;
return o.id;
}
return null;
};
_1a.onInitialize();
_1.setObject("dojox.embed.Flash",_1a);
return _1a;
});
