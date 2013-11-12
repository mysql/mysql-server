//>>built
define("dojox/av/FLVideo",["dojo","dijit","dijit/_Widget","dojox/embed/Flash","dojox/av/_Media"],function(_1,_2){
_1.experimental("dojox.av.FLVideo");
_1.declare("dojox.av.FLVideo",[_2._Widget,dojox.av._Media],{_swfPath:_1.moduleUrl("dojox.av","resources/video.swf"),constructor:function(_3){
_1.global.swfIsInHTML=function(){
return true;
};
},postCreate:function(){
this._subs=[];
this._cons=[];
this.mediaUrl=this._normalizeUrl(this.mediaUrl);
this.initialVolume=this._normalizeVolume(this.initialVolume);
var _4={path:this._swfPath,width:"100%",height:"100%",minimumVersion:9,expressInstall:true,params:{allowFullScreen:this.allowFullScreen,wmode:this.wmode,allowScriptAccess:this.allowScriptAccess,allowNetworking:this.allowNetworking},vars:{videoUrl:this.mediaUrl,id:this.id,autoPlay:this.autoPlay,volume:this.initialVolume,isDebug:this.isDebug}};
this._sub("stageClick","onClick");
this._sub("stageSized","onSwfSized");
this._sub("mediaStatus","onPlayerStatus");
this._sub("mediaMeta","onMetaData");
this._sub("mediaError","onError");
this._sub("mediaStart","onStart");
this._sub("mediaEnd","onEnd");
this._flashObject=new dojox.embed.Flash(_4,this.domNode);
this._flashObject.onError=function(_5){
console.error("Flash Error:",_5);
};
this._flashObject.onLoad=_1.hitch(this,function(_6){
this.flashMedia=_6;
this.isPlaying=this.autoPlay;
this.isStopped=!this.autoPlay;
this.onLoad(this.flashMedia);
this._initStatus();
this._update();
});
this.inherited(arguments);
},play:function(_7){
this.isPlaying=true;
this.isStopped=false;
this.flashMedia.doPlay(this._normalizeUrl(_7));
},pause:function(){
this.isPlaying=false;
this.isStopped=false;
if(this.onPaused){
this.onPaused();
}
this.flashMedia.pause();
},seek:function(_8){
this.flashMedia.seek(_8);
},volume:function(_9){
if(_9){
if(!this.flashMedia){
this.initialVolume=_9;
}
this.flashMedia.setVolume(this._normalizeVolume(_9));
}
if(!this.flashMedia||!this.flashMedia.doGetVolume){
return this.initialVolume;
}
return this.flashMedia.getVolume();
},_checkBuffer:function(_a,_b){
if(this.percentDownloaded==100){
if(this.isBuffering){
this.onBuffer(false);
this.flashMedia.doPlay();
}
return;
}
if(!this.isBuffering&&_b<0.1){
this.onBuffer(true);
this.flashMedia.pause();
return;
}
var _c=this.percentDownloaded*0.01*this.duration;
if(!this.isBuffering&&_a+this.minBufferTime*0.001>_c){
this.onBuffer(true);
this.flashMedia.pause();
}else{
if(this.isBuffering&&_a+this.bufferTime*0.001<=_c){
this.onBuffer(false);
this.flashMedia.doPlay();
}
}
},_update:function(){
var _d=Math.min(this.getTime()||0,this.duration);
var _e=this.flashMedia.getLoaded();
this.percentDownloaded=Math.ceil(_e.bytesLoaded/_e.bytesTotal*100);
this.onDownloaded(this.percentDownloaded);
this.onPosition(_d);
if(this.duration){
this._checkBuffer(_d,_e.buffer);
}
this._updateHandle=setTimeout(_1.hitch(this,"_update"),this.updateTime);
},destroy:function(){
clearTimeout(this._updateHandle);
_1.disconnect(this._positionHandle);
this.inherited(arguments);
}});
return dojox.av.FLVideo;
});
