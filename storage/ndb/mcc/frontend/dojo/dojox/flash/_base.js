//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/window"],function(_1,_2,_3){
_2.provide("dojox.flash._base");
_2.experimental("dojox.flash");
_2.require("dojo.window");
_3.flash=function(){
};
_3.flash={ready:false,url:null,_visible:true,_loadedListeners:[],_installingListeners:[],setSwf:function(_4,_5){
this.url=_4;
this._visible=true;
if(_5!==null&&_5!==undefined){
this._visible=_5;
}
this._initialize();
},addLoadedListener:function(_6){
this._loadedListeners.push(_6);
},addInstallingListener:function(_7){
this._installingListeners.push(_7);
},loaded:function(){
_3.flash.ready=true;
if(_3.flash._loadedListeners.length){
for(var i=0;i<_3.flash._loadedListeners.length;i++){
_3.flash._loadedListeners[i].call(null);
}
}
},installing:function(){
if(_3.flash._installingListeners.length){
for(var i=0;i<_3.flash._installingListeners.length;i++){
_3.flash._installingListeners[i].call(null);
}
}
},_initialize:function(){
var _8=new _3.flash.Install();
_3.flash.installer=_8;
if(_8.needed()){
_8.install();
}else{
_3.flash.obj=new _3.flash.Embed(this._visible);
_3.flash.obj.write();
_3.flash.comm=new _3.flash.Communicator();
}
}};
_3.flash.Info=function(){
this._detectVersion();
};
_3.flash.Info.prototype={version:-1,versionMajor:-1,versionMinor:-1,versionRevision:-1,capable:false,installing:false,isVersionOrAbove:function(_9,_a,_b){
_b=parseFloat("."+_b);
if(this.versionMajor>=_9&&this.versionMinor>=_a&&this.versionRevision>=_b){
return true;
}else{
return false;
}
},_detectVersion:function(){
var _c;
for(var _d=25;_d>0;_d--){
if(_2.isIE){
var _e;
try{
if(_d>6){
_e=new ActiveXObject("ShockwaveFlash.ShockwaveFlash."+_d);
}else{
_e=new ActiveXObject("ShockwaveFlash.ShockwaveFlash");
}
if(typeof _e=="object"){
if(_d==6){
_e.AllowScriptAccess="always";
}
_c=_e.GetVariable("$version");
}
}
catch(e){
continue;
}
}else{
_c=this._JSFlashInfo(_d);
}
if(_c==-1){
this.capable=false;
return;
}else{
if(_c!=0){
var _f;
if(_2.isIE){
var _10=_c.split(" ");
var _11=_10[1];
_f=_11.split(",");
}else{
_f=_c.split(".");
}
this.versionMajor=_f[0];
this.versionMinor=_f[1];
this.versionRevision=_f[2];
var _12=this.versionMajor+"."+this.versionRevision;
this.version=parseFloat(_12);
this.capable=true;
break;
}
}
}
},_JSFlashInfo:function(_13){
if(navigator.plugins!=null&&navigator.plugins.length>0){
if(navigator.plugins["Shockwave Flash 2.0"]||navigator.plugins["Shockwave Flash"]){
var _14=navigator.plugins["Shockwave Flash 2.0"]?" 2.0":"";
var _15=navigator.plugins["Shockwave Flash"+_14].description;
var _16=_15.split(" ");
var _17=_16[2].split(".");
var _18=_17[0];
var _19=_17[1];
var _1a=(_16[3]||_16[4]).split("r");
var _1b=_1a[1]>0?_1a[1]:0;
var _1c=_18+"."+_19+"."+_1b;
return _1c;
}
}
return -1;
}};
_3.flash.Embed=function(_1d){
this._visible=_1d;
};
_3.flash.Embed.prototype={width:215,height:138,id:"flashObject",_visible:true,protocol:function(){
switch(window.location.protocol){
case "https:":
return "https";
break;
default:
return "http";
break;
}
},write:function(_1e){
var _1f;
var _20=_3.flash.url;
var _21=_20;
var _22=_20;
var _23=_2.baseUrl;
var _24=document.location.protocol+"//"+document.location.host;
if(_1e){
var _25=escape(window.location);
document.title=document.title.slice(0,47)+" - Flash Player Installation";
var _26=escape(document.title);
_21+="?MMredirectURL="+_25+"&MMplayerType=ActiveX"+"&MMdoctitle="+_26+"&baseUrl="+escape(_23)+"&xdomain="+escape(_24);
_22+="?MMredirectURL="+_25+"&MMplayerType=PlugIn"+"&baseUrl="+escape(_23)+"&xdomain="+escape(_24);
}else{
_21+="?cachebust="+new Date().getTime();
_21+="&baseUrl="+escape(_23);
_21+="&xdomain="+escape(_24);
}
if(_22.indexOf("?")==-1){
_22+="?baseUrl="+escape(_23);
}else{
_22+="&baseUrl="+escape(_23);
}
_22+="&xdomain="+escape(_24);
_1f="<object classid=\"clsid:d27cdb6e-ae6d-11cf-96b8-444553540000\" "+"codebase=\""+this.protocol()+"://fpdownload.macromedia.com/pub/shockwave/cabs/flash/"+"swflash.cab#version=8,0,0,0\"\n "+"width=\""+this.width+"\"\n "+"height=\""+this.height+"\"\n "+"id=\""+this.id+"\"\n "+"name=\""+this.id+"\"\n "+"align=\"middle\">\n "+"<param name=\"allowScriptAccess\" value=\"always\"></param>\n "+"<param name=\"movie\" value=\""+_21+"\"></param>\n "+"<param name=\"quality\" value=\"high\"></param>\n "+"<param name=\"bgcolor\" value=\"#ffffff\"></param>\n "+"<embed src=\""+_22+"\" "+"quality=\"high\" "+"bgcolor=\"#ffffff\" "+"width=\""+this.width+"\" "+"height=\""+this.height+"\" "+"id=\""+this.id+"Embed"+"\" "+"name=\""+this.id+"\" "+"swLiveConnect=\"true\" "+"align=\"middle\" "+"allowScriptAccess=\"always\" "+"type=\"application/x-shockwave-flash\" "+"pluginspage=\""+this.protocol()+"://www.macromedia.com/go/getflashplayer\" "+"></embed>\n"+"</object>\n";
_2.connect(_2,"loaded",_2.hitch(this,function(){
var _27=this.id+"Container";
if(_2.byId(_27)){
return;
}
var div=document.createElement("div");
div.id=this.id+"Container";
div.style.width=this.width+"px";
div.style.height=this.height+"px";
if(!this._visible){
div.style.position="absolute";
div.style.zIndex="10000";
div.style.top="-1000px";
}
div.innerHTML=_1f;
var _28=document.getElementsByTagName("body");
if(!_28||!_28.length){
throw new Error("No body tag for this page");
}
_28=_28[0];
_28.appendChild(div);
}));
},get:function(){
if(_2.isIE||_2.isWebKit){
return _2.byId(this.id);
}else{
return document[this.id+"Embed"];
}
},setVisible:function(_29){
var _2a=_2.byId(this.id+"Container");
if(_29){
_2a.style.position="absolute";
_2a.style.visibility="visible";
}else{
_2a.style.position="absolute";
_2a.style.y="-1000px";
_2a.style.visibility="hidden";
}
},center:function(){
var _2b=this.width;
var _2c=this.height;
var _2d=_2.window.getBox();
var x=_2d.l+(_2d.w-_2b)/2;
var y=_2d.t+(_2d.h-_2c)/2;
var _2e=_2.byId(this.id+"Container");
_2e.style.top=y+"px";
_2e.style.left=x+"px";
}};
_3.flash.Communicator=function(){
};
_3.flash.Communicator.prototype={_addExternalInterfaceCallback:function(_2f){
var _30=_2.hitch(this,function(){
var _31=new Array(arguments.length);
for(var i=0;i<arguments.length;i++){
_31[i]=this._encodeData(arguments[i]);
}
var _32=this._execFlash(_2f,_31);
_32=this._decodeData(_32);
return _32;
});
this[_2f]=_30;
},_encodeData:function(_33){
if(!_33||typeof _33!="string"){
return _33;
}
_33=_33.replace("\\","&custom_backslash;");
_33=_33.replace(/\0/g,"&custom_null;");
return _33;
},_decodeData:function(_34){
if(_34&&_34.length&&typeof _34!="string"){
_34=_34[0];
}
if(!_34||typeof _34!="string"){
return _34;
}
_34=_34.replace(/\&custom_null\;/g,"\x00");
_34=_34.replace(/\&custom_lt\;/g,"<").replace(/\&custom_gt\;/g,">").replace(/\&custom_backslash\;/g,"\\");
return _34;
},_execFlash:function(_35,_36){
var _37=_3.flash.obj.get();
_36=(_36)?_36:[];
for(var i=0;i<_36;i++){
if(typeof _36[i]=="string"){
_36[i]=this._encodeData(_36[i]);
}
}
var _38=function(){
return eval(_37.CallFunction("<invoke name=\""+_35+"\" returntype=\"javascript\">"+__flash__argumentsToXML(_36,0)+"</invoke>"));
};
var _39=_38.call(_36);
if(typeof _39=="string"){
_39=this._decodeData(_39);
}
return _39;
}};
_3.flash.Install=function(){
};
_3.flash.Install.prototype={needed:function(){
if(!_3.flash.info.capable){
return true;
}
if(!_3.flash.info.isVersionOrAbove(8,0,0)){
return true;
}
return false;
},install:function(){
var _3a;
_3.flash.info.installing=true;
_3.flash.installing();
if(_3.flash.info.capable==false){
_3a=new _3.flash.Embed(false);
_3a.write();
}else{
if(_3.flash.info.isVersionOrAbove(6,0,65)){
_3a=new _3.flash.Embed(false);
_3a.write(true);
_3a.setVisible(true);
_3a.center();
}else{
alert("This content requires a more recent version of the Macromedia "+" Flash Player.");
window.location.href=+_3.flash.Embed.protocol()+"://www.macromedia.com/go/getflashplayer";
}
}
},_onInstallStatus:function(msg){
if(msg=="Download.Complete"){
_3.flash._initialize();
}else{
if(msg=="Download.Cancelled"){
alert("This content requires a more recent version of the Macromedia "+" Flash Player.");
window.location.href=_3.flash.Embed.protocol()+"://www.macromedia.com/go/getflashplayer";
}else{
if(msg=="Download.Failed"){
alert("There was an error downloading the Flash Player update. "+"Please try again later, or visit macromedia.com to download "+"the latest version of the Flash plugin.");
}
}
}
}};
_3.flash.info=new _3.flash.Info();
});
