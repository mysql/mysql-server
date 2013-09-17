//>>built
define("dijit/form/_ComboBoxMenu",["dojo/_base/declare","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/keys","../_WidgetBase","../_TemplatedMixin","./_ComboBoxMenuMixin","./_ListMouseMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9){
return _1("dijit.form._ComboBoxMenu",[_6,_7,_9,_8],{templateString:"<div class='dijitReset dijitMenu' data-dojo-attach-point='containerNode' style='overflow: auto; overflow-x: hidden;'>"+"<div class='dijitMenuItem dijitMenuPreviousButton' data-dojo-attach-point='previousButton' role='option'></div>"+"<div class='dijitMenuItem dijitMenuNextButton' data-dojo-attach-point='nextButton' role='option'></div>"+"</div>",baseClass:"dijitComboBoxMenu",_createMenuItem:function(){
return _3.create("div",{"class":"dijitReset dijitMenuItem"+(this.isLeftToRight()?"":" dijitMenuItemRtl"),role:"option"});
},onHover:function(_a){
_2.add(_a,"dijitMenuItemHover");
},onUnhover:function(_b){
_2.remove(_b,"dijitMenuItemHover");
},onSelect:function(_c){
_2.add(_c,"dijitMenuItemSelected");
},onDeselect:function(_d){
_2.remove(_d,"dijitMenuItemSelected");
},_page:function(up){
var _e=0;
var _f=this.domNode.scrollTop;
var _10=_4.get(this.domNode,"height");
if(!this.getHighlightedOption()){
this.selectNextNode();
}
while(_e<_10){
if(up){
if(!this.getHighlightedOption().previousSibling||this._highlighted_option.previousSibling.style.display=="none"){
break;
}
this.selectPreviousNode();
}else{
if(!this.getHighlightedOption().nextSibling||this._highlighted_option.nextSibling.style.display=="none"){
break;
}
this.selectNextNode();
}
var _11=this.domNode.scrollTop;
_e+=(_11-_f)*(up?-1:1);
_f=_11;
}
},handleKey:function(evt){
switch(evt.charOrCode){
case _5.DOWN_ARROW:
this.selectNextNode();
return false;
case _5.PAGE_DOWN:
this._page(false);
return false;
case _5.UP_ARROW:
this.selectPreviousNode();
return false;
case _5.PAGE_UP:
this._page(true);
return false;
default:
return true;
}
}});
});
