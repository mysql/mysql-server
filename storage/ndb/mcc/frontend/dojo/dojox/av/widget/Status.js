//>>built
define("dojox/av/widget/Status",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2,_3,_4){
return _1.declare("dojox.av.widget.Status",[_3,_4],{templateString:_1.cache("dojox.av.widget","resources/Status.html"),setMedia:function(_5){
this.media=_5;
_1.connect(this.media,"onMetaData",this,function(_6){
this.duration=_6.duration;
this.durNode.innerHTML=this.toSeconds(this.duration);
});
_1.connect(this.media,"onPosition",this,function(_7){
this.timeNode.innerHTML=this.toSeconds(_7);
});
var _8=["onMetaData","onPosition","onStart","onBuffer","onPlay","onPaused","onStop","onEnd","onError","onLoad"];
_1.forEach(_8,function(c){
_1.connect(this.media,c,this,c);
},this);
},onMetaData:function(_9){
this.duration=_9.duration;
this.durNode.innerHTML=this.toSeconds(this.duration);
if(this.media.title){
this.title=this.media.title;
}else{
var a=this.media.mediaUrl.split("/");
var b=a[a.length-1].split(".")[0];
this.title=b;
}
},onBuffer:function(_a){
this.isBuffering=_a;
console.warn("status onBuffer",this.isBuffering);
if(this.isBuffering){
this.setStatus("buffering...");
}else{
this.setStatus("Playing");
}
},onPosition:function(_b){
},onStart:function(){
this.setStatus("Starting");
},onPlay:function(){
this.setStatus("Playing");
},onPaused:function(){
this.setStatus("Paused");
},onStop:function(){
this.setStatus("Stopped");
},onEnd:function(){
this.setStatus("Stopped");
},onError:function(_c){
var _d=_c.info.code;
if(_d=="NetStream.Play.StreamNotFound"){
_d="Stream Not Found";
}
this.setStatus("ERROR: "+_d,true);
},onLoad:function(){
this.setStatus("Loading...");
},setStatus:function(_e,_f){
if(_f){
_1.addClass(this.titleNode,"statusError");
}else{
_1.removeClass(this.titleNode,"statusError");
if(this.isBuffering){
_e="buffering...";
}
}
this.titleNode.innerHTML="<span class=\"statusTitle\">"+this.title+"</span> <span class=\"statusInfo\">"+_e+"</span>";
},toSeconds:function(_10){
var ts=_10.toString();
if(ts.indexOf(".")<0){
ts+=".00";
}else{
if(ts.length-ts.indexOf(".")==2){
ts+="0";
}else{
if(ts.length-ts.indexOf(".")>2){
ts=ts.substring(0,ts.indexOf(".")+3);
}
}
}
return ts;
}});
});
