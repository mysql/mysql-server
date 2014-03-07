//>>built
define("dojox/av/FLAudio",["dojo","dojox/embed/Flash","dojox/timing/doLater"],function(_1,_2){
_1.experimental("dojox.av.FLVideo");
_1.declare("dojox.av.FLAudio",null,{id:"",initialVolume:0.7,initialPan:0,isDebug:false,statusInterval:200,_swfPath:_1.moduleUrl("dojox.av","resources/audio.swf"),allowScriptAccess:"always",allowNetworking:"all",constructor:function(_3){
_1.global.swfIsInHTML=function(){
return true;
};
_1.mixin(this,_3||{});
if(!this.id){
this.id="flaudio_"+new Date().getTime();
}
this.domNode=_1.doc.createElement("div");
_1.style(this.domNode,{position:"relative",width:"1px",height:"1px",top:"1px",left:"1px"});
_1.body().appendChild(this.domNode);
this.init();
},init:function(){
this._subs=[];
this.initialVolume=this._normalizeVolume(this.initialVolume);
var _4={path:this._swfPath.uri,width:"1px",height:"1px",minimumVersion:9,expressInstall:true,params:{wmode:"transparent",allowScriptAccess:this.allowScriptAccess,allowNetworking:this.allowNetworking},vars:{id:this.id,autoPlay:this.autoPlay,initialVolume:this.initialVolume,initialPan:this.initialPan,statusInterval:this.statusInterval,isDebug:this.isDebug}};
this._sub("mediaError","onError");
this._sub("filesProgress","onLoadStatus");
this._sub("filesAllLoaded","onAllLoaded");
this._sub("mediaPosition","onPlayStatus");
this._sub("mediaEnd","onComplete");
this._sub("mediaMeta","onID3");
this._flashObject=new dojox.embed.Flash(_4,this.domNode);
this._flashObject.onError=function(_5){
console.warn("Flash Error:",_5);
};
this._flashObject.onLoad=_1.hitch(this,function(_6){
this.flashMedia=_6;
this.isPlaying=this.autoPlay;
this.isStopped=!this.autoPlay;
this.onLoad(this.flashMedia);
});
},load:function(_7){
if(dojox.timing.doLater(this.flashMedia,this)){
return false;
}
if(!_7.url){
throw new Error("An url is required for loading media");
return false;
}else{
_7.url=this._normalizeUrl(_7.url);
}
this.flashMedia.load(_7);
return _7.url;
},doPlay:function(_8){
this.flashMedia.doPlay(_8);
},pause:function(_9){
this.flashMedia.pause(_9);
},stop:function(_a){
this.flashMedia.doStop(_a);
},setVolume:function(_b){
this.flashMedia.setVolume(_b);
},setPan:function(_c){
this.flashMedia.setPan(_c);
},getVolume:function(_d){
return this.flashMedia.getVolume(_d);
},getPan:function(_e){
return this.flashMedia.getPan(_e);
},getPosition:function(_f){
return this.flashMedia.getPosition(_f);
},onError:function(msg){
console.warn("SWF ERROR:",msg);
},onLoadStatus:function(_10){
},onAllLoaded:function(){
},onPlayStatus:function(_11){
},onComplete:function(_12){
},onLoad:function(){
},onID3:function(evt){
},destroy:function(){
if(!this.flashMedia){
this._cons.push(_1.connect(this,"onLoad",this,"destroy"));
return;
}
_1.forEach(this._subs,function(s){
_1.unsubscribe(s);
});
_1.forEach(this._cons,function(c){
_1.disconnect(c);
});
this._flashObject.destroy();
},_sub:function(_13,_14){
_1.subscribe(this.id+"/"+_13,this,_14);
},_normalizeVolume:function(vol){
if(vol>1){
while(vol>1){
vol*=0.1;
}
}
return vol;
},_normalizeUrl:function(_15){
if(_15&&_15.toLowerCase().indexOf("http")<0){
var loc=window.location.href.split("/");
loc.pop();
loc=loc.join("/")+"/";
_15=loc+_15;
}
return _15;
}});
return dojox.av.FLAudio;
});
