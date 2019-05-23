//>>built
define("dojox/widget/FisheyeListItem",["dojo/_base/declare","dojo/_base/sniff","dojo/dom","dojo/dom-attr","dojo/dom-class","dojo/dom-style","dojo/dom-construct","dojo/_base/window","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_Contained"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b){
return _1("dojox.widget.FisheyeListItem",[_9,_a,_b],{iconSrc:"",label:"",id:"",templateString:"<div class=\"dojoxFisheyeListItem\">"+"  <img class=\"dojoxFisheyeListItemImage\" data-dojo-attach-point=\"imgNode\" data-dojo-attach-event=\"onmouseover:onMouseOver,onmouseout:onMouseOut,onclick:onClick\">"+"  <div class=\"dojoxFisheyeListItemLabel\" data-dojo-attach-point=\"lblNode\"></div>"+"</div>",_isNode:function(wh){
if(typeof Element=="function"){
try{
return wh instanceof Element;
}
catch(e){
}
}else{
return wh&&!isNaN(wh.nodeType);
}
return false;
},_hasParent:function(_c){
return Boolean(_c&&_c.parentNode&&this._isNode(_c.parentNode));
},postCreate:function(){
var _d;
if((this.iconSrc.toLowerCase().substring(this.iconSrc.length-4)==".png")&&_2("ie")<7){
if(this._hasParent(this.imgNode)&&this.id!=""){
_d=this.imgNode.parentNode;
_4.set(_d,"id",this.id);
}
_6.set(this.imgNode,"filter","progid:DXImageTransform.Microsoft.AlphaImageLoader(src='"+this.iconSrc+"', sizingMethod='scale')");
this.imgNode.src=this._blankGif.toString();
}else{
if(this._hasParent(this.imgNode)&&this.id!=""){
_d=this.imgNode.parentNode;
_4.set(_d,"id",this.id);
}
this.imgNode.src=this.iconSrc;
}
if(this.lblNode){
_7.place(_8.doc.createTextNode(this.label),this.lblNode);
}
_3.setSelectable(this.domNode,false);
this.startup();
},startup:function(){
this.parent=this.getParent();
},onMouseOver:function(e){
if(!this.parent.isOver){
this.parent._setActive(e);
}
if(this.label!=""){
_5.add(this.lblNode,"dojoxFishSelected");
this.parent._positionLabel(this);
}
},onMouseOut:function(e){
_5.remove(this.lblNode,"dojoxFishSelected");
},onClick:function(e){
}});
});
