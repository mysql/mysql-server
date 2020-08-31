//>>built
define("dojox/form/ListInput",["dojo/_base/kernel","dojo/_base/lang","dojo/_base/array","dojo/_base/json","dojo/_base/fx","dojo/_base/window","dojo/_base/connect","dojo/dom-class","dojo/dom-style","dojo/dom-construct","dojo/dom-geometry","dojo/keys","dijit/_Widget","dijit/_TemplatedMixin","dijit/form/_FormValueWidget","dijit/form/ValidationTextBox","dijit/InlineEditBox","dojo/i18n!dijit/nls/common","dojo/_base/declare"],function(_1,_2,_3,_4,fx,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12){
_1.experimental("dojox.form.ListInput");
var _13=_12("dojox.form.ListInput",[_e],{constructor:function(){
this._items=[];
if(!_2.isArray(this.delimiter)){
this.delimiter=[this.delimiter];
}
var r="("+this.delimiter.join("|")+")?";
this.regExp="^"+this.regExp+r+"$";
},inputClass:"dojox.form._ListInputInputBox",inputHandler:"onChange",inputProperties:{minWidth:50},submitOnlyValidValue:true,useOnBlur:true,readOnlyInput:false,maxItems:null,showCloseButtonWhenValid:true,showCloseButtonWhenInvalid:true,regExp:".*",delimiter:",",constraints:{},baseClass:"dojoxListInput",type:"select",value:"",templateString:"<div dojoAttachPoint=\"focusNode\" class=\"dijit dijitReset dijitLeft dojoxListInput\"><select dojoAttachpoint=\"_selectNode\" multiple=\"multiple\" class=\"dijitHidden\" ${!nameAttrSetting}></select><ul dojoAttachPoint=\"_listInput\"><li dojoAttachEvent=\"onclick: _onClick\" class=\"dijitInputField dojoxListInputNode dijitHidden\" dojoAttachPoint=\"_inputNode\"></li></ul></div>",_setNameAttr:"_selectNode",useAnim:true,duration:500,easingIn:null,easingOut:null,readOnlyItem:false,useArrowForEdit:true,_items:null,_lastAddedItem:null,_currentItem:null,_input:null,_count:0,postCreate:function(){
this.inherited(arguments);
this._createInputBox();
},_setReadOnlyInputAttr:function(_14){
if(!this._started){
return this._createInputBox();
}
this.readOnlyInput=_14;
this._createInputBox();
},_setReadOnlyItemAttr:function(_15){
if(!this._started){
return;
}
for(var i in this._items){
this._items[i].set("readOnlyItem",_15);
}
},_createInputBox:function(){
_7.toggle(this._inputNode,"dijitHidden",this.readOnlyInput);
if(this.readOnlyInput){
return;
}
if(this._input){
return;
}
if(this.inputHandler===null){
console.warn("you must add some handler to connect to input field");
return false;
}
if(_2.isString(this.inputHandler)){
this.inputHandler=this.inputHandler.split(",");
}
if(_2.isString(this.inputProperties)){
this.inputProperties=_4.fromJson(this.inputProperties);
}
var _16=_2.getObject(this.inputClass,false);
this.inputProperties.regExp=this.regExpGen(this.constraints);
this._input=new _16(this.inputProperties);
this._input.startup();
this._inputNode.appendChild(this._input.domNode);
_3.forEach(this.inputHandler,function(_17){
this.connect(this._input,_2.trim(_17),"_onHandler");
},this);
this.connect(this._input,"onKeyDown","_inputOnKeyDown");
this.connect(this._input,"onBlur","_inputOnBlur");
},compare:function(_18,_19){
_18=_18.join(",");
_19=_19.join(",");
if(_18>_19){
return 1;
}else{
if(_18<_19){
return -1;
}else{
return 0;
}
}
},add:function(_1a){
if(this._count>=this.maxItems&&this.maxItems!==null){
return;
}
this._lastValueReported=this._getValues();
if(!_2.isArray(_1a)){
_1a=[_1a];
}
for(var i in _1a){
var _1b=_1a[i];
if(_1b===""||typeof _1b!="string"){
continue;
}
this._count++;
var re=new RegExp(this.regExpGen(this.constraints));
this._lastAddedItem=new _1c({"index":this._items.length,readOnlyItem:this.readOnlyItem,value:_1b,regExp:this.regExpGen(this.constraints)});
this._lastAddedItem.startup();
this._testItem(this._lastAddedItem,_1b);
this._lastAddedItem.onClose=_2.hitch(this,"_onItemClose",this._lastAddedItem);
this._lastAddedItem.onChange=_2.hitch(this,"_onItemChange",this._lastAddedItem);
this._lastAddedItem.onEdit=_2.hitch(this,"_onItemEdit",this._lastAddedItem);
this._lastAddedItem.onKeyDown=_2.hitch(this,"_onItemKeyDown",this._lastAddedItem);
if(this.useAnim){
_8.set(this._lastAddedItem.domNode,{opacity:0,display:""});
}
this._placeItem(this._lastAddedItem.domNode);
if(this.useAnim){
var _1d=fx.fadeIn({node:this._lastAddedItem.domNode,duration:this.duration,easing:this.easingIn}).play();
}
this._items[this._lastAddedItem.index]=this._lastAddedItem;
if(this._onChangeActive&&this.intermediateChanges){
this.onChange(_1b);
}
if(this._count>=this.maxItems&&this.maxItems!==null){
break;
}
}
this._updateValues();
if(this._lastValueReported.length==0){
this._lastValueReported=this.value;
}
if(!this.readOnlyInput){
this._input.set("value","");
}
if(this._onChangeActive){
this.onChange(this.value);
}
this._setReadOnlyWhenMaxItemsReached();
},_setReadOnlyWhenMaxItemsReached:function(){
this.set("readOnlyInput",(this._count>=this.maxItems&&this.maxItems!==null));
},_setSelectNode:function(){
this._selectNode.options.length=0;
var _1e=this.submitOnlyValidValue?this.get("MatchedValue"):this.value;
if(!_2.isArray(_1e)){
return;
}
_3.forEach(_1e,function(_1f){
this._selectNode.options[this._selectNode.options.length]=new Option(_1f,_1f,true,true);
},this);
},_placeItem:function(_20){
_9.place(_20,this._inputNode,"before");
},_getCursorPos:function(_21){
if(typeof _21.selectionStart!="undefined"){
return _21.selectionStart;
}
try{
_21.focus();
}
catch(e){
}
var _22=_21.createTextRange();
_22.moveToBookmark(_5.doc.selection.createRange().getBookmark());
_22.moveEnd("character",_21.value.length);
try{
return _21.value.length-_22.text.length;
}
finally{
_22=null;
}
},_onItemClose:function(_23){
if(this.disabled){
return;
}
if(this.useAnim){
var _24=fx.fadeOut({node:_23.domNode,duration:this.duration,easing:this.easingOut,onEnd:_2.hitch(this,"_destroyItem",_23)}).play();
}else{
this._destroyItem(_23);
}
},_onItemKeyDown:function(_25,e){
if(this.readOnlyItem||!this.useArrowForEdit){
return;
}
if(e.keyCode==_b.LEFT_ARROW&&this._getCursorPos(e.target)==0){
this._editBefore(_25);
}else{
if(e.keyCode==_b.RIGHT_ARROW&&this._getCursorPos(e.target)==e.target.value.length){
this._editAfter(_25);
}
}
},_editBefore:function(_26){
this._currentItem=this._getPreviousItem(_26);
if(this._currentItem!==null){
this._currentItem.edit();
}
},_editAfter:function(_27){
this._currentItem=this._getNextItem(_27);
if(this._currentItem!==null){
this._currentItem.edit();
}
if(!this.readOnlyInput){
if(this._currentItem===null){
this._focusInput();
}
}
},_onItemChange:function(_28,_29){
_29=_29||_28.get("value");
this._testItem(_28,_29);
this._updateValues();
},_onItemEdit:function(_2a){
_7.remove(_2a.domNode,["dijitError",this.baseClass+"Match",this.baseClass+"Mismatch"]);
},_testItem:function(_2b,_2c){
var re=new RegExp(this.regExpGen(this.constraints));
var _2d=(""+_2c).match(re);
_7.remove(_2b.domNode,this.baseClass+(!_2d?"Match":"Mismatch"));
_7.add(_2b.domNode,this.baseClass+(_2d?"Match":"Mismatch"));
_7.toggle(_2b.domNode,"dijitError",!_2d);
if((this.showCloseButtonWhenValid&&_2d)||(this.showCloseButtonWhenInvalid&&!_2d)){
_7.add(_2b.domNode,this.baseClass+"Closable");
}else{
_7.remove(_2b.domNode,this.baseClass+"Closable");
}
},_getValueAttr:function(){
return this.value;
},_setValueAttr:function(_2e){
this._destroyAllItems();
this.add(this._parseValue(_2e));
},_parseValue:function(_2f){
if(typeof _2f=="string"){
if(_2.isString(this.delimiter)){
this.delimiter=[this.delimiter];
}
var re=new RegExp("^.*("+this.delimiter.join("|")+").*");
if(_2f.match(re)){
re=new RegExp(this.delimiter.join("|"));
return _2f.split(re);
}
}
return _2f;
},regExpGen:function(_30){
return this.regExp;
},_setDisabledAttr:function(_31){
if(!this.readOnlyItem){
for(var i in this._items){
this._items[i].set("disabled",_31);
}
}
if(!this.readOnlyInput){
this._input.set("disabled",_31);
}
this.inherited(arguments);
},_onHandler:function(_32){
var _33=this._parseValue(_32);
if(_2.isArray(_33)){
this.add(_33);
}
},_onClick:function(e){
this._focusInput();
},_focusInput:function(){
if(!this.readOnlyInput&&this._input.focus){
this._input.focus();
}
},_inputOnKeyDown:function(e){
this._currentItem=null;
var val=this._input.get("value");
if(e.keyCode==_b.BACKSPACE&&val==""&&this.get("lastItem")){
this._destroyItem(this.get("lastItem"));
}else{
if(e.keyCode==_b.ENTER&&val!=""){
this.add(val);
}else{
if(e.keyCode==_b.LEFT_ARROW&&this._getCursorPos(this._input.focusNode)==0&&!this.readOnlyItem&&this.useArrowForEdit){
this._editBefore();
}
}
}
},_inputOnBlur:function(){
var val=this._input.get("value");
if(this.useOnBlur&&val!=""){
this.add(val);
}
},_getMatchedValueAttr:function(){
return this._getValues(_2.hitch(this,this._matchValidator));
},_getMismatchedValueAttr:function(){
return this._getValues(_2.hitch(this,this._mismatchValidator));
},_getValues:function(_34){
var _35=[];
_34=_34||this._nullValidator;
for(var i in this._items){
var _36=this._items[i];
if(_36===null){
continue;
}
var _37=_36.get("value");
if(_34(_37)){
_35.push(_37);
}
}
return _35;
},_nullValidator:function(_38){
return true;
},_matchValidator:function(_39){
var re=new RegExp(this.regExpGen(this.constraints));
return _39.match(re);
},_mismatchValidator:function(_3a){
var re=new RegExp(this.regExpGen(this.constraints));
return !(_3a.match(re));
},_getLastItemAttr:function(){
return this._getSomeItem();
},_getSomeItem:function(_3b,_3c){
_3b=_3b||false;
_3c=_3c||"last";
var _3d=null;
var _3e=-1;
for(var i in this._items){
if(this._items[i]===null){
continue;
}
if(_3c=="before"&&this._items[i]===_3b){
break;
}
_3d=this._items[i];
if(_3c=="first"||_3e==0){
_3e=1;
break;
}
if(_3c=="after"&&this._items[i]===_3b){
_3e=0;
}
}
if(_3c=="after"&&_3e==0){
_3d=null;
}
return _3d;
},_getPreviousItem:function(_3f){
return this._getSomeItem(_3f,"before");
},_getNextItem:function(_40){
return this._getSomeItem(_40,"after");
},_destroyItem:function(_41,_42){
this._items[_41.index]=null;
_41.destroy();
this._count--;
if(_42!==false){
this._updateValues();
this._setReadOnlyWhenMaxItemsReached();
}
},_updateValues:function(){
this.value=this._getValues();
this._setSelectNode();
},_destroyAllItems:function(){
for(var i in this._items){
if(this._items[i]==null){
continue;
}
this._destroyItem(this._items[i],false);
}
this._items=[];
this._count=0;
this.value=null;
this._setSelectNode();
this._setReadOnlyWhenMaxItemsReached();
},destroy:function(){
this._destroyAllItems();
this._lastAddedItem=null;
if(!this._input){
this._input.destroy();
}
this.inherited(arguments);
}});
var _1c=_12("dojox.form._ListInputInputItem",[_c,_d],{templateString:"<li class=\"dijit dijitReset dijitLeft dojoxListInputItem\" dojoAttachEvent=\"onclick: onClick\" ><span dojoAttachPoint=\"labelNode\"></span></li>",closeButtonNode:null,readOnlyItem:true,baseClass:"dojoxListInputItem",value:"",regExp:".*",_editBox:null,_handleKeyDown:null,attributeMap:{value:{node:"labelNode",type:"innerHTML"}},postMixInProperties:function(){
var _43=_11;
_2.mixin(this,_43);
this.inherited(arguments);
},postCreate:function(){
this.inherited(arguments);
this.closeButtonNode=_9.create("span",{"class":"dijitButtonNode dijitDialogCloseIcon",title:this.itemClose,onclick:_2.hitch(this,"onClose"),onmouseenter:_2.hitch(this,"_onCloseEnter"),onmouseleave:_2.hitch(this,"_onCloseLeave")},this.domNode);
_9.create("span",{"class":"closeText",title:this.itemClose,innerHTML:"x"},this.closeButtonNode);
},startup:function(){
this.inherited(arguments);
this._createInlineEditBox();
},_setReadOnlyItemAttr:function(_44){
this.readOnlyItem=_44;
if(!_44){
this._createInlineEditBox();
}else{
if(this._editBox){
this._editBox.set("disabled",true);
}
}
},_createInlineEditBox:function(){
if(this.readOnlyItem){
return;
}
if(!this._started){
return;
}
if(this._editBox){
this._editBox.set("disabled",false);
return;
}
this._editBox=new _10({value:this.value,editor:"dijit.form.ValidationTextBox",editorParams:{regExp:this.regExp}},this.labelNode);
this.connect(this._editBox,"edit","_onEdit");
this.connect(this._editBox,"onChange","_onCloseEdit");
this.connect(this._editBox,"onCancel","_onCloseEdit");
},edit:function(){
if(!this.readOnlyItem){
this._editBox.edit();
}
},_onCloseEdit:function(_45){
_7.remove(this.closeButtonNode,this.baseClass+"Edited");
_6.disconnect(this._handleKeyDown);
this.onChange(_45);
},_onEdit:function(){
_7.add(this.closeButtonNode,this.baseClass+"Edited");
this._handleKeyDown=_6.connect(this._editBox.editWidget,"_onKeyPress",this,"onKeyDown");
this.onEdit();
},_setDisabledAttr:function(_46){
if(!this.readOnlyItem){
this._editBox.set("disabled",_46);
}
},_getValueAttr:function(){
return (!this.readOnlyItem&&this._started?this._editBox.get("value"):this.value);
},destroy:function(){
if(this._editBox){
this._editBox.destroy();
}
this.inherited(arguments);
},_onCloseEnter:function(){
_7.add(this.closeButtonNode,"dijitDialogCloseIcon-hover");
},_onCloseLeave:function(){
_7.remove(this.closeButtonNode,"dijitDialogCloseIcon-hover");
},onClose:function(){
},onEdit:function(){
},onClick:function(){
},onChange:function(_47){
},onKeyDown:function(_48){
}});
var _49=_12("dojox.form._ListInputInputBox",[_f],{minWidth:50,intermediateChanges:true,regExp:".*",_sizer:null,onChange:function(_4a){
this.inherited(arguments);
if(this._sizer===null){
this._sizer=_9.create("div",{style:{position:"absolute",left:"-10000px",top:"-10000px"}},_5.body());
}
this._sizer.innerHTML=_4a;
var w=_a.getContentBox(this._sizer).w+this.minWidth;
_a.setContentSize(this.domNode,{w:w});
},destroy:function(){
_9.destroy(this._sizer);
this.inherited(arguments);
}});
return _13;
});
