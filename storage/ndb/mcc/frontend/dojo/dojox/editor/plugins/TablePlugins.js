//>>built
define("dojox/editor/plugins/TablePlugins",["dojo","dijit","dojox","dijit/_base/popup","dijit/_Widget","dijit/_TemplatedMixin","dijit/_WidgetsInTemplateMixin","dijit/Menu","dijit/MenuItem","dijit/MenuSeparator","dijit/TooltipDialog","dijit/form/Button","dijit/form/DropDownButton","dijit/Dialog","dijit/form/TextBox","dijit/form/FilteringSelect","dijit/popup","dijit/_editor/_Plugin","dijit/_editor/range","dijit/_editor/selection","dijit/ColorPalette","dojox/widget/ColorPicker","dojo/_base/connect","dojo/_base/declare","dojo/i18n","dojo/i18n!dojox/editor/plugins/nls/TableDialog"],function(_1,_2,_3){
_1.experimental("dojox.editor.plugins.TablePlugins");
_1.declare("dojox.editor.plugins._TableHandler",_2._editor._Plugin,{tablesConnected:false,currentlyAvailable:false,alwaysAvailable:false,availableCurrentlySet:false,initialized:false,tableData:null,shiftKeyDown:false,editorDomNode:null,undoEnabled:true,refCount:0,doMixins:function(){
_1.mixin(this.editor,{getAncestorElement:function(_4){
return _1.withGlobal(this.window,"getAncestorElement",_2._editor.selection,[_4]);
},hasAncestorElement:function(_5){
return _1.withGlobal(this.window,"hasAncestorElement",_2._editor.selection,[_5]);
},selectElement:function(_6){
_1.withGlobal(this.window,"selectElement",_2._editor.selection,[_6]);
},byId:function(id){
return _1.withGlobal(this.window,"byId",_1,[id]);
},query:function(_7,_8,_9){
var ar=_1.withGlobal(this.window,"query",_1,[_7,_8]);
return (_9)?ar[0]:ar;
}});
},initialize:function(_a){
this.refCount++;
_a.customUndo=true;
if(this.initialized){
return;
}
this.initialized=true;
this.editor=_a;
this.editor._tablePluginHandler=this;
_a.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.editorDomNode=this.editor.editNode||this.editor.iframe.document.body.firstChild;
this._myListeners=[];
this._myListeners.push(_1.connect(this.editorDomNode,"mouseup",this.editor,"onClick"));
this._myListeners.push(_1.connect(this.editor,"onDisplayChanged",this,"checkAvailable"));
this._myListeners.push(_1.connect(this.editor,"onBlur",this,"checkAvailable"));
this.doMixins();
this.connectDraggable();
}));
},getTableInfo:function(_b){
if(_b){
this._tempStoreTableData(false);
}
if(this.tableData){
return this.tableData;
}
var tr,_c,td,_d,_e,_f,_10,_11;
td=this.editor.getAncestorElement("td");
if(td){
tr=td.parentNode;
}
_e=this.editor.getAncestorElement("table");
_d=_1.query("td",_e);
_d.forEach(function(d,i){
if(td==d){
_10=i;
}
});
_c=_1.query("tr",_e);
_c.forEach(function(r,i){
if(tr==r){
_11=i;
}
});
_f=_d.length/_c.length;
var o={tbl:_e,td:td,tr:tr,trs:_c,tds:_d,rows:_c.length,cols:_f,tdIndex:_10,trIndex:_11,colIndex:_10%_f};
this.tableData=o;
this._tempStoreTableData(500);
return this.tableData;
},connectDraggable:function(){
if(!_1.isIE){
return;
}
this.editorDomNode.ondragstart=_1.hitch(this,"onDragStart");
this.editorDomNode.ondragend=_1.hitch(this,"onDragEnd");
},onDragStart:function(){
var e=window.event;
if(!e.srcElement.id){
e.srcElement.id="tbl_"+(new Date().getTime());
}
},onDragEnd:function(){
var e=window.event;
var _12=e.srcElement;
var id=_12.id;
var win=this.editor.window;
if(_12.tagName.toLowerCase()=="table"){
setTimeout(function(){
var _13=_1.withGlobal(win,"byId",_1,[id]);
_1.removeAttr(_13,"align");
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
this.currentlyAvailable=this.editor.focused?this.editor.hasAncestorElement("table"):false;
if(this.currentlyAvailable){
this.connectTableKeys();
}else{
this.disconnectTableKeys();
}
this._tempAvailability(500);
_1.publish(this.editor.id+"_tablePlugins",[this.currentlyAvailable]);
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
},_tempStoreTableData:function(_14){
if(_14===true){
}else{
if(_14===false){
this.tableData=null;
}else{
if(_14===undefined){
console.warn("_tempStoreTableData must be passed an argument");
}else{
setTimeout(_1.hitch(this,function(){
this.tableData=null;
}),_14);
}
}
}
},_tempAvailability:function(_15){
if(_15===true){
this.availableCurrentlySet=true;
}else{
if(_15===false){
this.availableCurrentlySet=false;
}else{
if(_15===undefined){
console.warn("_tempAvailability must be passed an argument");
}else{
this.availableCurrentlySet=true;
setTimeout(_1.hitch(this,function(){
this.availableCurrentlySet=false;
}),_15);
}
}
}
},connectTableKeys:function(){
if(this.tablesConnected){
return;
}
this.tablesConnected=true;
var _16=(this.editor.iframe)?this.editor.document:this.editor.editNode;
this.cnKeyDn=_1.connect(_16,"onkeydown",this,"onKeyDown");
this.cnKeyUp=_1.connect(_16,"onkeyup",this,"onKeyUp");
this._myListeners.push(_1.connect(_16,"onkeypress",this,"onKeyUp"));
},disconnectTableKeys:function(){
_1.disconnect(this.cnKeyDn);
_1.disconnect(this.cnKeyUp);
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
_1.stopEvent(evt);
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
_1.stopEvent(evt);
}
},onDisplayChanged:function(){
this.currentlyAvailable=false;
this._tempStoreTableData(false);
this._tempAvailability(false);
this.checkAvailable();
},uninitialize:function(_17){
if(this.editor==_17){
this.refCount--;
if(!this.refCount&&this.initialized){
if(this.tablesConnected){
this.disconnectTableKeys();
}
this.initialized=false;
_1.forEach(this._myListeners,function(l){
_1.disconnect(l);
});
delete this._myListeners;
delete this.editor._tablePluginHandler;
delete this.editor;
}
this.inherited(arguments);
}
}});
_1.declare("dojox.editor.plugins.TablePlugins",_2._editor._Plugin,{iconClassPrefix:"editorIcon",useDefaultCommand:false,buttonClass:_2.form.Button,commandName:"",label:"",alwaysAvailable:false,undoEnabled:true,onDisplayChanged:function(_18){
if(!this.alwaysAvailable){
this.available=_18;
this.button.set("disabled",!this.available);
}
},setEditor:function(_19){
this.editor=_19;
this.editor.customUndo=true;
this.inherited(arguments);
this._availableTopic=_1.subscribe(this.editor.id+"_tablePlugins",this,"onDisplayChanged");
this.onEditorLoaded();
},onEditorLoaded:function(){
if(!this.editor._tablePluginHandler){
var _1a=new _3.editor.plugins._TableHandler();
_1a.initialize(this.editor);
}else{
this.editor._tablePluginHandler.initialize(this.editor);
}
},selectTable:function(){
var o=this.getTableInfo();
if(o&&o.tbl){
_1.withGlobal(this.editor.window,"selectElement",_2._editor.selection,[o.tbl]);
}
},_initButton:function(){
this.command=this.commandName;
this.label=this.editor.commands[this.command]=this._makeTitle(this.command);
this.inherited(arguments);
delete this.command;
this.connect(this.button,"onClick","modTable");
this.onDisplayChanged(false);
},modTable:function(cmd,_1b){
this.begEdit();
var o=this.getTableInfo();
var sw=(_1.isString(cmd))?cmd:this.commandName;
var r,c,i;
var _1c=false;
if(_1.isIE){
this.editor.focus();
}
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
_1c=true;
break;
case "insertTableColumnAfter":
o.trs.forEach(function(r){
c=r.insertCell(o.colIndex+1);
c.innerHTML="&nbsp;";
});
_1c=true;
break;
case "deleteTableRow":
o.tbl.deleteRow(o.trIndex);
break;
case "deleteTableColumn":
o.trs.forEach(function(tr){
tr.deleteCell(o.colIndex);
});
_1c=true;
break;
case "modifyTable":
break;
case "insertTable":
break;
}
if(_1c){
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
var _1d=this.editor.getValue();
this.editor.setValue(this.valBeforeUndo);
this.editor.replaceValue(_1d);
}
this.editor.onDisplayChanged();
}
},makeColumnsEven:function(){
setTimeout(_1.hitch(this,function(){
var o=this.getTableInfo(true);
var w=Math.floor(100/o.cols);
o.tds.forEach(function(d){
_1.attr(d,"width",w+"%");
});
}),10);
},getTableInfo:function(_1e){
return this.editor._tablePluginHandler.getTableInfo(_1e);
},_makeTitle:function(str){
var ns=[];
_1.forEach(str,function(c,i){
if(c.charCodeAt(0)<91&&i>0&&ns[i-1].charCodeAt(0)!=32){
ns.push(" ");
}
if(i===0){
c=c.toUpperCase();
}
ns.push(c);
});
return ns.join("");
},getSelectedCells:function(){
var _1f=[];
var tbl=this.getTableInfo().tbl;
this.editor._tablePluginHandler._prepareTable(tbl);
var e=this.editor;
var _20=_1.withGlobal(e.window,"getSelectedHtml",_2._editor.selection,[null]);
var str=_20.match(/id="*\w*"*/g);
_1.forEach(str,function(a){
var id=a.substring(3,a.length);
if(id.charAt(0)=="\""&&id.charAt(id.length-1)=="\""){
id=id.substring(1,id.length-1);
}
var _21=e.byId(id);
if(_21&&_21.tagName.toLowerCase()=="td"){
_1f.push(_21);
}
},this);
if(!_1f.length){
var sel=_2.range.getSelection(e.window);
if(sel.rangeCount){
var r=sel.getRangeAt(0);
var _22=r.startContainer;
while(_22&&_22!=e.editNode&&_22!=e.document){
if(_22.nodeType===1){
var tg=_22.tagName?_22.tagName.toLowerCase():"";
if(tg==="td"){
return [_22];
}
}
_22=_22.parentNode;
}
}
}
return _1f;
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
_1.unsubscribe(this._availableTopic);
this.editor._tablePluginHandler.uninitialize(this.editor);
}});
_1.declare("dojox.editor.plugins.TableContextMenu",_3.editor.plugins.TablePlugins,{constructor:function(){
this.connect(this,"setEditor",function(_23){
_23.onLoadDeferred.addCallback(_1.hitch(this,function(){
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
if(this.commandName=="tableContextMenu"){
this.button.domNode.display="none";
}
},_createContextMenu:function(){
var _24=new _2.Menu({targetNodeIds:[this.editor.iframe]});
var _25=_1.i18n.getLocalization("dojox.editor.plugins","TableDialog",this.lang);
_24.addChild(new _2.MenuItem({label:_25.selectTableLabel,onClick:_1.hitch(this,"selectTable")}));
_24.addChild(new _2.MenuSeparator());
_24.addChild(new _2.MenuItem({label:_25.insertTableRowBeforeLabel,onClick:_1.hitch(this,"modTable","insertTableRowBefore")}));
_24.addChild(new _2.MenuItem({label:_25.insertTableRowAfterLabel,onClick:_1.hitch(this,"modTable","insertTableRowAfter")}));
_24.addChild(new _2.MenuItem({label:_25.insertTableColumnBeforeLabel,onClick:_1.hitch(this,"modTable","insertTableColumnBefore")}));
_24.addChild(new _2.MenuItem({label:_25.insertTableColumnAfterLabel,onClick:_1.hitch(this,"modTable","insertTableColumnAfter")}));
_24.addChild(new _2.MenuSeparator());
_24.addChild(new _2.MenuItem({label:_25.deleteTableRowLabel,onClick:_1.hitch(this,"modTable","deleteTableRow")}));
_24.addChild(new _2.MenuItem({label:_25.deleteTableColumnLabel,onClick:_1.hitch(this,"modTable","deleteTableColumn")}));
this.menu=_24;
}});
_1.declare("dojox.editor.plugins.InsertTable",_3.editor.plugins.TablePlugins,{alwaysAvailable:true,modTable:function(){
var w=new _3.editor.plugins.EditorTableDialog({});
w.show();
var c=_1.connect(w,"onBuildTable",this,function(obj){
_1.disconnect(c);
var res=this.editor.execCommand("inserthtml",obj.htmlText);
});
}});
_1.declare("dojox.editor.plugins.ModifyTable",_3.editor.plugins.TablePlugins,{modTable:function(){
if(!this.editor._tablePluginHandler.checkAvailable()){
return;
}
var o=this.getTableInfo();
var w=new _3.editor.plugins.EditorModifyTableDialog({table:o.tbl});
w.show();
this.connect(w,"onSetTable",function(_26){
var o=this.getTableInfo();
_1.attr(o.td,"bgcolor",_26);
});
}});
_1.declare("dojox.editor.plugins._CellColorDropDown",[_2._Widget,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{templateString:"<div style='display: none; position: absolute; top: -10000; z-index: -10000'>"+"<div dojoType='dijit.TooltipDialog' dojoAttachPoint='dialog' class='dojoxEditorColorPicker'>"+"<div dojoType='dojox.widget.ColorPicker' dojoAttachPoint='_colorPicker'></div>"+"<div style='margin: 0.5em 0em 0em 0em'>"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_setButton'>${buttonSet}</button>"+"&nbsp;"+"<button dojoType='dijit.form.Button' type='button' dojoAttachPoint='_cancelButton'>${buttonCancel}</button>"+"</div>"+"</div>"+"</div>",widgetsInTemplate:true,constructor:function(){
var _27=_1.i18n.getLocalization("dojox.editor.plugins","TableDialog");
_1.mixin(this,_27);
},startup:function(){
if(!this._started){
this.inherited(arguments);
this.connect(this._setButton,"onClick",function(){
this.onChange(this.get("value"));
});
this.connect(this._cancelButton,"onClick",function(){
_2.popup.close(this.dialog);
this.onCancel();
});
_1.style(this.domNode,"display","block");
}
},_setValueAttr:function(_28,_29){
this._colorPicker.set("value",_28,_29);
},_getValueAttr:function(){
return this._colorPicker.get("value");
},setColor:function(_2a){
this._colorPicker.setColor(_2a,false);
},onChange:function(_2b){
},onCancel:function(){
}});
_1.declare("dojox.editor.plugins.ColorTableCell",_3.editor.plugins.TablePlugins,{constructor:function(){
this.closable=true;
this.buttonClass=_2.form.DropDownButton;
var _2c=new _3.editor.plugins._CellColorDropDown();
_1.body().appendChild(_2c.domNode);
_2c.startup();
this.dropDown=_2c.dialog;
this.connect(_2c,"onChange",function(_2d){
this.modTable(null,_2d);
this.editor.focus();
});
this.connect(_2c,"onCancel",function(_2e){
this.editor.focus();
});
this.connect(_2c.dialog,"onOpen",function(){
var o=this.getTableInfo(),tds=this.getSelectedCells(o.tbl);
if(tds&&tds.length>0){
var t=tds[0]==this.lastObject?tds[0]:tds[tds.length-1],_2f;
while(t&&((_2f=_1.style(t,"backgroundColor"))=="transparent"||_2f.indexOf("rgba")==0)){
t=t.parentNode;
}
_2f=_1.style(t,"backgroundColor");
if(_2f!="transparent"&&_2f.indexOf("rgba")!=0){
_2c.setColor(_2f);
}
}
});
this.connect(this,"setEditor",function(_30){
_30.onLoadDeferred.addCallback(_1.hitch(this,function(){
this.connect(this.editor.editNode,"onmouseup",function(evt){
this.lastObject=evt.target;
});
}));
});
},_initButton:function(){
this.command=this.commandName;
this.label=this.editor.commands[this.command]=this._makeTitle(this.command);
this.inherited(arguments);
delete this.command;
this.onDisplayChanged(false);
},modTable:function(cmd,_31){
this.begEdit();
var o=this.getTableInfo();
var tds=this.getSelectedCells(o.tbl);
_1.forEach(tds,function(td){
_1.style(td,"backgroundColor",_31);
});
this.endEdit();
}});
_1.declare("dojox.editor.plugins.EditorTableDialog",[_2.Dialog,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{baseClass:"EditorTableDialog",templateString:_1.cache("dojox.editor.plugins","resources/insertTable.html"),postMixInProperties:function(){
var _32=_1.i18n.getLocalization("dojox.editor.plugins","TableDialog",this.lang);
_1.mixin(this,_32);
this.inherited(arguments);
},postCreate:function(){
_1.addClass(this.domNode,this.baseClass);
this.inherited(arguments);
},onInsert:function(){
var _33=this.selectRow.get("value")||1,_34=this.selectCol.get("value")||1,_35=this.selectWidth.get("value"),_36=this.selectWidthType.get("value"),_37=this.selectBorder.get("value"),pad=this.selectPad.get("value"),_38=this.selectSpace.get("value"),_39="tbl_"+(new Date().getTime()),t="<table id=\""+_39+"\"width=\""+_35+((_36=="percent")?"%":"")+"\" border=\""+_37+"\" cellspacing=\""+_38+"\" cellpadding=\""+pad+"\">\n";
for(var r=0;r<_33;r++){
t+="\t<tr>\n";
for(var c=0;c<_34;c++){
t+="\t\t<td width=\""+(Math.floor(100/_34))+"%\">&nbsp;</td>\n";
}
t+="\t</tr>\n";
}
t+="</table><br />";
this.onBuildTable({htmlText:t,id:_39});
var cl=_1.connect(this,"onHide",function(){
_1.disconnect(cl);
var _3a=this;
setTimeout(function(){
_3a.destroyRecursive();
},10);
});
this.hide();
},onCancel:function(){
var c=_1.connect(this,"onHide",function(){
_1.disconnect(c);
var _3b=this;
setTimeout(function(){
_3b.destroyRecursive();
},10);
});
},onBuildTable:function(_3c){
}});
_1.declare("dojox.editor.plugins.EditorModifyTableDialog",[_2.Dialog,_2._TemplatedMixin,_2._WidgetsInTemplateMixin],{baseClass:"EditorTableDialog",table:null,tableAtts:{},templateString:_1.cache("dojox.editor.plugins","resources/modifyTable.html"),postMixInProperties:function(){
var _3d=_1.i18n.getLocalization("dojox.editor.plugins","TableDialog",this.lang);
_1.mixin(this,_3d);
this.inherited(arguments);
},postCreate:function(){
_1.addClass(this.domNode,this.baseClass);
this.inherited(arguments);
this._cleanupWidgets=[];
var w1=new _2.ColorPalette({});
this.connect(w1,"onChange",function(_3e){
_2.popup.close(w1);
this.setBrdColor(_3e);
});
this.connect(w1,"onBlur",function(){
_2.popup.close(w1);
});
this.connect(this.borderCol,"click",function(){
_2.popup.open({popup:w1,around:this.borderCol});
w1.focus();
});
var w2=new _2.ColorPalette({});
this.connect(w2,"onChange",function(_3f){
_2.popup.close(w2);
this.setBkColor(_3f);
});
this.connect(w2,"onBlur",function(){
_2.popup.close(w2);
});
this.connect(this.backgroundCol,"click",function(){
_2.popup.open({popup:w2,around:this.backgroundCol});
w2.focus();
});
this._cleanupWidgets.push(w1);
this._cleanupWidgets.push(w2);
this.setBrdColor(_1.attr(this.table,"bordercolor"));
this.setBkColor(_1.attr(this.table,"bgcolor"));
var w=_1.attr(this.table,"width");
if(!w){
w=this.table.style.width;
}
var p="pixels";
if(_1.isString(w)&&w.indexOf("%")>-1){
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
this.selectBorder.set("value",_1.attr(this.table,"border"));
this.selectPad.set("value",_1.attr(this.table,"cellPadding"));
this.selectSpace.set("value",_1.attr(this.table,"cellSpacing"));
this.selectAlign.set("value",_1.attr(this.table,"align"));
},setBrdColor:function(_40){
this.brdColor=_40;
_1.style(this.borderCol,"backgroundColor",_40);
},setBkColor:function(_41){
this.bkColor=_41;
_1.style(this.backgroundCol,"backgroundColor",_41);
},onSet:function(){
_1.attr(this.table,"borderColor",this.brdColor);
_1.attr(this.table,"bgColor",this.bkColor);
if(this.selectWidth.get("value")){
_1.style(this.table,"width","");
_1.attr(this.table,"width",(this.selectWidth.get("value")+((this.selectWidthType.get("value")=="pixels")?"":"%")));
}
_1.attr(this.table,"border",this.selectBorder.get("value"));
_1.attr(this.table,"cellPadding",this.selectPad.get("value"));
_1.attr(this.table,"cellSpacing",this.selectSpace.get("value"));
_1.attr(this.table,"align",this.selectAlign.get("value"));
var c=_1.connect(this,"onHide",function(){
_1.disconnect(c);
var _42=this;
setTimeout(function(){
_42.destroyRecursive();
},10);
});
this.hide();
},onCancel:function(){
var c=_1.connect(this,"onHide",function(){
_1.disconnect(c);
var _43=this;
setTimeout(function(){
_43.destroyRecursive();
},10);
});
},onSetTable:function(_44){
},destroy:function(){
this.inherited(arguments);
_1.forEach(this._cleanupWidgets,function(w){
if(w&&w.destroy){
w.destroy();
}
});
delete this._cleanupWidgets;
}});
_1.subscribe(_2._scopeName+".Editor.getPlugin",null,function(o){
if(o.plugin){
return;
}
if(o.args&&o.args.command){
var cmd=o.args.command.charAt(0).toLowerCase()+o.args.command.substring(1,o.args.command.length);
switch(cmd){
case "insertTableRowBefore":
case "insertTableRowAfter":
case "insertTableColumnBefore":
case "insertTableColumnAfter":
case "deleteTableRow":
case "deleteTableColumn":
o.plugin=new _3.editor.plugins.TablePlugins({commandName:cmd});
break;
case "colorTableCell":
o.plugin=new _3.editor.plugins.ColorTableCell({commandName:cmd});
break;
case "modifyTable":
o.plugin=new _3.editor.plugins.ModifyTable({commandName:cmd});
break;
case "insertTable":
o.plugin=new _3.editor.plugins.InsertTable({commandName:cmd});
break;
case "tableContextMenu":
o.plugin=new _3.editor.plugins.TableContextMenu({commandName:cmd});
break;
}
}
});
return _3.editor.plugins.TablePlugins;
});
