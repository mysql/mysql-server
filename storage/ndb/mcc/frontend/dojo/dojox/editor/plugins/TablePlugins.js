//>>built
require({cache:{"url:dojox/editor/plugins/resources/insertTable.html":"<div class=\"dijitDialog\" tabindex=\"-1\" role=\"dialog\" aria-labelledby=\"${id}_title\">\n\t<div dojoAttachPoint=\"titleBar\" class=\"dijitDialogTitleBar\">\n\t<span dojoAttachPoint=\"titleNode\" class=\"dijitDialogTitle\" id=\"${id}_title\">${insertTableTitle}</span>\n\t<span dojoAttachPoint=\"closeButtonNode\" class=\"dijitDialogCloseIcon\" dojoAttachEvent=\"onclick: onCancel\" title=\"${buttonCancel}\">\n\t\t<span dojoAttachPoint=\"closeText\" class=\"closeText\" title=\"${buttonCancel}\">x</span>\n\t</span>\n\t</div>\n    <div dojoAttachPoint=\"containerNode\" class=\"dijitDialogPaneContent\">\n        <table class=\"etdTable\"><tr>\n            <td>\n                <label>${rows}</label>\n\t\t\t</td><td>\n                <span dojoAttachPoint=\"selectRow\" dojoType=\"dijit.form.TextBox\" value=\"2\"></span>\n            </td><td><table><tr><td class=\"inner\">\n                <label>${columns}</label>\n\t\t\t</td><td class=\"inner\">\n                <span dojoAttachPoint=\"selectCol\" dojoType=\"dijit.form.TextBox\" value=\"2\"></span>\n            </td></tr></table></td></tr>\t\t\n\t\t\t<tr><td>\n                <label>${tableWidth}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectWidth\" dojoType=\"dijit.form.TextBox\" value=\"100\"></span>\n\t\t\t</td><td>\n                <select dojoAttachPoint=\"selectWidthType\" hasDownArrow=\"true\" dojoType=\"dijit.form.FilteringSelect\">\n                  <option value=\"percent\">${percent}</option>\n                  <option value=\"pixels\">${pixels}</option>\n                </select></td></tr>\t\n            <tr><td>\n                <label>${borderThickness}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectBorder\" dojoType=\"dijit.form.TextBox\" value=\"1\"></span>\n            </td><td>\n                ${pixels}\n            </td></tr><tr><td>\n                <label>${cellPadding}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectPad\" dojoType=\"dijit.form.TextBox\" value=\"0\"></span>\n            </td><td class=\"cellpad\"></td></tr><tr><td>\n                <label>${cellSpacing}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectSpace\" dojoType=\"dijit.form.TextBox\" value=\"0\"></span>\n            </td><td class=\"cellspace\"></td></tr></table>\n        <div class=\"dialogButtonContainer\">\n            <div dojoType=\"dijit.form.Button\" dojoAttachEvent=\"onClick: onInsert\">${buttonInsert}</div>\n            <div dojoType=\"dijit.form.Button\" dojoAttachEvent=\"onClick: onCancel\">${buttonCancel}</div>\n        </div>\n\t</div>\n</div>\n","url:dojox/editor/plugins/resources/modifyTable.html":"<div class=\"dijitDialog\" tabindex=\"-1\" role=\"dialog\" aria-labelledby=\"${id}_title\">\n\t<div dojoAttachPoint=\"titleBar\" class=\"dijitDialogTitleBar\">\n\t<span dojoAttachPoint=\"titleNode\" class=\"dijitDialogTitle\" id=\"${id}_title\">${modifyTableTitle}</span>\n\t<span dojoAttachPoint=\"closeButtonNode\" class=\"dijitDialogCloseIcon\" dojoAttachEvent=\"onclick: onCancel\" title=\"${buttonCancel}\">\n\t\t<span dojoAttachPoint=\"closeText\" class=\"closeText\" title=\"${buttonCancel}\">x</span>\n\t</span>\n\t</div>\n    <div dojoAttachPoint=\"containerNode\" class=\"dijitDialogPaneContent\">\n        <table class=\"etdTable\">\n          <tr><td>\n                <label>${backgroundColor}</label>\n            </td><td colspan=\"2\">\n                <span class=\"colorSwatchBtn\" dojoAttachPoint=\"backgroundCol\"></span>\n            </td></tr><tr><td>\n                <label>${borderColor}</label>\n            </td><td colspan=\"2\">\n                <span class=\"colorSwatchBtn\" dojoAttachPoint=\"borderCol\"></span>\n            </td></tr><tr><td>\n                <label>${align}</label>\n            </td><td colspan=\"2\">\t\n                <select dojoAttachPoint=\"selectAlign\" dojoType=\"dijit.form.FilteringSelect\">\n                  <option value=\"default\">${default}</option>\n                  <option value=\"left\">${left}</option>\n                  <option value=\"center\">${center}</option>\n                  <option value=\"right\">${right}</option>\n                </select>\n            </td></tr>\n            <tr><td>\n                <label>${tableWidth}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectWidth\" dojoType=\"dijit.form.TextBox\" value=\"100\"></span>\n            </td><td>\n                <select dojoAttachPoint=\"selectWidthType\" hasDownArrow=\"true\" dojoType=\"dijit.form.FilteringSelect\">\n                  <option value=\"percent\">${percent}</option>\n                  <option value=\"pixels\">${pixels}</option>\n                </select></td></tr>\t\n            <tr><td>\n                <label>${borderThickness}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectBorder\" dojoType=\"dijit.form.TextBox\" value=\"1\"></span>\n            </td><td>\n                ${pixels}\n            </td></tr><tr><td>\n                <label>${cellPadding}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectPad\" dojoType=\"dijit.form.TextBox\" value=\"0\"></span>\n            </td><td class=\"cellpad\"></td></tr><tr><td>\n                <label>${cellSpacing}</label>\n            </td><td>\n                <span dojoAttachPoint=\"selectSpace\" dojoType=\"dijit.form.TextBox\" value=\"0\"></span>\n            </td><td class=\"cellspace\"></td></tr>\n        </table>\n        <div class=\"dialogButtonContainer\">\n            <div dojoType=\"dijit.form.Button\" dojoAttachEvent=\"onClick: onSet\">${buttonSet}</div>\n            <div dojoType=\"dijit.form.Button\" dojoAttachEvent=\"onClick: onCancel\">${buttonCancel}</div>\n        </div>\n\t</div>\n</div>\n"}});
define("dojox/editor/plugins/TablePlugins",["dojo/_base/declare","dojo/_base/array","dojo/_base/Color","dojo/aspect","dojo/dom-attr","dojo/dom-style","dijit/_editor/_Plugin","dijit/_WidgetBase","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/Dialog","dijit/Menu","dijit/MenuItem","dijit/MenuSeparator","dijit/ColorPalette","dojox/widget/ColorPicker","dojo/text!./resources/insertTable.html","dojo/text!./resources/modifyTable.html","dojo/i18n!./nls/TableDialog","dijit/_base/popup","dijit/popup","dojo/_base/connect","dijit/TooltipDialog","dijit/form/Button","dijit/form/DropDownButton","dijit/form/TextBox","dijit/form/FilteringSelect"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,_12,_13){
dojo.experimental("dojox.editor.plugins.TablePlugins");
var _14=_1(_7,{tablesConnected:false,currentlyAvailable:false,alwaysAvailable:false,availableCurrentlySet:false,initialized:false,tableData:null,shiftKeyDown:false,editorDomNode:null,undoEnabled:true,refCount:0,doMixins:function(){
dojo.mixin(this.editor,{getAncestorElement:function(_15){
return this._sCall("getAncestorElement",[_15]);
},hasAncestorElement:function(_16){
return this._sCall("hasAncestorElement",[_16]);
},selectElement:function(_17){
this._sCall("selectElement",[_17]);
},byId:function(id){
return dojo.byId(id,this.document);
},query:function(arg,_18,_19){
var ar=dojo.query(arg,_18||this.document);
return (_19)?ar[0]:ar;
}});
},initialize:function(_1a){
this.refCount++;
_1a.customUndo=true;
if(this.initialized){
return;
}
this.initialized=true;
this.editor=_1a;
this.editor._tablePluginHandler=this;
_1a.onLoadDeferred.addCallback(dojo.hitch(this,function(){
this.editorDomNode=this.editor.editNode||this.editor.iframe.document.body.firstChild;
this._myListeners=[dojo.connect(this.editorDomNode,"mouseup",this.editor,"onClick"),dojo.connect(this.editor,"onDisplayChanged",this,"checkAvailable"),dojo.connect(this.editor,"onBlur",this,"checkAvailable"),dojo.connect(this.editor,"_saveSelection",this,function(){
this._savedTableInfo=this.getTableInfo();
}),dojo.connect(this.editor,"_restoreSelection",this,function(){
delete this._savedTableInfo;
})];
this.doMixins();
this.connectDraggable();
}));
},getTableInfo:function(_1b){
if(this._savedTableInfo){
return this._savedTableInfo;
}
if(_1b){
this._tempStoreTableData(false);
}
if(this.tableData){
return this.tableData;
}
var tr,trs,td,tds,tbl,_1c,_1d,_1e,o;
td=this.editor.getAncestorElement("td");
if(td){
tr=td.parentNode;
}
tbl=this.editor.getAncestorElement("table");
if(tbl){
tds=dojo.query("td",tbl);
tds.forEach(function(d,i){
if(td==d){
_1d=i;
}
});
trs=dojo.query("tr",tbl);
trs.forEach(function(r,i){
if(tr==r){
_1e=i;
}
});
_1c=tds.length/trs.length;
o={tbl:tbl,td:td,tr:tr,trs:trs,tds:tds,rows:trs.length,cols:_1c,tdIndex:_1d,trIndex:_1e,colIndex:_1d%_1c};
}else{
o={};
}
this.tableData=o;
this._tempStoreTableData(500);
return this.tableData;
},connectDraggable:function(){
if(!dojo.isIE){
return;
}
this.editorDomNode.ondragstart=dojo.hitch(this,"onDragStart");
this.editorDomNode.ondragend=dojo.hitch(this,"onDragEnd");
},onDragStart:function(){
var e=window.event;
if(!e.srcElement.id){
e.srcElement.id="tbl_"+(new Date().getTime());
}
},onDragEnd:function(){
var e=window.event;
var _1f=e.srcElement;
var id=_1f.id;
var doc=this.editor.document;
if(_1f.tagName.toLowerCase()=="table"){
setTimeout(function(){
var _20=dojo.byId(id,doc);
dojo.removeAttr(_20,"align");
},100);
}
},checkAvailable:function(){
if(this.availableCurrentlySet){
return this.currentlyAvailable;
}
if(!this.editor){
return false;
}
if(this.alwaysAvailable){
return true;
}
this.currentlyAvailable=this.editor.focused&&(this._savedTableInfo?this._savedTableInfo.tbl:this.editor.hasAncestorElement("table"));
if(this.currentlyAvailable){
this.connectTableKeys();
}else{
this.disconnectTableKeys();
}
this._tempAvailability(500);
dojo.publish(this.editor.id+"_tablePlugins",[this.currentlyAvailable]);
return this.currentlyAvailable;
},_prepareTable:function(tbl){
var tds=this.editor.query("td",tbl);
if(!tds[0].id){
tds.forEach(function(td,i){
if(!td.id){
td.id="tdid"+i+this.getTimeStamp();
}
},this);
}
return tds;
},getTimeStamp:function(){
return new Date().getTime();
},_tempStoreTableData:function(_21){
if(_21===true){
}else{
if(_21===false){
this.tableData=null;
}else{
if(_21===undefined){
console.warn("_tempStoreTableData must be passed an argument");
}else{
setTimeout(dojo.hitch(this,function(){
this.tableData=null;
}),_21);
}
}
}
},_tempAvailability:function(_22){
if(_22===true){
this.availableCurrentlySet=true;
}else{
if(_22===false){
this.availableCurrentlySet=false;
}else{
if(_22===undefined){
console.warn("_tempAvailability must be passed an argument");
}else{
this.availableCurrentlySet=true;
setTimeout(dojo.hitch(this,function(){
this.availableCurrentlySet=false;
}),_22);
}
}
}
},connectTableKeys:function(){
if(this.tablesConnected){
return;
}
this.tablesConnected=true;
var _23=(this.editor.iframe)?this.editor.document:this.editor.editNode;
this.cnKeyDn=dojo.connect(_23,"onkeydown",this,"onKeyDown");
this.cnKeyUp=dojo.connect(_23,"onkeyup",this,"onKeyUp");
this._myListeners.push(dojo.connect(_23,"onkeypress",this,"onKeyUp"));
},disconnectTableKeys:function(){
dojo.disconnect(this.cnKeyDn);
dojo.disconnect(this.cnKeyUp);
this.tablesConnected=false;
},onKeyDown:function(evt){
var key=evt.keyCode;
if(key==16){
this.shiftKeyDown=true;
}
if(key==9){
var o=this.getTableInfo();
o.tdIndex=(this.shiftKeyDown)?o.tdIndex-1:tabTo=o.tdIndex+1;
if(o.tdIndex>=0&&o.tdIndex<o.tds.length){
this.editor.selectElement(o.tds[o.tdIndex]);
this.currentlyAvailable=true;
this._tempAvailability(true);
this._tempStoreTableData(true);
this.stopEvent=true;
}else{
this.stopEvent=false;
this.onDisplayChanged();
}
if(this.stopEvent){
dojo.stopEvent(evt);
}
}
},onKeyUp:function(evt){
var key=evt.keyCode;
if(key==16){
this.shiftKeyDown=false;
}
if(key==37||key==38||key==39||key==40){
this.onDisplayChanged();
}
if(key==9&&this.stopEvent){
dojo.stopEvent(evt);
}
},onDisplayChanged:function(){
this.currentlyAvailable=false;
this._tempStoreTableData(false);
this._tempAvailability(false);
this.checkAvailable();
},uninitialize:function(_24){
if(this.editor==_24){
this.refCount--;
if(!this.refCount&&this.initialized){
if(this.tablesConnected){
this.disconnectTableKeys();
}
this.initialized=false;
dojo.forEach(this._myListeners,function(l){
dojo.disconnect(l);
});
delete this._myListeners;
delete this.editor._tablePluginHandler;
delete this.editor;
}
this.inherited(arguments);
}
}});
var _25=_1("dojox.editor.plugins.TablePlugins",_7,{iconClassPrefix:"editorIcon",useDefaultCommand:false,buttonClass:dijit.form.Button,commandName:"",label:"",alwaysAvailable:false,undoEnabled:true,onDisplayChanged:function(_26){
if(!this.alwaysAvailable){
this.available=_26;
this.button.set("disabled",!this.available);
}
},setEditor:function(_27){
this.editor=_27;
this.editor.customUndo=true;
this.inherited(arguments);
this._availableTopic=dojo.subscribe(this.editor.id+"_tablePlugins",this,"onDisplayChanged");
this.onEditorLoaded();
},onEditorLoaded:function(){
if(!this.editor._tablePluginHandler){
var _28=new _14();
_28.initialize(this.editor);
}else{
this.editor._tablePluginHandler.initialize(this.editor);
}
},selectTable:function(){
var o=this.getTableInfo();
if(o&&o.tbl){
this.editor._sCall("selectElement",[o.tbl]);
}
},_initButton:function(){
this.command=this.name;
this.label=this.editor.commands[this.command]=this._makeTitle(this.command);
this.inherited(arguments);
delete this.command;
this.connect(this.button,"onClick","modTable");
this.onDisplayChanged(false);
},modTable:function(cmd,_29){
if(dojo.isIE){
this.editor.focus();
}
this.begEdit();
var o=this.getTableInfo();
var sw=(dojo.isString(cmd))?cmd:this.name;
var r,c,i;
var _2a=false;
switch(sw){
case "insertTableRowBefore":
r=o.tbl.insertRow(o.trIndex);
for(i=0;i<o.cols;i++){
c=r.insertCell(-1);
c.innerHTML="&nbsp;";
}
break;
case "insertTableRowAfter":
r=o.tbl.insertRow(o.trIndex+1);
for(i=0;i<o.cols;i++){
c=r.insertCell(-1);
c.innerHTML="&nbsp;";
}
break;
case "insertTableColumnBefore":
o.trs.forEach(function(r){
c=r.insertCell(o.colIndex);
c.innerHTML="&nbsp;";
});
_2a=true;
break;
case "insertTableColumnAfter":
o.trs.forEach(function(r){
c=r.insertCell(o.colIndex+1);
c.innerHTML="&nbsp;";
});
_2a=true;
break;
case "deleteTableRow":
o.tbl.deleteRow(o.trIndex);
break;
case "deleteTableColumn":
o.trs.forEach(function(tr){
tr.deleteCell(o.colIndex);
});
_2a=true;
break;
case "modifyTable":
break;
case "insertTable":
break;
}
if(_2a){
this.makeColumnsEven();
}
this.endEdit();
},begEdit:function(){
if(this.editor._tablePluginHandler.undoEnabled){
if(this.editor.customUndo){
this.editor.beginEditing();
}else{
this.valBeforeUndo=this.editor.getValue();
}
}
},endEdit:function(){
if(this.editor._tablePluginHandler.undoEnabled){
if(this.editor.customUndo){
this.editor.endEditing();
}else{
var _2b=this.editor.getValue();
this.editor.setValue(this.valBeforeUndo);
this.editor.replaceValue(_2b);
}
this.editor.onDisplayChanged();
}
},makeColumnsEven:function(){
setTimeout(dojo.hitch(this,function(){
var o=this.getTableInfo(true);
var w=Math.floor(100/o.cols);
o.tds.forEach(function(d){
dojo.attr(d,"width",w+"%");
});
}),10);
},getTableInfo:function(_2c){
return this.editor._tablePluginHandler.getTableInfo(_2c);
},_makeTitle:function(str){
this._strings=dojo.i18n.getLocalization("dojox.editor.plugins","TableDialog");
var _2d=this._strings[str+"Title"]||this._strings[str+"Label"]||str;
return _2d;
},getSelectedCells:function(){
var _2e=[];
var tbl=this.getTableInfo().tbl;
this.editor._tablePluginHandler._prepareTable(tbl);
var e=this.editor;
var _2f=e._sCall("getSelectedHtml",[null]);
var str=_2f.match(/id="*\w*"*/g);
dojo.forEach(str,function(a){
var id=a.substring(3,a.length);
if(id.charAt(0)=="\""&&id.charAt(id.length-1)=="\""){
id=id.substring(1,id.length-1);
}
var _30=e.byId(id);
if(_30&&_30.tagName.toLowerCase()=="td"){
_2e.push(_30);
}
},this);
if(!_2e.length){
var sel=dijit.range.getSelection(e.window);
if(sel.rangeCount){
var r=sel.getRangeAt(0);
var _31=r.startContainer;
while(_31&&_31!=e.editNode&&_31!=e.document){
if(_31.nodeType===1){
var tg=_31.tagName?_31.tagName.toLowerCase():"";
if(tg==="td"){
return [_31];
}
}
_31=_31.parentNode;
}
}
}
return _2e;
},updateState:function(){
if(this.button){
if((this.available||this.alwaysAvailable)&&!this.get("disabled")){
this.button.set("disabled",false);
}else{
this.button.set("disabled",true);
}
}
},destroy:function(){
this.inherited(arguments);
dojo.unsubscribe(this._availableTopic);
this.editor._tablePluginHandler.uninitialize(this.editor);
}});
var _32=_1(_25,{constructor:function(){
this.connect(this,"setEditor",function(_33){
_33.onLoadDeferred.addCallback(dojo.hitch(this,function(){
this._createContextMenu();
}));
this.button.domNode.style.display="none";
});
},destroy:function(){
if(this.menu){
this.menu.destroyRecursive();
delete this.menu;
}
this.inherited(arguments);
},_initButton:function(){
this.inherited(arguments);
if(this.name==="tableContextMenu"){
this.button.domNode.display="none";
}
},_createContextMenu:function(){
var _34=new _c({targetNodeIds:[this.editor.iframe]});
var _35=_13;
_34.addChild(new _d({label:_35.selectTableLabel,onClick:dojo.hitch(this,"selectTable")}));
_34.addChild(new _e());
_34.addChild(new _d({label:_35.insertTableRowBeforeLabel,onClick:dojo.hitch(this,"modTable","insertTableRowBefore")}));
_34.addChild(new _d({label:_35.insertTableRowAfterLabel,onClick:dojo.hitch(this,"modTable","insertTableRowAfter")}));
_34.addChild(new _d({label:_35.insertTableColumnBeforeLabel,onClick:dojo.hitch(this,"modTable","insertTableColumnBefore")}));
_34.addChild(new _d({label:_35.insertTableColumnAfterLabel,onClick:dojo.hitch(this,"modTable","insertTableColumnAfter")}));
_34.addChild(new _e());
_34.addChild(new _d({label:_35.deleteTableRowLabel,onClick:dojo.hitch(this,"modTable","deleteTableRow")}));
_34.addChild(new _d({label:_35.deleteTableColumnLabel,onClick:dojo.hitch(this,"modTable","deleteTableColumn")}));
this.menu=_34;
}});
var _36=_1("dojox.editor.plugins.EditorTableDialog",[_b,_9,_a],{baseClass:"EditorTableDialog",templateString:_11,postMixInProperties:function(){
dojo.mixin(this,_13);
this.inherited(arguments);
},postCreate:function(){
dojo.addClass(this.domNode,this.baseClass);
this.inherited(arguments);
},onInsert:function(){
var _37=this.selectRow.get("value")||1,_38=this.selectCol.get("value")||1,_39=this.selectWidth.get("value"),_3a=this.selectWidthType.get("value"),_3b=this.selectBorder.get("value"),pad=this.selectPad.get("value"),_3c=this.selectSpace.get("value"),_3d="tbl_"+(new Date().getTime()),t="<table id=\""+_3d+"\"width=\""+_39+((_3a=="percent")?"%":"")+"\" border=\""+_3b+"\" cellspacing=\""+_3c+"\" cellpadding=\""+pad+"\">\n";
for(var r=0;r<_37;r++){
t+="\t<tr>\n";
for(var c=0;c<_38;c++){
t+="\t\t<td width=\""+(Math.floor(100/_38))+"%\">&nbsp;</td>\n";
}
t+="\t</tr>\n";
}
t+="</table><br />";
var cl=dojo.connect(this,"onHide",function(){
dojo.disconnect(cl);
var _3e=this;
setTimeout(function(){
_3e.destroyRecursive();
},10);
});
this.hide();
this.onBuildTable({htmlText:t,id:_3d});
},onCancel:function(){
var c=dojo.connect(this,"onHide",function(){
dojo.disconnect(c);
var _3f=this;
setTimeout(function(){
_3f.destroyRecursive();
},10);
});
},onBuildTable:function(_40){
}});
var _41=_1("dojox.editor.plugins.InsertTable",_25,{alwaysAvailable:true,modTable:function(){
var w=new _36({});
w.show();
var c=dojo.connect(w,"onBuildTable",this,function(obj){
dojo.disconnect(c);
this.editor.focus();
var res=this.editor.execCommand("inserthtml",obj.htmlText);
});
}});
var _42=_1([_b,_9,_a],{baseClass:"EditorTableDialog",table:null,tableAtts:{},templateString:_12,postMixInProperties:function(){
dojo.mixin(this,_13);
this.inherited(arguments);
},postCreate:function(){
dojo.addClass(this.domNode,this.baseClass);
this.inherited(arguments);
var w1=new this.colorPicker({params:this.params});
this.connect(w1,"onChange",function(_43){
if(!this._started){
return;
}
dijit.popup.close(w1);
this.setBrdColor(_43);
});
this.connect(w1,"onBlur",function(){
dijit.popup.close(w1);
});
this.connect(this.borderCol,"click",function(){
w1.set("value",this.brdColor,false);
dijit.popup.open({popup:w1,around:this.borderCol});
w1.focus();
});
var w2=new this.colorPicker({params:this.params});
this.connect(w2,"onChange",function(_44){
if(!this._started){
return;
}
dijit.popup.close(w2);
this.setBkColor(_44);
});
this.connect(w2,"onBlur",function(){
dijit.popup.close(w2);
});
this.connect(this.backgroundCol,"click",function(){
w2.set("value",this.bkColor,false);
dijit.popup.open({popup:w2,around:this.backgroundCol});
w2.focus();
});
this.own(w1,w2);
this.pickers=[w1,w2];
this.setBrdColor(_6.get(this.table,"borderColor"));
this.setBkColor(_6.get(this.table,"backgroundColor"));
var w=_5.get(this.table,"width");
if(!w){
w=this.table.style.width;
}
var p="pixels";
if(dojo.isString(w)&&w.indexOf("%")>-1){
p="percent";
w=w.replace(/%/,"");
}
if(w){
this.selectWidth.set("value",w);
this.selectWidthType.set("value",p);
}else{
this.selectWidth.set("value","");
this.selectWidthType.set("value","percent");
}
this.selectBorder.set("value",_5.get(this.table,"border"));
this.selectPad.set("value",_5.get(this.table,"cellPadding"));
this.selectSpace.set("value",_5.get(this.table,"cellSpacing"));
this.selectAlign.set("value",_5.get(this.table,"align"));
},startup:function(){
_2.forEach(this.pickers,function(_45){
_45.startup();
});
this.inherited(arguments);
},setBrdColor:function(_46){
this.brdColor=_46;
_6.set(this.borderCol,"backgroundColor",_46);
},setBkColor:function(_47){
this.bkColor=_47;
_6.set(this.backgroundCol,"backgroundColor",_47);
},onSet:function(){
_6.set(this.table,"borderColor",this.brdColor);
_6.set(this.table,"backgroundColor",this.bkColor);
if(this.selectWidth.get("value")){
_6.set(this.table,"width","");
_5.set(this.table,"width",(this.selectWidth.get("value")+((this.selectWidthType.get("value")=="pixels")?"":"%")));
}
_5.set(this.table,"border",this.selectBorder.get("value"));
_5.set(this.table,"cellPadding",this.selectPad.get("value"));
_5.set(this.table,"cellSpacing",this.selectSpace.get("value"));
_5.set(this.table,"align",this.selectAlign.get("value"));
var c=dojo.connect(this,"onHide",function(){
dojo.disconnect(c);
var _48=this;
setTimeout(function(){
_48.destroyRecursive();
},10);
});
this.hide();
},onCancel:function(){
var c=dojo.connect(this,"onHide",function(){
dojo.disconnect(c);
var _49=this;
setTimeout(function(){
_49.destroyRecursive();
},10);
});
},onSetTable:function(_4a){
}});
var _4b=_1("dojox.editor.plugins.ModifyTable",_25,{colorPicker:_f,modTable:function(){
if(!this.editor._tablePluginHandler.checkAvailable()){
return;
}
var o=this.getTableInfo();
var w=new _42({table:o.tbl,colorPicker:typeof this.colorPicker==="string"?require(this.colorPicker):this.colorPicker,params:this.params});
w.show();
this.connect(w,"onSetTable",function(_4c){
var o=this.getTableInfo();
_6.set(o.td,"backgroundColor",_4c);
});
}});
var _4d=_1([_8,_9,_a],{colorPicker:_10,templateString:"<div style='display: none; position: absolute; top: -10000; z-index: -10000'>"+"<div dojoType='dijit.TooltipDialog' dojoAttachPoint='dialog' class='dojoxEditorColorPicker'>"+"<div dojoAttachPoint='_colorPicker'></div>"+"<div style='margin: 0.5em 0em 0em 0em'>"+"<button dojoType='dijit.form.Button' type='submit' dojoAttachPoint='_setButton'>${buttonSet}</button>"+"&nbsp;"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_cancelButton'>${buttonCancel}</button>"+"</div>"+"</div>"+"</div>",widgetsInTemplate:true,constructor:function(){
dojo.mixin(this,_13);
},postCreate:function(){
var _4e=typeof this.colorPicker=="string"?require(this.colorPicker):this.colorPicker;
this._colorPicker=new _4e({params:this.params},this._colorPicker);
},startup:function(){
if(!this._started){
this.inherited(arguments);
this.connect(this.dialog,"execute",function(){
this.onChange(this.get("value"));
});
this.connect(this._cancelButton,"onClick",function(){
dijit.popup.close(this.dialog);
});
this.connect(this.dialog,"onCancel","onCancel");
dojo.style(this.domNode,"display","block");
}
},_setValueAttr:function(_4f,_50){
this._colorPicker.set("value",_4f,_50);
},_getValueAttr:function(){
return this._colorPicker.get("value");
},onChange:function(_51){
},onCancel:function(){
}});
var _52=_1("dojox.editor.plugins.ColorTableCell",_25,{colorPicker:_10,constructor:function(){
this.closable=true;
this.buttonClass=dijit.form.DropDownButton;
var _53=this,_54,_55={colorPicker:this.colorPicker,params:this.params};
if(!this.dropDown){
_54=new _4d(_55);
_54.startup();
this.dropDown=_54.dialog;
}else{
_54=this.dropDown;
_54.set(_55);
}
this.connect(_54,"onChange",function(_56){
this.editor.focus();
this.modTable(null,_56);
});
this.connect(_54,"onCancel",function(){
this.editor.focus();
});
_4.before(this.dropDown,"onOpen",function(){
var o=_53.getTableInfo(),tds=_53.getSelectedCells(o.tbl);
if(tds&&tds.length>0){
var t=tds[0]===_53.lastObject?tds[0]:tds[tds.length-1],_57;
while(t&&t!==_53.editor.document&&((_57=dojo.style(t,"backgroundColor"))==="transparent"||_57.indexOf("rgba")===0)){
t=t.parentNode;
}
if(_57!=="transparent"&&_57.indexOf("rgba")!==0){
_54.set("value",_3.fromString(_57).toHex());
}
}
});
this.connect(this,"setEditor",function(_58){
_58.onLoadDeferred.addCallback(dojo.hitch(this,function(){
this.connect(this.editor.editNode,"onmouseup",function(evt){
this.lastObject=evt.target;
});
}));
});
},_initButton:function(){
this.command=this.name;
this.label=this.editor.commands[this.command]=this._makeTitle(this.command);
this.inherited(arguments);
delete this.command;
this.onDisplayChanged(false);
},modTable:function(cmd,_59){
this.begEdit();
var o=this.getTableInfo();
var tds=this.getSelectedCells(o.tbl);
dojo.forEach(tds,function(td){
dojo.style(td,"backgroundColor",_59);
});
this.endEdit();
}});
function _5a(_5b){
return new _25(_5b);
};
_7.registry["insertTableRowBefore"]=_5a;
_7.registry["insertTableRowAfter"]=_5a;
_7.registry["insertTableColumnBefore"]=_5a;
_7.registry["insertTableColumnAfter"]=_5a;
_7.registry["deleteTableRow"]=_5a;
_7.registry["deleteTableColumn"]=_5a;
_7.registry["colorTableCell"]=function(_5c){
return new _52(_5c);
};
_7.registry["modifyTable"]=function(_5d){
return new _4b(_5d);
};
_7.registry["insertTable"]=function(_5e){
return new _41(_5e);
};
_7.registry["tableContextMenu"]=function(_5f){
return new _32(_5f);
};
return _25;
});
