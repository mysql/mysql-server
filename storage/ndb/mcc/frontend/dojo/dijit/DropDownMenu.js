//>>built
require({cache:{"url:dijit/templates/Menu.html":"<table class=\"dijit dijitMenu dijitMenuPassive dijitReset dijitMenuTable\" role=\"menu\" tabIndex=\"${tabIndex}\" data-dojo-attach-event=\"onkeypress:_onKeyPress\" cellspacing=\"0\">\n\t<tbody class=\"dijitReset\" data-dojo-attach-point=\"containerNode\"></tbody>\n</table>\n"}});
define("dijit/DropDownMenu",["dojo/_base/declare","dojo/_base/event","dojo/keys","dojo/text!./templates/Menu.html","./_OnDijitClickMixin","./_MenuBase"],function(_1,_2,_3,_4,_5,_6){
return _1("dijit.DropDownMenu",[_6,_5],{templateString:_4,baseClass:"dijitMenu",postCreate:function(){
var l=this.isLeftToRight();
this._openSubMenuKey=l?_3.RIGHT_ARROW:_3.LEFT_ARROW;
this._closeSubMenuKey=l?_3.LEFT_ARROW:_3.RIGHT_ARROW;
this.connectKeyNavHandlers([_3.UP_ARROW],[_3.DOWN_ARROW]);
},_onKeyPress:function(_7){
if(_7.ctrlKey||_7.altKey){
return;
}
switch(_7.charOrCode){
case this._openSubMenuKey:
this._moveToPopup(_7);
_2.stop(_7);
break;
case this._closeSubMenuKey:
if(this.parentMenu){
if(this.parentMenu._isMenuBar){
this.parentMenu.focusPrev();
}else{
this.onCancel(false);
}
}else{
_2.stop(_7);
}
break;
}
}});
});
