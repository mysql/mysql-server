//>>built
define("dijit/_OnDijitClickMixin",["dojo/on","dojo/_base/array","dojo/keys","dojo/_base/declare","dojo/has","dojo/_base/unload","dojo/_base/window","./a11yclick"],function(on,_1,_2,_3,_4,_5,_6,_7){
var _8=_3("dijit._OnDijitClickMixin",null,{connect:function(_9,_a,_b){
return this.inherited(arguments,[_9,_a=="ondijitclick"?_7:_a,_b]);
}});
_8.a11yclick=_7;
return _8;
});
