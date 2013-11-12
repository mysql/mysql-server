//>>built
require({cache:{"url:dijit/templates/CheckedMenuItem.html":"<tr class=\"dijitReset dijitMenuItem\" data-dojo-attach-point=\"focusNode\" role=\"menuitemcheckbox\" tabIndex=\"-1\"\n\t\tdata-dojo-attach-event=\"onmouseenter:_onHover,onmouseleave:_onUnhover,ondijitclick:_onClick\">\n\t<td class=\"dijitReset dijitMenuItemIconCell\" role=\"presentation\">\n\t\t<img src=\"${_blankGif}\" alt=\"\" class=\"dijitMenuItemIcon dijitCheckedMenuItemIcon\" data-dojo-attach-point=\"iconNode\"/>\n\t\t<span class=\"dijitCheckedMenuItemIconChar\">&#10003;</span>\n\t</td>\n\t<td class=\"dijitReset dijitMenuItemLabel\" colspan=\"2\" data-dojo-attach-point=\"containerNode,labelNode\"></td>\n\t<td class=\"dijitReset dijitMenuItemAccelKey\" style=\"display: none\" data-dojo-attach-point=\"accelKeyNode\"></td>\n\t<td class=\"dijitReset dijitMenuArrowCell\" role=\"presentation\">&#160;</td>\n</tr>\n"}});
define("dijit/CheckedMenuItem",["dojo/_base/declare","dojo/dom-class","./MenuItem","dojo/text!./templates/CheckedMenuItem.html","./hccss"],function(_1,_2,_3,_4){
return _1("dijit.CheckedMenuItem",_3,{templateString:_4,checked:false,_setCheckedAttr:function(_5){
_2.toggle(this.domNode,"dijitCheckedMenuItemChecked",_5);
this.domNode.setAttribute("aria-checked",_5);
this._set("checked",_5);
},iconClass:"",onChange:function(){
},_onClick:function(e){
if(!this.disabled){
this.set("checked",!this.checked);
this.onChange(this.checked);
}
this.inherited(arguments);
}});
});
