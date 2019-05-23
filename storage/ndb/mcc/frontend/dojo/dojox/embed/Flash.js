//>>built
define("dojox/embed/Flash",["dojo"],function(_1){
function _2(_3){
return String(_3).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/"/g,"&quot;").replace(/'/g,"&apos;");
};
var _4,_5;
var _6=9;
var _7="dojox-embed-flash-",_8=0;
var _9={expressInstall:false,width:320,height:240,swLiveConnect:"true",allowScriptAccess:"sameDomain",allowNetworking:"all",style:null,redirect:null};
function _a(_b){
_b=_1.delegate(_9,_b);
if(!("path" in _b)){
console.error("dojox.embed.Flash(ctor):: no path reference to a Flash movie was provided.");
return null;
}
if(!("id" in _b)){
_b.id=(_7+_8++);
}
return _b;
};
if(_1.isIE){
_4=function(_c){
_c=_a(_c);
if(!_c){
return null;
}
var p;
var _d=_c.path;
if(_c.vars){
var a=[];
for(p in _c.vars){
a.push(encodeURIComponent(p)+"="+encodeURIComponent(_c.vars[p]));
}
_c.params.FlashVars=a.join("&");
delete _c.vars;
}
var s="<object id=\""+_2(String(_c.id))+"\" "+"classid=\"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000\" "+"width=\""+_2(String(_c.width))+"\" "+"height=\""+_2(String(_c.height))+"\""+((_c.style)?" style=\""+_2(String(_c.style))+"\"":"")+">"+"<param name=\"movie\" value=\""+_2(String(_d))+"\" />";
if(_c.params){
for(p in _c.params){
s+="<param name=\""+_2(p)+"\" value=\""+_2(String(_c.params[p]))+"\" />";
}
}
s+="</object>";
return {id:_c.id,markup:s};
};
_5=(function(){
var _e=10,_f=null;
while(!_f&&_e>7){
try{
_f=new ActiveXObject("ShockwaveFlash.ShockwaveFlash."+_e--);
}
catch(e){
}
}
if(_f){
var v=_f.GetVariable("$version").split(" ")[1].split(",");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
_1.addOnUnload(function(){
var _10=function(){
};
var _11=_1.query("object").reverse().style("display","none").forEach(function(i){
for(var p in i){
if((p!="FlashVars")&&_1.isFunction(i[p])){
try{
i[p]=_10;
}
catch(e){
}
}
}
});
});
}else{
_4=function(_12){
_12=_a(_12);
if(!_12){
return null;
}
var p;
var _13=_12.path;
if(_12.vars){
var a=[];
for(p in _12.vars){
a.push(encodeURIComponent(p)+"="+encodeURIComponent(_12.vars[p]));
}
_12.params.flashVars=a.join("&");
delete _12.vars;
}
var s="<embed type=\"application/x-shockwave-flash\" "+"src=\""+_2(String(_13))+"\" "+"id=\""+_2(String(_12.id))+"\" "+"width=\""+_2(String(_12.width))+"\" "+"height=\""+_2(String(_12.height))+"\""+((_12.style)?" style=\""+_2(String(_12.style))+"\" ":"")+"pluginspage=\""+window.location.protocol+"//www.adobe.com/go/getflashplayer\" ";
if(_12.params){
for(p in _12.params){
s+=" "+_2(p)+"=\""+_2(String(_12.params[p]))+"\"";
}
}
s+=" />";
return {id:_12.id,markup:s};
};
_5=(function(){
var _14=navigator.plugins["Shockwave Flash"];
if(_14&&_14.description){
var v=_14.description.replace(/([a-zA-Z]|\s)+/,"").replace(/(\s+r|\s+b[0-9]+)/,".").split(".");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
}
var _15=function(_16,_17){
if(location.href.toLowerCase().indexOf("file://")>-1){
throw new Error("dojox.embed.Flash can't be run directly from a file. To instatiate the required SWF correctly it must be run from a server, like localHost.");
}
this.available=dojox.embed.Flash.available;
this.minimumVersion=_16.minimumVersion||_6;
this.id=null;
this.movie=null;
this.domNode=null;
if(_17){
_17=_1.byId(_17);
}
setTimeout(_1.hitch(this,function(){
if(_16.expressInstall||this.available&&this.available>=this.minimumVersion){
if(_16&&_17){
this.init(_16,_17);
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
_1.extend(_15,{onReady:function(_18){
},onLoad:function(_19){
},onError:function(msg){
},_onload:function(){
clearInterval(this._poller);
delete this._poller;
delete this._pollCount;
delete this._pollMax;
this.onLoad(this.movie);
},init:function(_1a,_1b){
this.destroy();
_1b=_1.byId(_1b||this.domNode);
if(!_1b){
throw new Error("dojox.embed.Flash: no domNode reference has been passed.");
}
var p=0,_1c=false;
this._poller=null;
this._pollCount=0;
this._pollMax=15;
this.pollTime=100;
if(dojox.embed.Flash.initialized){
this.id=dojox.embed.Flash.place(_1a,_1b);
this.domNode=_1b;
setTimeout(_1.hitch(this,function(){
this.movie=this.byId(this.id,_1a.doc);
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
var _1d=_1.delegate({id:true,movie:true,domNode:true,onReady:true,onLoad:true});
for(var p in this){
if(!_1d[p]){
delete this[p];
}
}
if(this._poller){
_1.connect(this,"onLoad",this,"_destroy");
}else{
this._destroy();
}
},byId:function(_1e,doc){
doc=doc||document;
if(doc.embeds[_1e]){
return doc.embeds[_1e];
}
if(doc[_1e]){
return doc[_1e];
}
if(window[_1e]){
return window[_1e];
}
if(document[_1e]){
return document[_1e];
}
return null;
}});
_1.mixin(_15,{minSupported:8,available:_5.major,supported:(_5.major>=_5.required),minimumRequired:_5.required,version:_5,initialized:false,onInitialize:function(){
_15.initialized=true;
},__ie_markup__:function(_1f){
return _4(_1f);
},proxy:function(obj,_20){
_1.forEach((_1.isArray(_20)?_20:[_20]),function(_21){
this[_21]=_1.hitch(this,function(){
return (function(){
return eval(this.movie.CallFunction("<invoke name=\""+_21+"\" returntype=\"javascript\">"+"<arguments>"+_1.map(arguments,function(_22){
return __flash__toXML(_22);
}).join("")+"</arguments>"+"</invoke>"));
}).apply(this,arguments||[]);
});
},obj);
}});
_15.place=function(_23,_24){
var o=_4(_23);
_24=_1.byId(_24);
if(!_24){
_24=_1.doc.createElement("div");
_24.id=o.id+"-container";
_1.body().appendChild(_24);
}
if(o){
_24.innerHTML=o.markup;
return o.id;
}
return null;
};
_15.onInitialize();
_1.setObject("dojox.embed.Flash",_15);
return _15;
});
