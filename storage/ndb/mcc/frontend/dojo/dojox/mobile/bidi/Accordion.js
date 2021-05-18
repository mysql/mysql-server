//>>built
define("dojox/mobile/bidi/Accordion",["dojo/_base/declare","./common","dojo/dom-class"],function(_1,_2,_3){
return _1(null,{_setupChild:function(_4){
if(this.textDir){
_4.label=_2.enforceTextDirWithUcc(_4.label,this.textDir);
}
this.inherited(arguments);
},_setIconDir:function(_5){
_3.add(_5,"mblAccordionIconParentRtl");
}});
});
