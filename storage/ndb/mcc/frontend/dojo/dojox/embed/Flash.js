//>>built
define("dojox/embed/Flash",["dojo"],function(_1){
_1.getObject("embed",true,dojox);
var _2,_3;
var _4=9;
var _5="dojox-embed-flash-",_6=0;
var _7={expressInstall:false,width:320,height:240,swLiveConnect:"true",allowScriptAccess:"sameDomain",allowNetworking:"all",style:null,redirect:null};
function _8(_9){
_9=_1.delegate(_7,_9);
if(!("path" in _9)){
console.error("dojox.embed.Flash(ctor):: no path reference to a Flash movie was provided.");
return null;
}
if(!("id" in _9)){
_9.id=(_5+_6++);
}
return _9;
};
if(_1.isIE){
_2=function(_a){
_a=_8(_a);
if(!_a){
return null;
}
var p;
var _b=_a.path;
if(_a.vars){
var a=[];
for(p in _a.vars){
a.push(p+"="+_a.vars[p]);
}
_a.params.FlashVars=a.join("&");
delete _a.vars;
}
var s="<object id=\""+_a.id+"\" "+"classid=\"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000\" "+"width=\""+_a.width+"\" "+"height=\""+_a.height+"\""+((_a.style)?" style=\""+_a.style+"\"":"")+">"+"<param name=\"movie\" value=\""+_b+"\" />";
if(_a.params){
for(p in _a.params){
s+="<param name=\""+p+"\" value=\""+_a.params[p]+"\" />";
}
}
s+="</object>";
return {id:_a.id,markup:s};
};
_3=(function(){
var _c=10,_d=null;
while(!_d&&_c>7){
try{
_d=new ActiveXObject("ShockwaveFlash.ShockwaveFlash."+_c--);
}
catch(e){
}
}
if(_d){
var v=_d.GetVariable("$version").split(" ")[1].split(",");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
_1.addOnUnload(function(){
var _e=function(){
};
var _f=_1.query("object").reverse().style("display","none").forEach(function(i){
for(var p in i){
if((p!="FlashVars")&&_1.isFunction(i[p])){
try{
i[p]=_e;
}
catch(e){
}
}
}
});
});
}else{
_2=function(_10){
_10=_8(_10);
if(!_10){
return null;
}
var p;
var _11=_10.path;
if(_10.vars){
var a=[];
for(p in _10.vars){
a.push(p+"="+_10.vars[p]);
}
_10.params.flashVars=a.join("&");
delete _10.vars;
}
var s="<embed type=\"application/x-shockwave-flash\" "+"src=\""+_11+"\" "+"id=\""+_10.id+"\" "+"width=\""+_10.width+"\" "+"height=\""+_10.height+"\""+((_10.style)?" style=\""+_10.style+"\" ":"")+"pluginspage=\""+window.location.protocol+"//www.adobe.com/go/getflashplayer\" ";
if(_10.params){
for(p in _10.params){
s+=" "+p+"=\""+_10.params[p]+"\"";
}
}
s+=" />";
return {id:_10.id,markup:s};
};
_3=(function(){
var _12=navigator.plugins["Shockwave Flash"];
if(_12&&_12.description){
var v=_12.description.replace(/([a-zA-Z]|\s)+/,"").replace(/(\s+r|\s+b[0-9]+)/,".").split(".");
return {major:(v[0]!=null)?parseInt(v[0]):0,minor:(v[1]!=null)?parseInt(v[1]):0,rev:(v[2]!=null)?parseInt(v[2]):0};
}
return {major:0,minor:0,rev:0};
})();
}
dojox.embed.Flash=function(_13,_14){
if(location.href.toLowerCase().indexOf("file://")>-1){
throw new Error("dojox.embed.Flash can't be run directly from a file. To instatiate the required SWF correctly it must be run from a server, like localHost.");
}
this.available=dojox.embed.Flash.available;
this.minimumVersion=_13.minimumVersion||_4;
this.id=null;
this.movie=null;
this.domNode=null;
if(_14){
_14=_1.byId(_14);
}
setTimeout(_1.hitch(this,function(){
if(_13.expressInstall||this.available&&this.available>=this.minimumVersion){
if(_13&&_14){
this.init(_13,_14);
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
_1.extend(dojox.embed.Flash,{onReady:function(_15){
},onLoad:function(_16){
},onError:function(msg){
},_onload:function(){
clearInterval(this._poller);
delete this._poller;
delete this._pollCount;
delete this._pollMax;
this.onLoad(this.movie);
},init:function(_17,_18){
this.destroy();
_18=_1.byId(_18||this.domNode);
if(!_18){
throw new Error("dojox.embed.Flash: no domNode reference has been passed.");
}
var p=0,_19=false;
this._poller=null;
this._pollCount=0;
this._pollMax=15;
this.pollTime=100;
if(dojox.embed.Flash.initialized){
this.id=dojox.embed.Flash.place(_17,_18);
this.domNode=_18;
setTimeout(_1.hitch(this,function(){
this.movie=this.byId(this.id,_17.doc);
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
var _1a=_1.delegate({id:true,movie:true,domNode:true,onReady:true,onLoad:true});
for(var p in this){
if(!_1a[p]){
delete this[p];
}
}
if(this._poller){
_1.connect(this,"onLoad",this,"_destroy");
}else{
this._destroy();
}
},byId:function(_1b,doc){
doc=doc||document;
if(doc.embeds[_1b]){
return doc.embeds[_1b];
}
if(doc[_1b]){
return doc[_1b];
}
if(window[_1b]){
return window[_1b];
}
if(document[_1b]){
return document[_1b];
}
return null;
}});
_1.mixin(dojox.embed.Flash,{minSupported:8,available:_3.major,supported:(_3.major>=_3.required),minimumRequired:_3.required,version:_3,initialized:false,onInitialize:function(){
dojox.embed.Flash.initialized=true;
},__ie_markup__:function(_1c){
return _2(_1c);
},proxy:function(obj,_1d){
_1.forEach((_1.isArray(_1d)?_1d:[_1d]),function(_1e){
this[_1e]=_1.hitch(this,function(){
return (function(){
return eval(this.movie.CallFunction("<invoke name=\""+_1e+"\" returntype=\"javascript\">"+"<arguments>"+_1.map(arguments,function(_1f){
return __flash__toXML(_1f);
}).join("")+"</arguments>"+"</invoke>"));
}).apply(this,arguments||[]);
});
},obj);
}});
dojox.embed.Flash.place=function(_20,_21){
var o=_2(_20);
_21=_1.byId(_21);
if(!_21){
_21=_1.doc.createElement("div");
_21.id=o.id+"-container";
_1.body().appendChild(_21);
}
if(o){
_21.innerHTML=o.markup;
return o.id;
}
return null;
};
dojox.embed.Flash.onInitialize();
return dojox.embed.Flash;
});
