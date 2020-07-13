//>>built
define("dojox/mobile/Audio",["dojo/_base/declare","dojo/dom-construct","dojo/sniff","dijit/_Contained","dijit/_WidgetBase"],function(_1,_2,_3,_4,_5){
return _1("dojox.mobile.Audio",[_5,_4],{source:null,width:"200px",height:"15px",_playable:false,_tag:"audio",constructor:function(){
this.source=[];
},buildRendering:function(){
this.domNode=this.srcNodeRef||_2.create(this._tag);
},_getEmbedRegExp:function(){
return _3("ff")?/audio\/mpeg/i:_3("ie")?/audio\/wav/i:null;
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
var i,_6,re;
if(this.domNode.canPlayType){
if(this.source.length>0){
for(i=0,_6=this.source.length;i<_6;i++){
_2.create("source",{src:this.source[i].src,type:this.source[i].type},this.domNode);
this._playable=this._playable||!!this.domNode.canPlayType(this.source[i].type);
}
}else{
for(i=0,_6=this.domNode.childNodes.length;i<_6;i++){
var n=this.domNode.childNodes[i];
if(n.nodeType===1&&n.nodeName==="SOURCE"){
this.source.push({src:n.src,type:n.type});
this._playable=this._playable||!!this.domNode.canPlayType(n.type);
}
}
}
}
_3.add("mobile-embed-audio-video-support",true);
if(_3("mobile-embed-audio-video-support")){
if(!this._playable){
for(i=0,_6=this.source.length,re=this._getEmbedRegExp();i<_6;i++){
if(this.source[i].type.match(re)){
var _7=_2.create("embed",{src:this.source[0].src,type:this.source[0].type,width:this.width,height:this.height});
this.domNode.parentNode.replaceChild(_7,this.domNode);
this.domNode=_7;
this._playable=true;
break;
}
}
}
}
}});
});
