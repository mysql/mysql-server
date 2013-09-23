//>>built
require({cache:{"url:dijit/templates/TooltipDialog.html":"<div role=\"presentation\" tabIndex=\"-1\">\n\t<div class=\"dijitTooltipContainer\" role=\"presentation\">\n\t\t<div class =\"dijitTooltipContents dijitTooltipFocusNode\" data-dojo-attach-point=\"containerNode\" role=\"dialog\"></div>\n\t</div>\n\t<div class=\"dijitTooltipConnector\" role=\"presentation\"></div>\n</div>\n"}});
define("dijit/TooltipDialog",["dojo/_base/declare","dojo/dom-class","dojo/_base/event","dojo/keys","dojo/_base/lang","./focus","./layout/ContentPane","./_DialogMixin","./form/_FormMixin","./_TemplatedMixin","dojo/text!./templates/TooltipDialog.html","."],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c){
return _1("dijit.TooltipDialog",[_7,_a,_9,_8],{title:"",doLayout:false,autofocus:true,baseClass:"dijitTooltipDialog",_firstFocusItem:null,_lastFocusItem:null,templateString:_b,_setTitleAttr:function(_d){
this.containerNode.title=_d;
this._set("title",_d);
},postCreate:function(){
this.inherited(arguments);
this.connect(this.containerNode,"onkeypress","_onKey");
},orient:function(_e,_f,_10){
var _11="dijitTooltipAB"+(_10.charAt(1)=="L"?"Left":"Right")+" dijitTooltip"+(_10.charAt(0)=="T"?"Below":"Above");
_2.replace(this.domNode,_11,this._currentOrientClass||"");
this._currentOrientClass=_11;
},focus:function(){
this._getFocusItems(this.containerNode);
_6.focus(this._firstFocusItem);
},onOpen:function(pos){
this.orient(this.domNode,pos.aroundCorner,pos.corner);
this._onShow();
},onClose:function(){
this.onHide();
},_onKey:function(evt){
var _12=evt.target;
if(evt.charOrCode===_4.TAB){
this._getFocusItems(this.containerNode);
}
var _13=(this._firstFocusItem==this._lastFocusItem);
if(evt.charOrCode==_4.ESCAPE){
setTimeout(_5.hitch(this,"onCancel"),0);
_3.stop(evt);
}else{
if(_12==this._firstFocusItem&&evt.shiftKey&&evt.charOrCode===_4.TAB){
if(!_13){
_6.focus(this._lastFocusItem);
}
_3.stop(evt);
}else{
if(_12==this._lastFocusItem&&evt.charOrCode===_4.TAB&&!evt.shiftKey){
if(!_13){
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
