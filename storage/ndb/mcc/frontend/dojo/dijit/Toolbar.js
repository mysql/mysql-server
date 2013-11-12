//>>built
define("dijit/Toolbar",["require","dojo/_base/declare","dojo/_base/kernel","dojo/keys","dojo/ready","./_Widget","./_KeyNavContainer","./_TemplatedMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8){
if(!_3.isAsync){
_5(0,function(){
var _9=["dijit/ToolbarSeparator"];
_1(_9);
});
}
return _2("dijit.Toolbar",[_6,_8,_7],{templateString:"<div class=\"dijit\" role=\"toolbar\" tabIndex=\"${tabIndex}\" data-dojo-attach-point=\"containerNode\">"+"</div>",baseClass:"dijitToolbar",postCreate:function(){
this.inherited(arguments);
this.connectKeyNavHandlers(this.isLeftToRight()?[_4.LEFT_ARROW]:[_4.RIGHT_ARROW],this.isLeftToRight()?[_4.RIGHT_ARROW]:[_4.LEFT_ARROW]);
}});
});
