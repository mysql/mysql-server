//>>built
define("dijit/form/_AutoCompleterMixin",["dojo/_base/connect","dojo/data/util/filter","dojo/_base/declare","dojo/_base/Deferred","dojo/dom-attr","dojo/_base/event","dojo/keys","dojo/_base/lang","dojo/query","dojo/regexp","dojo/_base/sniff","dojo/string","dojo/_base/window","./DataList","../registry","./_TextBoxMixin"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10){
return _3("dijit.form._AutoCompleterMixin",null,{item:null,pageSize:Infinity,store:null,fetchProperties:{},query:{},autoComplete:true,highlightMatch:"first",searchDelay:100,searchAttr:"name",labelAttr:"",labelType:"text",queryExpr:"${0}*",ignoreCase:true,maxHeight:-1,_stopClickEvents:false,_getCaretPos:function(_11){
var pos=0;
if(typeof (_11.selectionStart)=="number"){
pos=_11.selectionStart;
}else{
if(_b("ie")){
var tr=_d.doc.selection.createRange().duplicate();
var ntr=_11.createTextRange();
tr.move("character",0);
ntr.move("character",0);
try{
ntr.setEndPoint("EndToEnd",tr);
pos=String(ntr.text).replace(/\r/g,"").length;
}
catch(e){
}
}
}
return pos;
},_setCaretPos:function(_12,_13){
_13=parseInt(_13);
_10.selectInputText(_12,_13,_13);
},_setDisabledAttr:function(_14){
this.inherited(arguments);
this.domNode.setAttribute("aria-disabled",_14);
},_abortQuery:function(){
if(this.searchTimer){
clearTimeout(this.searchTimer);
this.searchTimer=null;
}
if(this._fetchHandle){
if(this._fetchHandle.cancel){
this._cancelingQuery=true;
this._fetchHandle.cancel();
this._cancelingQuery=false;
}
this._fetchHandle=null;
}
},_onInput:function(evt){
this.inherited(arguments);
if(evt.charOrCode==229){
this._onKey(evt);
}
},_onKey:function(evt){
var key=evt.charOrCode;
if(evt.altKey||((evt.ctrlKey||evt.metaKey)&&(key!="x"&&key!="v"))||key==_7.SHIFT){
return;
}
var _15=false;
var pw=this.dropDown;
var _16=null;
this._prev_key_backspace=false;
this._abortQuery();
this.inherited(arguments);
if(this._opened){
_16=pw.getHighlightedOption();
}
switch(key){
case _7.PAGE_DOWN:
case _7.DOWN_ARROW:
case _7.PAGE_UP:
case _7.UP_ARROW:
if(this._opened){
this._announceOption(_16);
}
_6.stop(evt);
break;
case _7.ENTER:
if(_16){
if(_16==pw.nextButton){
this._nextSearch(1);
_6.stop(evt);
break;
}else{
if(_16==pw.previousButton){
this._nextSearch(-1);
_6.stop(evt);
break;
}
}
}else{
this._setBlurValue();
this._setCaretPos(this.focusNode,this.focusNode.value.length);
}
if(this._opened||this._fetchHandle){
_6.stop(evt);
}
case _7.TAB:
var _17=this.get("displayedValue");
if(pw&&(_17==pw._messages["previousMessage"]||_17==pw._messages["nextMessage"])){
break;
}
if(_16){
this._selectOption(_16);
}
case _7.ESCAPE:
if(this._opened){
this._lastQuery=null;
this.closeDropDown();
}
break;
case " ":
if(_16){
_6.stop(evt);
this._selectOption(_16);
this.closeDropDown();
}else{
_15=true;
}
break;
case _7.DELETE:
case _7.BACKSPACE:
this._prev_key_backspace=true;
_15=true;
break;
default:
_15=typeof key=="string"||key==229;
}
if(_15){
this.item=undefined;
this.searchTimer=setTimeout(_8.hitch(this,"_startSearchFromInput"),1);
}
},_autoCompleteText:function(_18){
var fn=this.focusNode;
_10.selectInputText(fn,fn.value.length);
var _19=this.ignoreCase?"toLowerCase":"substr";
if(_18[_19](0).indexOf(this.focusNode.value[_19](0))==0){
var _1a=this.autoComplete?this._getCaretPos(fn):fn.value.length;
if((_1a+1)>fn.value.length){
fn.value=_18;
_10.selectInputText(fn,_1a);
}
}else{
fn.value=_18;
_10.selectInputText(fn);
}
},_openResultList:function(_1b,_1c,_1d){
this._fetchHandle=null;
if(this.disabled||this.readOnly||(_1c[this.searchAttr]!==this._lastQuery)){
return;
}
var _1e=this.dropDown.getHighlightedOption();
this.dropDown.clearResultList();
if(!_1b.length&&_1d.start==0){
this.closeDropDown();
return;
}
var _1f=this.dropDown.createOptions(_1b,_1d,_8.hitch(this,"_getMenuLabelFromItem"));
this._showResultList();
if(_1d.direction){
if(1==_1d.direction){
this.dropDown.highlightFirstOption();
}else{
if(-1==_1d.direction){
this.dropDown.highlightLastOption();
}
}
if(_1e){
this._announceOption(this.dropDown.getHighlightedOption());
}
}else{
if(this.autoComplete&&!this._prev_key_backspace&&!/^[*]+$/.test(_1c[this.searchAttr].toString())){
this._announceOption(_1f[1]);
}
}
},_showResultList:function(){
this.closeDropDown(true);
this.openDropDown();
this.domNode.setAttribute("aria-expanded","true");
},loadDropDown:function(){
this._startSearchAll();
},isLoaded:function(){
return false;
},closeDropDown:function(){
this._abortQuery();
if(this._opened){
this.inherited(arguments);
this.domNode.setAttribute("aria-expanded","false");
this.focusNode.removeAttribute("aria-activedescendant");
}
},_setBlurValue:function(){
var _20=this.get("displayedValue");
var pw=this.dropDown;
if(pw&&(_20==pw._messages["previousMessage"]||_20==pw._messages["nextMessage"])){
this._setValueAttr(this._lastValueReported,true);
}else{
if(typeof this.item=="undefined"){
this.item=null;
this.set("displayedValue",_20);
}else{
if(this.value!=this._lastValueReported){
this._handleOnChange(this.value,true);
}
this._refreshState();
}
}
},_setItemAttr:function(_21,_22,_23){
var _24="";
if(_21){
if(!_23){
_23=this.store._oldAPI?this.store.getValue(_21,this.searchAttr):_21[this.searchAttr];
}
_24=this._getValueField()!=this.searchAttr?this.store.getIdentity(_21):_23;
}
this.set("value",_24,_22,_23,_21);
},_announceOption:function(_25){
if(!_25){
return;
}
var _26;
if(_25==this.dropDown.nextButton||_25==this.dropDown.previousButton){
_26=_25.innerHTML;
this.item=undefined;
this.value="";
}else{
_26=(this.store._oldAPI?this.store.getValue(_25.item,this.searchAttr):_25.item[this.searchAttr]).toString();
this.set("item",_25.item,false,_26);
}
this.focusNode.value=this.focusNode.value.substring(0,this._lastInput.length);
this.focusNode.setAttribute("aria-activedescendant",_5.get(_25,"id"));
this._autoCompleteText(_26);
},_selectOption:function(_27){
this.closeDropDown();
if(_27){
this._announceOption(_27);
}
this._setCaretPos(this.focusNode,this.focusNode.value.length);
this._handleOnChange(this.value,true);
},_startSearchAll:function(){
this._startSearch("");
},_startSearchFromInput:function(){
this._startSearch(this.focusNode.value.replace(/([\\\*\?])/g,"\\$1"));
},_getQueryString:function(_28){
return _c.substitute(this.queryExpr,[_28]);
},_startSearch:function(key){
if(!this.dropDown){
var _29=this.id+"_popup",_2a=_8.isString(this.dropDownClass)?_8.getObject(this.dropDownClass,false):this.dropDownClass;
this.dropDown=new _2a({onChange:_8.hitch(this,this._selectOption),id:_29,dir:this.dir,textDir:this.textDir});
this.focusNode.removeAttribute("aria-activedescendant");
this.textbox.setAttribute("aria-owns",_29);
}
this._lastInput=key;
var _2b=_8.clone(this.query);
var _2c={start:0,count:this.pageSize,queryOptions:{ignoreCase:this.ignoreCase,deep:true}};
_8.mixin(_2c,this.fetchProperties);
var qs=this._getQueryString(key),q;
if(this.store._oldAPI){
q=qs;
}else{
q=_2.patternToRegExp(qs,this.ignoreCase);
q.toString=function(){
return qs;
};
}
this._lastQuery=_2b[this.searchAttr]=q;
var _2d=this,_2e=function(){
var _2f=_2d._fetchHandle=_2d.store.query(_2b,_2c);
_4.when(_2f,function(res){
_2d._fetchHandle=null;
res.total=_2f.total;
_2d._openResultList(res,_2b,_2c);
},function(err){
_2d._fetchHandle=null;
if(!_2d._cancelingQuery){
console.error(_2d.declaredClass+" "+err.toString());
_2d.closeDropDown();
}
});
};
this.searchTimer=setTimeout(_8.hitch(this,function(_30,_31){
this.searchTimer=null;
_2e();
this._nextSearch=this.dropDown.onPage=function(_32){
_2c.start+=_2c.count*_32;
_2c.direction=_32;
_2e();
_31.focus();
};
},_2b,this),this.searchDelay);
},_getValueField:function(){
return this.searchAttr;
},constructor:function(){
this.query={};
this.fetchProperties={};
},postMixInProperties:function(){
if(!this.store){
var _33=this.srcNodeRef;
var _34=this.list;
if(_34){
this.store=_f.byId(_34);
}else{
this.store=new _e({},_33);
}
if(!("value" in this.params)){
var _35=(this.item=this.store.fetchSelectedItem());
if(_35){
var _36=this._getValueField();
this.value=this.store._oldAPI?this.store.getValue(_35,_36):_35[_36];
}
}
}
this.inherited(arguments);
},postCreate:function(){
var _37=_9("label[for=\""+this.id+"\"]");
if(_37.length){
_37[0].id=(this.id+"_label");
this.domNode.setAttribute("aria-labelledby",_37[0].id);
}
this.inherited(arguments);
},_getMenuLabelFromItem:function(_38){
var _39=this.labelFunc(_38,this.store),_3a=this.labelType;
if(this.highlightMatch!="none"&&this.labelType=="text"&&this._lastInput){
_39=this.doHighlight(_39,this._escapeHtml(this._lastInput));
_3a="html";
}
return {html:_3a=="html",label:_39};
},doHighlight:function(_3b,_3c){
var _3d=(this.ignoreCase?"i":"")+(this.highlightMatch=="all"?"g":""),i=this.queryExpr.indexOf("${0}");
_3c=_a.escapeString(_3c);
return this._escapeHtml(_3b).replace(new RegExp((i==0?"^":"")+"("+_3c+")"+(i==(this.queryExpr.length-4)?"$":""),_3d),"<span class=\"dijitComboBoxHighlightMatch\">$1</span>");
},_escapeHtml:function(str){
str=String(str).replace(/&/gm,"&amp;").replace(/</gm,"&lt;").replace(/>/gm,"&gt;").replace(/"/gm,"&quot;");
return str;
},reset:function(){
this.item=null;
this.inherited(arguments);
},labelFunc:function(_3e,_3f){
return (_3f._oldAPI?_3f.getValue(_3e,this.labelAttr||this.searchAttr):_3e[this.labelAttr||this.searchAttr]).toString();
},_setValueAttr:function(_40,_41,_42,_43){
this._set("item",_43||null);
if(!_40){
_40="";
}
this.inherited(arguments);
},_setTextDirAttr:function(_44){
this.inherited(arguments);
if(this.dropDown){
this.dropDown._set("textDir",_44);
}
}});
});
