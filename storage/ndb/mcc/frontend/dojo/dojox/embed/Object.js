//>>built
define("dojox/embed/Object",["dojo/_base/kernel","dojo/_base/declare","dojo/dom-geometry","dijit/_Widget","./Flash","./Quicktime"],function(_1,_2,_3,_4,_5,_6){
_1.experimental("dojox.embed.Object");
return _1.declare("dojox.embed.Object",_4,{width:0,height:0,src:"",movie:null,params:null,reFlash:/\.swf|\.flv/gi,reQtMovie:/\.3gp|\.avi|\.m4v|\.mov|\.mp4|\.mpg|\.mpeg|\.qt/gi,reQtAudio:/\.aiff|\.aif|\.m4a|\.m4b|\.m4p|\.midi|\.mid|\.mp3|\.mpa|\.wav/gi,postCreate:function(){
if(!this.width||!this.height){
var _7=_3.getMarginBox(this.domNode);
this.width=_7.w,this.height=_7.h;
}
var em=_5;
if(this.src.match(this.reQtMovie)||this.src.match(this.reQtAudio)){
em=_6;
}
if(!this.params){
this.params={};
if(this.domNode.hasAttributes()){
var _8={dojoType:"",width:"",height:"","class":"",style:"",id:"",src:""};
var _9=this.domNode.attributes;
for(var i=0,l=_9.length;i<l;i++){
if(!_8[_9[i].name]){
this.params[_9[i].name]=_9[i].value;
}
}
}
}
var _a={path:this.src,width:this.width,height:this.height,params:this.params};
this.movie=new (em)(_a,this.domNode);
}});
});
