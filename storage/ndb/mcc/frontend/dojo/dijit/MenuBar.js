//>>built
require({cache:{"url:dijit/templates/MenuBar.html":"<div class=\"dijitMenuBar dijitMenuPassive\" data-dojo-attach-point=\"containerNode\"  role=\"menubar\" tabIndex=\"${tabIndex}\" data-dojo-attach-event=\"onkeypress: _onKeyPress\"></div>\n"}});
define("dijit/MenuBar",["dojo/_base/declare","dojo/_base/event","dojo/keys","./_MenuBase","dojo/text!./templates/MenuBar.html"],function(_1,_2,_3,_4,_5){
return _1("dijit.MenuBar",_4,{templateString:_5,baseClass:"dijitMenuBar",_isMenuBar:true,postCreate:function(){
this.inherited(arguments);
var l=this.isLeftToRight();
this.connectKeyNavHandlers(l?[_3.LEFT_ARROW]:[_3.RIGHT_ARROW],l?[_3.RIGHT_ARROW]:[_3.LEFT_ARROW]);
this._orient=["below"];
},_moveToPopup:function(_6){
if(this.focusedChild&&this.focusedChild.popup&&!this.focusedChild.disabled){
this.onItemClick(this.focusedChild,_6);
}
},focusChild:function(_7){
var _8=this.focusedChild,_9=_8&&_8.popup&&_8.popup.isShowingNow;
this.inherited(arguments);
if(_9&&_7.popup&&!_7.disabled){
this._openPopup(true);
}
},_onKeyPress:function(_a){
if(_a.ctrlKey||_a.altKey){
return;
}
switch(_a.charOrCode){
case _3.DOWN_ARROW:
this._moveToPopup(_a);
_2.stop(_a);
}
},onItemClick:function(_b,_c){
if(_b.popup&&_b.popup.isShowingNow&&(_c.type!=="keypress"||_c.keyCode!==_3.DOWN_ARROW)){
_b.popup.onCancel();
}else{
this.inherited(arguments);
}
}});
});
