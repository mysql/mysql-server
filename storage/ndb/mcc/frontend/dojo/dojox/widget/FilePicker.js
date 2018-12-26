//>>built
define(["dijit","dojo","dojox","dojo/i18n!dojox/widget/nls/FilePicker","dojo/require!dojox/widget/RollingList,dojo/i18n"],function(_1,_2,_3){
_2.provide("dojox.widget.FilePicker");
_2.require("dojox.widget.RollingList");
_2.require("dojo.i18n");
_2.requireLocalization("dojox.widget","FilePicker");
_2.declare("dojox.widget._FileInfoPane",[_3.widget._RollingListPane],{templateString:"",templateString:_2.cache("dojox.widget","FilePicker/_FileInfoPane.html","<div class=\"dojoxFileInfoPane\">\n\t<table>\n\t\t<tbody>\n\t\t\t<tr>\n\t\t\t\t<td class=\"dojoxFileInfoLabel dojoxFileInfoNameLabel\">${_messages.name}</td>\n\t\t\t\t<td class=\"dojoxFileInfoName\" dojoAttachPoint=\"nameNode\"></td>\n\t\t\t</tr>\n\t\t\t<tr>\n\t\t\t\t<td class=\"dojoxFileInfoLabel dojoxFileInfoPathLabel\">${_messages.path}</td>\n\t\t\t\t<td class=\"dojoxFileInfoPath\" dojoAttachPoint=\"pathNode\"></td>\n\t\t\t</tr>\n\t\t\t<tr>\n\t\t\t\t<td class=\"dojoxFileInfoLabel dojoxFileInfoSizeLabel\">${_messages.size}</td>\n\t\t\t\t<td class=\"dojoxFileInfoSize\" dojoAttachPoint=\"sizeNode\"></td>\n\t\t\t</tr>\n\t\t</tbody>\n\t</table>\n\t<div dojoAttachPoint=\"containerNode\" style=\"display:none;\"></div>\n</div>"),postMixInProperties:function(){
this._messages=_2.i18n.getLocalization("dojox.widget","FilePicker",this.lang);
this.inherited(arguments);
},onItems:function(){
var _4=this.store,_5=this.items[0];
if(!_5){
this._onError("Load",new Error("No item defined"));
}else{
this.nameNode.innerHTML=_4.getLabel(_5);
this.pathNode.innerHTML=_4.getIdentity(_5);
this.sizeNode.innerHTML=_4.getValue(_5,"size");
this.parentWidget.scrollIntoView(this);
this.inherited(arguments);
}
}});
_2.declare("dojox.widget.FilePicker",_3.widget.RollingList,{className:"dojoxFilePicker",pathSeparator:"",topDir:"",parentAttr:"parentDir",pathAttr:"path",preloadItems:50,selectDirectories:true,selectFiles:true,_itemsMatch:function(_6,_7){
if(!_6&&!_7){
return true;
}else{
if(!_6||!_7){
return false;
}else{
if(_6==_7){
return true;
}else{
if(this._isIdentity){
var _8=[this.store.getIdentity(_6),this.store.getIdentity(_7)];
_2.forEach(_8,function(i,_9){
if(i.lastIndexOf(this.pathSeparator)==(i.length-1)){
_8[_9]=i.substring(0,i.length-1);
}else{
}
},this);
return (_8[0]==_8[1]);
}
}
}
}
return false;
},startup:function(){
if(this._started){
return;
}
this.inherited(arguments);
var _a,_b=this.getChildren()[0];
var _c=_2.hitch(this,function(){
if(_a){
this.disconnect(_a);
}
delete _a;
var _d=_b.items[0];
if(_d){
var _e=this.store;
var _f=_e.getValue(_d,this.parentAttr);
var _10=_e.getValue(_d,this.pathAttr);
this.pathSeparator=this.pathSeparator||_e.pathSeparator;
if(!this.pathSeparator){
this.pathSeparator=_10.substring(_f.length,_f.length+1);
}
if(!this.topDir){
this.topDir=_f;
if(this.topDir.lastIndexOf(this.pathSeparator)!=(this.topDir.length-1)){
this.topDir+=this.pathSeparator;
}
}
}
});
if(!this.pathSeparator||!this.topDir){
if(!_b.items){
_a=this.connect(_b,"onItems",_c);
}else{
_c();
}
}
},getChildItems:function(_11){
var ret=this.inherited(arguments);
if(!ret&&this.store.getValue(_11,"directory")){
ret=[];
}
return ret;
},getMenuItemForItem:function(_12,_13,_14){
var _15={iconClass:"dojoxDirectoryItemIcon"};
if(!this.store.getValue(_12,"directory")){
_15.iconClass="dojoxFileItemIcon";
var l=this.store.getLabel(_12),idx=l.lastIndexOf(".");
if(idx>=0){
_15.iconClass+=" dojoxFileItemIcon_"+l.substring(idx+1);
}
if(!this.selectFiles){
_15.disabled=true;
}
}
var ret=new _1.MenuItem(_15);
return ret;
},getPaneForItem:function(_16,_17,_18){
var ret=null;
if(!_16||(this.store.isItem(_16)&&this.store.getValue(_16,"directory"))){
ret=new _3.widget._RollingListGroupPane({});
}else{
if(this.store.isItem(_16)&&!this.store.getValue(_16,"directory")){
ret=new _3.widget._FileInfoPane({});
}
}
return ret;
},_setPathValueAttr:function(_19,_1a,_1b){
if(!_19){
this.set("value",null);
return;
}
if(_19.lastIndexOf(this.pathSeparator)==(_19.length-1)){
_19=_19.substring(0,_19.length-1);
}
this.store.fetchItemByIdentity({identity:_19,onItem:function(v){
if(_1a){
this._lastExecutedValue=v;
}
this.set("value",v);
if(_1b){
_1b();
}
},scope:this});
},_getPathValueAttr:function(val){
if(!val){
val=this.value;
}
if(val&&this.store.isItem(val)){
return this.store.getValue(val,this.pathAttr);
}else{
return "";
}
},_setValue:function(_1c){
delete this._setInProgress;
var _1d=this.store;
if(_1c&&_1d.isItem(_1c)){
var _1e=this.store.getValue(_1c,"directory");
if((_1e&&!this.selectDirectories)||(!_1e&&!this.selectFiles)){
return;
}
}else{
_1c=null;
}
if(!this._itemsMatch(this.value,_1c)){
this.value=_1c;
this._onChange(_1c);
}
}});
});
