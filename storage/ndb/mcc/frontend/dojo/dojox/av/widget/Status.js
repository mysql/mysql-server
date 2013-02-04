//>>built
define("dojox/av/widget/Status",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2){
_1.declare("dojox.av.widget.Status",[_2._Widget,_2._TemplatedMixin],{templateString:_1.cache("dojox.av.widget","resources/Status.html"),setMedia:function(_3){
this.media=_3;
_1.connect(this.media,"onMetaData",this,function(_4){
this.duration=_4.duration;
this.durNode.innerHTML=this.toSeconds(this.duration);
});
_1.connect(this.media,"onPosition",this,function(_5){
this.timeNode.innerHTML=this.toSeconds(_5);
});
var _6=["onMetaData","onPosition","onStart","onBuffer","onPlay","onPaused","onStop","onEnd","onError","onLoad"];
_1.forEach(_6,function(c){
_1.connect(this.media,c,this,c);
},this);
},onMetaData:function(_7){
this.duration=_7.duration;
this.durNode.innerHTML=this.toSeconds(this.duration);
if(this.media.title){
this.title=this.media.title;
}else{
var a=this.media.mediaUrl.split("/");
var b=a[a.length-1].split(".")[0];
this.title=b;
}
},onBuffer:function(_8){
this.isBuffering=_8;
console.warn("status onBuffer",this.isBuffering);
if(this.isBuffering){
this.setStatus("buffering...");
}else{
this.setStatus("Playing");
}
},onPosition:function(_9){
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
},onError:function(_a){
var _b=_a.info.code;
if(_b=="NetStream.Play.StreamNotFound"){
_b="Stream Not Found";
}
this.setStatus("ERROR: "+_b,true);
},onLoad:function(){
this.setStatus("Loading...");
},setStatus:function(_c,_d){
if(_d){
_1.addClass(this.titleNode,"statusError");
}else{
_1.removeClass(this.titleNode,"statusError");
if(this.isBuffering){
_c="buffering...";
}
}
this.titleNode.innerHTML="<span class=\"statusTitle\">"+this.title+"</span> <span class=\"statusInfo\">"+_c+"</span>";
},toSeconds:function(_e){
var ts=_e.toString();
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
return dojox.av.widget.Status;
});
