//>>built
define("dijit/form/_ComboBoxMenuMixin",["dojo/_base/array","dojo/_base/declare","dojo/dom-attr","dojo/i18n","dojo/_base/window","dojo/i18n!./nls/ComboBox"],function(_1,_2,_3,_4,_5){
return _2("dijit.form._ComboBoxMenuMixin",null,{_messages:null,postMixInProperties:function(){
this.inherited(arguments);
this._messages=_4.getLocalization("dijit.form","ComboBox",this.lang);
},buildRendering:function(){
this.inherited(arguments);
this.previousButton.innerHTML=this._messages["previousMessage"];
this.nextButton.innerHTML=this._messages["nextMessage"];
},_setValueAttr:function(_6){
this.value=_6;
this.onChange(_6);
},onClick:function(_7){
if(_7==this.previousButton){
this._setSelectedAttr(null);
this.onPage(-1);
}else{
if(_7==this.nextButton){
this._setSelectedAttr(null);
this.onPage(1);
}else{
this.onChange(_7);
}
}
},onChange:function(){
},onPage:function(){
},onClose:function(){
this._setSelectedAttr(null);
},_createOption:function(_8,_9){
var _a=this._createMenuItem();
var _b=_9(_8);
if(_b.html){
_a.innerHTML=_b.label;
}else{
_a.appendChild(_5.doc.createTextNode(_b.label));
}
if(_a.innerHTML==""){
_a.innerHTML="&#160;";
}
this.applyTextDir(_a,(_a.innerText||_a.textContent||""));
_a.item=_8;
return _a;
},createOptions:function(_c,_d,_e){
this.previousButton.style.display=(_d.start==0)?"none":"";
_3.set(this.previousButton,"id",this.id+"_prev");
_1.forEach(_c,function(_f,i){
var _10=this._createOption(_f,_e);
_3.set(_10,"id",this.id+i);
this.nextButton.parentNode.insertBefore(_10,this.nextButton);
},this);
var _11=false;
if(_c.total&&!_c.total.then&&_c.total!=-1){
if((_d.start+_d.count)<_c.total){
_11=true;
}else{
if((_d.start+_d.count)>_c.total&&_d.count==_c.length){
_11=true;
}
}
}else{
if(_d.count==_c.length){
_11=true;
}
}
this.nextButton.style.display=_11?"":"none";
_3.set(this.nextButton,"id",this.id+"_next");
return this.containerNode.childNodes;
},clearResultList:function(){
var _12=this.containerNode;
while(_12.childNodes.length>2){
_12.removeChild(_12.childNodes[_12.childNodes.length-2]);
}
this._setSelectedAttr(null);
},highlightFirstOption:function(){
this.selectFirstNode();
},highlightLastOption:function(){
this.selectLastNode();
},selectFirstNode:function(){
this.inherited(arguments);
if(this.getHighlightedOption()==this.previousButton){
this.selectNextNode();
}
},selectLastNode:function(){
this.inherited(arguments);
if(this.getHighlightedOption()==this.nextButton){
this.selectPreviousNode();
}
},getHighlightedOption:function(){
return this._getSelectedAttr();
}});
});
