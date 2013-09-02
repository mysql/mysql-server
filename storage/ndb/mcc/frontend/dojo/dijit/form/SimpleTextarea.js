//>>built
define("dijit/form/SimpleTextarea",["dojo/_base/declare","dojo/dom-class","dojo/_base/sniff","dojo/_base/window","./TextBox"],function(_1,_2,_3,_4,_5){
return _1("dijit.form.SimpleTextarea",_5,{baseClass:"dijitTextBox dijitTextArea",rows:"3",cols:"20",templateString:"<textarea ${!nameAttrSetting} data-dojo-attach-point='focusNode,containerNode,textbox' autocomplete='off'></textarea>",postMixInProperties:function(){
if(!this.value&&this.srcNodeRef){
this.value=this.srcNodeRef.value;
}
this.inherited(arguments);
},buildRendering:function(){
this.inherited(arguments);
if(_3("ie")&&this.cols){
_2.add(this.textbox,"dijitTextAreaCols");
}
},filter:function(_6){
if(_6){
_6=_6.replace(/\r/g,"");
}
return this.inherited(arguments);
},_onInput:function(e){
if(this.maxLength){
var _7=parseInt(this.maxLength);
var _8=this.textbox.value.replace(/\r/g,"");
var _9=_8.length-_7;
if(_9>0){
var _a=this.textbox;
if(_a.selectionStart){
var _b=_a.selectionStart;
var cr=0;
if(_3("opera")){
cr=(this.textbox.value.substring(0,_b).match(/\r/g)||[]).length;
}
this.textbox.value=_8.substring(0,_b-_9-cr)+_8.substring(_b-cr);
_a.setSelectionRange(_b-_9,_b-_9);
}else{
if(_4.doc.selection){
_a.focus();
var _c=_4.doc.selection.createRange();
_c.moveStart("character",-_9);
_c.text="";
_c.select();
}
}
}
}
this.inherited(arguments);
}});
});
