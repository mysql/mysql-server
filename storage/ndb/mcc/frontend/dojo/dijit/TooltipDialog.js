//>>built
require({cache:{"url:dijit/templates/TooltipDialog.html":"<div role=\"alertdialog\" tabIndex=\"-1\">\n\t<div class=\"dijitTooltipContainer\" role=\"presentation\">\n\t\t<div class=\"dijitTooltipContents dijitTooltipFocusNode\" data-dojo-attach-point=\"containerNode\"></div>\n\t</div>\n\t<div class=\"dijitTooltipConnector\" role=\"presentation\" data-dojo-attach-point=\"connectorNode\"></div>\n</div>\n"}});
define("dijit/TooltipDialog",["dojo/_base/declare","dojo/dom-class","dojo/_base/event","dojo/keys","dojo/_base/lang","./focus","./layout/ContentPane","./_DialogMixin","./form/_FormMixin","./_TemplatedMixin","dojo/text!./templates/TooltipDialog.html","./main"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _1("dijit.TooltipDialog",[_7,_a,_9,_8],{title:"",doLayout:false,autofocus:true,baseClass:"dijitTooltipDialog",_firstFocusItem:null,_lastFocusItem:null,templateString:_b,_setTitleAttr:function(_d){
this.containerNode.title=_d;
this._set("title",_d);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.containerNode,"onkeypress","_onKey");
},orient:function(_e,_f,_10){
var _11={"MR-ML":"dijitTooltipRight","ML-MR":"dijitTooltipLeft","TM-BM":"dijitTooltipAbove","BM-TM":"dijitTooltipBelow","BL-TL":"dijitTooltipBelow dijitTooltipABLeft","TL-BL":"dijitTooltipAbove dijitTooltipABLeft","BR-TR":"dijitTooltipBelow dijitTooltipABRight","TR-BR":"dijitTooltipAbove dijitTooltipABRight","BR-BL":"dijitTooltipRight","BL-BR":"dijitTooltipLeft"}[_f+"-"+_10];
_2.replace(this.domNode,_11,this._currentOrientClass||"");
this._currentOrientClass=_11;
},focus:function(){
this._getFocusItems(this.containerNode);
_6.focus(this._firstFocusItem);
},onOpen:function(pos){
this.orient(this.domNode,pos.aroundCorner,pos.corner);
var _12=pos.aroundNodePos;
if(pos.corner.charAt(0)=="M"&&pos.aroundCorner.charAt(0)=="M"){
this.connectorNode.style.top=_12.y+((_12.h-this.connectorNode.offsetHeight)>>1)-pos.y+"px";
this.connectorNode.style.left="";
}else{
if(pos.corner.charAt(1)=="M"&&pos.aroundCorner.charAt(1)=="M"){
this.connectorNode.style.left=_12.x+((_12.w-this.connectorNode.offsetWidth)>>1)-pos.x+"px";
}
}
this._onShow();
},onClose:function(){
this.onHide();
},_onKey:function(evt){
var _13=evt.target;
if(evt.charOrCode===_4.TAB){
this._getFocusItems(this.containerNode);
}
var _14=(this._firstFocusItem==this._lastFocusItem);
if(evt.charOrCode==_4.ESCAPE){
this.defer("onCancel");
_3.stop(evt);
}else{
if(_13==this._firstFocusItem&&evt.shiftKey&&evt.charOrCode===_4.TAB){
if(!_14){
_6.focus(this._lastFocusItem);
}
_3.stop(evt);
}else{
if(_13==this._lastFocusItem&&evt.charOrCode===_4.TAB&&!evt.shiftKey){
if(!_14){
_6.focus(this._firstFocusItem);
}
_3.stop(evt);
}else{
if(evt.charOrCode===_4.TAB){
evt.stopPropagation();
}
}
}
}
}});
});
