//>>built
require({cache:{"url:dijit/templates/MenuBar.html":"<div class=\"dijitMenuBar dijitMenuPassive\" data-dojo-attach-point=\"containerNode\"  role=\"menubar\" tabIndex=\"${tabIndex}\" data-dojo-attach-event=\"onkeypress: _onKeyPress\"></div>\n"}});
define("dijit/MenuBar",["dojo/_base/declare","dojo/_base/event","dojo/keys","./_MenuBase","dojo/text!./templates/MenuBar.html"],function(_1,_2,_3,_4,_5){
return _1("dijit.MenuBar",_4,{templateString:_5,baseClass:"dijitMenuBar",_isMenuBar:true,postCreate:function(){
var l=this.isLeftToRight();
this.connectKeyNavHandlers(l?[_3.LEFT_ARROW]:[_3.RIGHT_ARROW],l?[_3.RIGHT_ARROW]:[_3.LEFT_ARROW]);
this._orient=["below"];
},focusChild:function(_6){
var _7=this.focusedChild,_8=_7&&_7.popup&&_7.popup.isShowingNow;
this.inherited(arguments);
if(_8&&_6.popup&&!_6.disabled){
this._openPopup();
}
},_onKeyPress:function(_9){
if(_9.ctrlKey||_9.altKey){
return;
}
switch(_9.charOrCode){
case _3.DOWN_ARROW:
this._moveToPopup(_9);
_2.stop(_9);
}
},onItemClick:function(_a,_b){
if(_a.popup&&_a.popup.isShowingNow){
_a.popup.onCancel();
}else{
this.inherited(arguments);
}
}});
});
