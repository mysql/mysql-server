//>>built
define("dojox/av/widget/Player",["dojo","dijit","dijit/_Widget","dijit/_TemplatedMixin"],function(_1,_2,_3,_4){
_1.experimental("dojox.av.widget.Player");
return _1.declare("dojox.av.widget.Player",[_3,_4],{playerWidth:"480px",widgetsInTemplate:true,templateString:_1.cache("dojox.av.widget","resources/Player.html"),_fillContent:function(){
if(!this.items&&this.srcNodeRef){
this.items=[];
var _5=_1.query("*",this.srcNodeRef);
_1.forEach(_5,function(n){
this.items.push(n);
},this);
}
},postCreate:function(){
_1.style(this.domNode,"width",this.playerWidth+(_1.isString(this.playerWidth)?"":"px"));
if(_1.isString(this.playerWidth)&&this.playerWidth.indexOf("%")){
_1.connect(window,"resize",this,"onResize");
}
this.children=[];
var _6;
_1.forEach(this.items,function(n,i){
n.id=_2.getUniqueId("player_control");
switch(_1.attr(n,"controlType")){
case "play":
this.playContainer.appendChild(n);
break;
case "volume":
this.controlsBottom.appendChild(n);
break;
case "status":
this.statusContainer.appendChild(n);
break;
case "progress":
case "slider":
this.progressContainer.appendChild(n);
break;
case "video":
this.mediaNode=n;
this.playerScreen.appendChild(n);
break;
default:
}
this.items[i]=n.id;
},this);
},startup:function(){
this.media=_2.byId(this.mediaNode.id);
if(!_1.isAIR){
_1.style(this.media.domNode,"width","100%");
_1.style(this.media.domNode,"height","100%");
}
_1.forEach(this.items,function(id){
if(id!==this.mediaNode.id){
var _7=_2.byId(id);
this.children.push(_7);
if(_7){
_7.setMedia(this.media,this);
}
}
},this);
},onResize:function(_8){
var _9=_1.marginBox(this.domNode);
if(this.media&&this.media.onResize!==null){
this.media.onResize(_9);
}
_1.forEach(this.children,function(_a){
if(_a.onResize){
_a.onResize(_9);
}
});
}});
});
