//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/string,dijit/_WidgetBase"],function(_1,_2,_3){
_2.provide("dojox.mobile.app.List");
_2.experimental("dojox.mobile.app.List");
_2.require("dojo.string");
_2.require("dijit._WidgetBase");
(function(){
var _4={};
_2.declare("dojox.mobile.app.List",_1._WidgetBase,{items:null,itemTemplate:"",emptyTemplate:"",dividerTemplate:"",dividerFunction:null,labelDelete:"Delete",labelCancel:"Cancel",controller:null,autoDelete:true,enableDelete:true,enableHold:true,formatters:null,_templateLoadCount:0,_mouseDownPos:null,baseClass:"list",constructor:function(){
this._checkLoadComplete=_2.hitch(this,this._checkLoadComplete);
this._replaceToken=_2.hitch(this,this._replaceToken);
this._postDeleteAnim=_2.hitch(this,this._postDeleteAnim);
},postCreate:function(){
var _5=this;
if(this.emptyTemplate){
this._templateLoadCount++;
}
if(this.itemTemplate){
this._templateLoadCount++;
}
if(this.dividerTemplate){
this._templateLoadCount++;
}
this.connect(this.domNode,"onmousedown",function(_6){
var _7=_6;
if(_6.targetTouches&&_6.targetTouches.length>0){
_7=_6.targetTouches[0];
}
var _8=_5._getRowNode(_6.target);
if(_8){
_5._setDataInfo(_8,_6);
_5._selectRow(_8);
_5._mouseDownPos={x:_7.pageX,y:_7.pageY};
_5._dragThreshold=null;
}
});
this.connect(this.domNode,"onmouseup",function(_9){
if(_9.targetTouches&&_9.targetTouches.length>0){
_9=_9.targetTouches[0];
}
var _a=_5._getRowNode(_9.target);
if(_a){
_5._setDataInfo(_a,_9);
if(_5._selectedRow){
_5.onSelect(_a._data,_a._idx,_a);
}
this._deselectRow();
}
});
if(this.enableDelete){
this.connect(this.domNode,"mousemove",function(_b){
_2.stopEvent(_b);
if(!_5._selectedRow){
return;
}
var _c=_5._getRowNode(_b.target);
if(_5.enableDelete&&_c&&!_5._deleting){
_5.handleDrag(_b);
}
});
}
this.connect(this.domNode,"onclick",function(_d){
if(_d.touches&&_d.touches.length>0){
_d=_d.touches[0];
}
var _e=_5._getRowNode(_d.target,true);
if(_e){
_5._setDataInfo(_e,_d);
}
});
this.connect(this.domNode,"mouseout",function(_f){
if(_f.touches&&_f.touches.length>0){
_f=_f.touches[0];
}
if(_f.target==_5._selectedRow){
_5._deselectRow();
}
});
if(!this.itemTemplate){
throw Error("An item template must be provided to "+this.declaredClass);
}
this._loadTemplate(this.itemTemplate,"itemTemplate",this._checkLoadComplete);
if(this.emptyTemplate){
this._loadTemplate(this.emptyTemplate,"emptyTemplate",this._checkLoadComplete);
}
if(this.dividerTemplate){
this._loadTemplate(this.dividerTemplate,"dividerTemplate",this._checkLoadComplete);
}
},handleDrag:function(_10){
var _11=_10;
if(_10.targetTouches&&_10.targetTouches.length>0){
_11=_10.targetTouches[0];
}
var _12=_11.pageX-this._mouseDownPos.x;
var _13=Math.abs(_12);
if(_13>10&&!this._dragThreshold){
this._dragThreshold=_2.marginBox(this._selectedRow).w*0.6;
if(!this.autoDelete){
this.createDeleteButtons(this._selectedRow);
}
}
this._selectedRow.style.left=(_13>10?_12:0)+"px";
if(this._dragThreshold&&this._dragThreshold<_13){
this.preDelete(_12);
}
},handleDragCancel:function(){
if(this._deleting){
return;
}
_2.removeClass(this._selectedRow,"hold");
this._selectedRow.style.left=0;
this._mouseDownPos=null;
this._dragThreshold=null;
this._deleteBtns&&_2.style(this._deleteBtns,"display","none");
},preDelete:function(_14){
var _15=this;
this._deleting=true;
_2.animateProperty({node:this._selectedRow,duration:400,properties:{left:{end:_14+((_14>0?1:-1)*this._dragThreshold*0.8)}},onEnd:_2.hitch(this,function(){
if(this.autoDelete){
this.deleteRow(this._selectedRow);
}
})}).play();
},deleteRow:function(row){
_2.style(row,{visibility:"hidden",minHeight:"0px"});
_2.removeClass(row,"hold");
this._deleteAnimConn=this.connect(row,"webkitAnimationEnd",this._postDeleteAnim);
_2.addClass(row,"collapsed");
},_postDeleteAnim:function(_16){
if(this._deleteAnimConn){
this.disconnect(this._deleteAnimConn);
this._deleteAnimConn=null;
}
var row=this._selectedRow;
var _17=row.nextSibling;
var _18=row.previousSibling;
if(_18&&_18._isDivider){
if(!_17||_17._isDivider){
_18.parentNode.removeChild(_18);
}
}
row.parentNode.removeChild(row);
this.onDelete(row._data,row._idx,this.items);
while(_17){
if(_17._idx){
_17._idx--;
}
_17=_17.nextSibling;
}
_2.destroy(row);
_2.query("> *:not(.buttons)",this.domNode).forEach(this.applyClass);
this._deleting=false;
this._deselectRow();
},createDeleteButtons:function(_19){
var mb=_2.marginBox(_19);
var pos=_2._abs(_19,true);
if(!this._deleteBtns){
this._deleteBtns=_2.create("div",{"class":"buttons"},this.domNode);
this.buttons=[];
this.buttons.push(new _3.mobile.Button({btnClass:"mblRedButton",label:this.labelDelete}));
this.buttons.push(new _3.mobile.Button({btnClass:"mblBlueButton",label:this.labelCancel}));
_2.place(this.buttons[0].domNode,this._deleteBtns);
_2.place(this.buttons[1].domNode,this._deleteBtns);
_2.addClass(this.buttons[0].domNode,"deleteBtn");
_2.addClass(this.buttons[1].domNode,"cancelBtn");
this._handleButtonClick=_2.hitch(this._handleButtonClick);
this.connect(this._deleteBtns,"onclick",this._handleButtonClick);
}
_2.removeClass(this._deleteBtns,"fade out fast");
_2.style(this._deleteBtns,{display:"",width:mb.w+"px",height:mb.h+"px",top:(_19.offsetTop)+"px",left:"0px"});
},onDelete:function(_1a,_1b,_1c){
_1c.splice(_1b,1);
if(_1c.length<1){
this.render();
}
},cancelDelete:function(){
this._deleting=false;
this.handleDragCancel();
},_handleButtonClick:function(_1d){
if(_1d.touches&&_1d.touches.length>0){
_1d=_1d.touches[0];
}
var _1e=_1d.target;
if(_2.hasClass(_1e,"deleteBtn")){
this.deleteRow(this._selectedRow);
}else{
if(_2.hasClass(_1e,"cancelBtn")){
this.cancelDelete();
}else{
return;
}
}
_2.addClass(this._deleteBtns,"fade out");
},applyClass:function(_1f,idx,_20){
_2.removeClass(_1f,"first last");
if(idx==0){
_2.addClass(_1f,"first");
}
if(idx==_20.length-1){
_2.addClass(_1f,"last");
}
},_setDataInfo:function(_21,_22){
_22.item=_21._data;
_22.index=_21._idx;
},onSelect:function(_23,_24,_25){
},_selectRow:function(row){
if(this._deleting&&this._selectedRow&&row!=this._selectedRow){
this.cancelDelete();
}
if(!_2.hasClass(row,"row")){
return;
}
if(this.enableHold||this.enableDelete){
_2.addClass(row,"hold");
}
this._selectedRow=row;
},_deselectRow:function(){
if(!this._selectedRow||this._deleting){
return;
}
this.handleDragCancel();
_2.removeClass(this._selectedRow,"hold");
this._selectedRow=null;
},_getRowNode:function(_26,_27){
while(_26&&!_26._data&&_26!=this.domNode){
if(!_27&&_2.hasClass(_26,"noclick")){
return null;
}
_26=_26.parentNode;
}
return _26==this.domNode?null:_26;
},applyTemplate:function(_28,_29){
return _2._toDom(_2.string.substitute(_28,_29,this._replaceToken,this.formatters||this));
},render:function(){
_2.query("> *:not(.buttons)",this.domNode).forEach(_2.destroy);
if(this.items.length<1&&this.emptyTemplate){
_2.place(_2._toDom(this.emptyTemplate),this.domNode,"first");
}else{
this.domNode.appendChild(this._renderRange(0,this.items.length));
}
if(_2.hasClass(this.domNode.parentNode,"mblRoundRect")){
_2.addClass(this.domNode.parentNode,"mblRoundRectList");
}
var _2a=_2.query("> .row",this.domNode);
if(_2a.length>0){
_2.addClass(_2a[0],"first");
_2.addClass(_2a[_2a.length-1],"last");
}
},_renderRange:function(_2b,_2c){
var _2d=[];
var row,i;
var _2e=document.createDocumentFragment();
_2b=Math.max(0,_2b);
_2c=Math.min(_2c,this.items.length);
for(i=_2b;i<_2c;i++){
row=this.applyTemplate(this.itemTemplate,this.items[i]);
_2.addClass(row,"row");
row._data=this.items[i];
row._idx=i;
_2d.push(row);
}
if(!this.dividerFunction||!this.dividerTemplate){
for(i=_2b;i<_2c;i++){
_2d[i]._data=this.items[i];
_2d[i]._idx=i;
_2e.appendChild(_2d[i]);
}
}else{
var _2f=null;
var _30;
var _31;
for(i=_2b;i<_2c;i++){
_2d[i]._data=this.items[i];
_2d[i]._idx=i;
_30=this.dividerFunction(this.items[i]);
if(_30&&_30!=_2f){
_31=this.applyTemplate(this.dividerTemplate,{label:_30,item:this.items[i]});
_31._isDivider=true;
_2e.appendChild(_31);
_2f=_30;
}
_2e.appendChild(_2d[i]);
}
}
return _2e;
},_replaceToken:function(_32,key){
if(key.charAt(0)=="!"){
_32=_2.getObject(key.substr(1),false,_this);
}
if(typeof _32=="undefined"){
return "";
}
if(_32==null){
return "";
}
return key.charAt(0)=="!"?_32:_32.toString().replace(/"/g,"&quot;");
},_checkLoadComplete:function(){
this._templateLoadCount--;
if(this._templateLoadCount<1&&this.get("items")){
this.render();
}
},_loadTemplate:function(url,_33,_34){
if(!url){
_34();
return;
}
if(_4[url]){
this.set(_33,_4[url]);
_34();
}else{
var _35=this;
_2.xhrGet({url:url,sync:false,handleAs:"text",load:function(_36){
_4[url]=_2.trim(_36);
_35.set(_33,_4[url]);
_34();
}});
}
},_setFormattersAttr:function(_37){
this.formatters=_37;
},_setItemsAttr:function(_38){
this.items=_38||[];
if(this._templateLoadCount<1&&_38){
this.render();
}
},destroy:function(){
if(this.buttons){
_2.forEach(this.buttons,function(_39){
_39.destroy();
});
this.buttons=null;
}
this.inherited(arguments);
}});
})();
});
