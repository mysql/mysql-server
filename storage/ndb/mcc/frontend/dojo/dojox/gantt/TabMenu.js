//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/dijit,dijit/Menu,dijit/Dialog,dijit/form/NumberSpinner,dijit/form/Button,dijit/form/CheckBox,dijit/form/DateTextBox,dijit/form/TimeTextBox,dojo/date/locale,dijit/form/Form,dojo/parser"],function(_1,_2,_3){
_2.provide("dojox.gantt.TabMenu");
_2.require("dijit.dijit");
_2.require("dijit.Menu");
_2.require("dijit.Dialog");
_2.require("dijit.form.NumberSpinner");
_2.require("dijit.form.Button");
_2.require("dijit.form.CheckBox");
_2.require("dijit.form.DateTextBox");
_2.require("dijit.form.TimeTextBox");
_2.require("dojo.date.locale");
_2.require("dijit.form.Form");
_2.require("dojo.parser");
(function(){
_2.declare("dojox.gantt.TabMenu",null,{constructor:function(_4){
this.ganttChart=_4;
this.menuPanel=null;
this.paneContentArea=null;
this.paneActionBar=null;
this.tabPanelDlg=null;
this.tabPanelDlgId=null;
this.arrTabs=[];
this.isShow=false;
this.buildContent();
},buildContent:function(){
this.createMenuPanel();
this.createTabPanel();
var _5=this.createTab(11,"Add Successor Task","t",true,this);
_5.addItem(1,"Id","id",true);
_5.addItem(2,"Name","name");
_5.addItem(3,"Start Time","startTime");
_5.addItem(4,"Duration (hours)","duration");
_5.addItem(5,"Percent Complete (%)","percentage");
_5.addItem(6,"Task Assignee","taskOwner");
_5.addAction("addSuccessorTaskAction");
var _6=this.createTab(10,"Add Child Task","t",true,this);
_6.addItem(1,"Id","id",true);
_6.addItem(2,"Name","name");
_6.addItem(3,"Start Time","startTime");
_6.addItem(4,"Duration (hours)","duration");
_6.addItem(5,"Percent Complete (%)","percentage");
_6.addItem(6,"Task Assignee","taskOwner");
_6.addAction("addChildTaskAction");
var _7=this.createTab(4,"Set Duration(hours)","t",true,this,true);
_7.addItem(1,"Duration (hours)","duration",true);
_7.addAction("durationUpdateAction");
var _8=this.createTab(5,"Set Complete Percentage (%)","t",true,this,true);
_8.addItem(1,"Percent Complete (%)","percentage",true);
_8.addAction("cpUpdateAction");
var _9=this.createTab(20,"Set Owner","t",true,this,true);
_9.addItem(1,"Task Assignee","taskOwner",true);
_9.addAction("ownerUpdateAction");
var _a=this.createTab(13,"Set Previous Task","t",true,this);
_a.addItem(1,"Previous Task Id","previousTaskId",true);
_a.addAction("ptUpdateAction");
var _b=this.createTab(1,"Rename Task","t",true,this,true);
_b.addItem(1,"New Name","name",true);
_b.addAction("renameTaskAction");
var _c=this.createTab(2,"Delete Task","t",true,this);
_c.addAction("deleteAction");
var _d=this.createTab(12,"Add New Project","p",false,this);
_d.addItem(1,"Id","id",true);
_d.addItem(2,"Name","name",true);
_d.addItem(3,"Start Date","startDate",true);
_d.addAction("addProjectAction");
var _e=this.createTab(8,"Set Complete Percentage (%)","p",true,this,true);
_e.addItem(1,"Percent Complete (%)","percentage",true);
_e.addAction("cpProjectAction");
var _f=this.createTab(6,"Rename Project","p",true,this,true);
_f.addItem(1,"New Name","name",true);
_f.addAction("renameProjectAction");
var _10=this.createTab(7,"Delete Project","p",true,this);
_10.addAction("deleteProjectAction");
var _11=this.createTab(9,"Add New Task","p",true,this);
_11.addItem(1,"Id","id",true);
_11.addItem(2,"Name","name");
_11.addItem(3,"Start Time","startTime");
_11.addItem(4,"Duration (hours)","duration");
_11.addItem(5,"Percent Complete (%)","percentage");
_11.addItem(6,"Task Assignee","taskOwner");
_11.addItem(7,"Parent Task Id","parentTaskId");
_11.addItem(8,"Previous Task Id","previousTaskId");
_11.addAction("addTaskAction");
},createMenuPanel:function(){
this.menuPanel=_2.create("div",{innerHTML:"<table></table>",className:"ganttMenuPanel"},this.ganttChart.content);
_2.addClass(this.menuPanel.firstChild,"ganttContextMenu");
this.menuPanel.firstChild.cellPadding=0;
this.menuPanel.firstChild.cellSpacing=0;
},createTabPanel:function(){
this.tabPanelDlg=_1.byId(this.tabPanelDlgId)||new _1.Dialog({title:"Settings"});
this.tabPanelDlgId=this.tabPanelDlg.id;
this.tabPanelDlg.closeButtonNode.style.display="none";
var _12=this.tabPanelDlg.containerNode;
this.paneContentArea=_2.create("div",{className:"dijitDialogPaneContentArea"},_12);
this.paneActionBar=_2.create("div",{className:"dijitDialogPaneActionBar"},_12);
this.paneContentArea.innerHTML="<table cellpadding=0 cellspacing=0><tr><th></th></tr><tr><td></td></tr></table>";
var _13=this.paneContentArea.firstChild.rows[0].cells[0];
_13.colSpan=2;
_13.innerHTML="Description: ";
_2.addClass(_13,"ganttDialogContentHeader");
var _14=this.paneContentArea.firstChild.rows[1].cells[0];
_14.innerHTML="<table></table>";
_2.addClass(_14.firstChild,"ganttDialogContentCell");
_14.align="center";
this.ok=new _1.form.Button({label:"OK"});
this.cancel=new _1.form.Button({label:"Cancel"});
this.paneActionBar.appendChild(this.ok.domNode);
this.paneActionBar.appendChild(this.cancel.domNode);
},addItemMenuPanel:function(tab){
var row=this.menuPanel.firstChild.insertRow(this.menuPanel.firstChild.rows.length);
var _15=_2.create("td",{className:"ganttContextMenuItem",innerHTML:tab.Description});
_2.attr(_15,"tabIndex",0);
this.ganttChart._events.push(_2.connect(_15,"onclick",this,function(){
try{
this.hide();
tab.show();
}
catch(e){
}
}));
this.ganttChart._events.push(_2.connect(_15,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
try{
this.hide();
tab.show();
}
catch(e){
}
}));
this.ganttChart._events.push(_2.connect(_15,"onmouseover",this,function(){
_2.addClass(_15,"ganttContextMenuItemHover");
}));
this.ganttChart._events.push(_2.connect(_15,"onmouseout",this,function(){
_2.removeClass(_15,"ganttContextMenuItemHover");
}));
row.appendChild(_15);
},show:function(_16,_17){
if(_17.constructor==_3.gantt.GanttTaskControl){
_2.forEach(this.arrTabs,function(tab){
if(tab.type=="t"){
tab.object=_17;
this.addItemMenuPanel(tab);
}
},this);
}else{
if(_17.constructor==_3.gantt.GanttProjectControl){
_2.forEach(this.arrTabs,function(tab){
if(tab.type=="p"){
tab.object=_17;
this.addItemMenuPanel(tab);
}
},this);
}
}
this.isShow=true;
_2.style(this.menuPanel,{zIndex:15,visibility:"visible"});
var _18=_2.position(this.menuPanel,true),_19=_2.position(this.ganttChart.content,true),pos=_2.coords(_16,true);
if((pos.y+_18.h)>(_19.y+_19.h+50)){
this.menuPanel.style.top=pos.y-_18.h+pos.h+"px";
}else{
this.menuPanel.style.top=pos.y+"px";
}
if(_2._isBodyLtr()){
this.menuPanel.style.left=pos.x+pos.w+5+"px";
}else{
this.menuPanel.style.left=pos.x-_18.w-5+"px";
}
},hide:function(){
this.isShow=false;
this.menuPanel.style.visibility="hidden";
},clear:function(){
this.menuPanel.removeChild(this.menuPanel.firstChild);
this.menuPanel.innerHTML="<table></table>";
_2.addClass(this.menuPanel.firstChild,"ganttContextMenu");
this.menuPanel.firstChild.cellPadding=0;
this.menuPanel.firstChild.cellSpacing=0;
},createTab:function(id,_1a,_1b,_1c,_1d,_1e){
var tab=new _3.gantt.contextMenuTab(id,_1a,_1b,_1c,_1d,_1e);
this.arrTabs.push(tab);
return tab;
}});
_2.declare("dojox.gantt.contextMenuTab",null,{constructor:function(id,_1f,_20,_21,_22,_23){
this.id=id;
this.arrItems=[];
this.TabItemContainer=null;
this.Description=_1f;
this.tabMenu=_22;
this.type=_20;
this.object=null;
this.showObjectInfo=_21;
this.withDefaultValue=_23;
},preValueValidation:function(_24){
for(var i=0;i<_24.length;i++){
var _25=_24[i];
if(_25.required&&!_25.control.textbox.value){
return false;
}
}
return true;
},encodeDate:function(_26){
return _26.getFullYear()+"."+(_26.getMonth()+1)+"."+_26.getDate();
},decodeDate:function(_27){
var arr=_27.split(".");
return (arr.length<3)?"":(new Date(arr[0],parseInt(arr[1])-1,arr[2]));
},renameTaskAction:function(){
var _28=this.arrItems[0].control.textbox.value;
if(_2.trim(_28).length<=0){
return;
}
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.setName(_28);
this.hide();
},deleteAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.project.deleteTask(this.object.taskItem.id);
this.hide();
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},durationUpdateAction:function(){
var d=this.arrItems[0].control.textbox.value;
if(!this.preValueValidation(this.arrItems)){
return;
}
if(this.object.setDuration(d)){
this.hide();
}else{
alert("Duration out of Range");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.refresh();
},cpUpdateAction:function(){
var p=this.arrItems[0].control.textbox.value;
if(!this.preValueValidation(this.arrItems)){
return;
}
if(this.object.setPercentCompleted(p)){
this.hide();
}else{
alert("Complete Percentage out of Range");
return;
}
},ownerUpdateAction:function(){
var to=this.arrItems[0].control.textbox.value;
if(!this.preValueValidation(this.arrItems)){
return;
}
if(this.object.setTaskOwner(to)){
this.hide();
}else{
alert("Task owner not Valid");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},ptUpdateAction:function(){
var p=this.arrItems[0].control.textbox.value;
if(!this.preValueValidation(this.arrItems)){
return;
}
if(this.object.setPreviousTask(p)){
this.hide();
}else{
alert("Please verify the Previous Task ("+p+")  and adjust its Time Range");
return;
}
},renameProjectAction:function(){
var _29=this.arrItems[0].control.textbox.value;
if(_2.trim(_29).length<=0){
return;
}
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.setName(_29);
this.hide();
},deleteProjectAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.ganttChart.deleteProject(this.object.project.id);
this.hide();
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},cpProjectAction:function(){
var p=this.arrItems[0].control.textbox.value;
if(!this.preValueValidation(this.arrItems)){
return;
}
if(this.object.setPercentCompleted(p)){
this.hide();
}else{
alert("Percentage not Acceptable");
return;
}
},addTaskAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
var id=this.arrItems[0].control.textbox.value,_2a=this.arrItems[1].control.textbox.value,_2b=this.decodeDate(this.arrItems[2].control.textbox.value),_2c=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_2d=this.arrItems[5].control.textbox.value,_2e=this.arrItems[6].control.textbox.value,_2f=this.arrItems[7].control.textbox.value;
if(_2.trim(id).length<=0){
return;
}
if(this.object.insertTask(id,_2a,_2b,_2c,pc,_2f,_2d,_2e)){
this.hide();
}else{
alert("Please adjust your Customization");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},addSuccessorTaskAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
var pr=this.object.project,id=this.arrItems[0].control.textbox.value,_30=this.arrItems[1].control.textbox.value,_31=this.decodeDate(this.arrItems[2].control.textbox.value),_32=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_33=this.arrItems[5].control.textbox.value;
if(_2.trim(id).length<=0){
return;
}
var _34=!this.object.parentTask?"":this.object.parentTask.taskItem.id;
var _35=this.object.taskItem.id;
if(pr.insertTask(id,_30,_31,_32,pc,_35,_33,_34)){
this.hide();
}else{
alert("Please adjust your Customization");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},addChildTaskAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
var pr=this.object.project,id=this.arrItems[0].control.textbox.value,_36=this.arrItems[1].control.textbox.value,_37=this.decodeDate(this.arrItems[2].control.textbox.value),_38=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_39=this.arrItems[5].control.textbox.value,_3a=this.object.taskItem.id,_3b="";
if(_2.trim(id).length<=0){
return;
}
if(pr.insertTask(id,_36,_37,_38,pc,_3b,_39,_3a)){
this.hide();
}else{
alert("Please adjust your Customization");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},addProjectAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
var id=this.arrItems[0].control.textbox.value,_3c=this.arrItems[1].control.textbox.value,_3d=this.decodeDate(this.arrItems[2].control.textbox.value);
if(_2.trim(id).length<=0||_2.trim(_3c).length<=0){
return;
}
if(this.tabMenu.ganttChart.insertProject(id,_3c,_3d)){
this.hide();
}else{
alert("Please adjust your Customization");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},addAction:function(_3e){
this.actionFunc=this[_3e];
},addItem:function(id,_3f,key,_40){
var _41;
if(key=="startTime"||key=="startDate"){
_41=new _1.form.DateTextBox({type:"text",constraints:{datePattern:"yyyy.M.d",strict:true}});
}else{
if(key=="percentage"){
_41=new _1.form.NumberSpinner({constraints:{max:100,min:0}});
}else{
if(key=="duration"){
_41=new _1.form.NumberSpinner({constraints:{min:0}});
}else{
_41=new _1.form.TextBox();
}
}
}
this.arrItems.push({id:id,name:_3f,control:_41,tab:this,key:key,required:_40});
},show:function(){
this.tabMenu.tabPanelDlg=this.tabMenu.tabPanelDlg||_1.byId(this.tabMenu.tabPanelDlgId)||new _1.Dialog({title:"Settings"});
try{
this.tabMenu.tabPanelDlg.show();
}
catch(e){
return;
}
this.tabMenu.tabPanelDlg.titleNode.innerHTML=this.Description;
var _42=this.tabMenu.paneContentArea.firstChild.rows[1].cells[0].firstChild,_43=this.tabMenu.paneActionBar;
var _44,_45,row=null;
if(this.showObjectInfo){
if(this.object){
if(this.object.constructor==_3.gantt.GanttTaskControl){
this.insertData(_42,"Id",this.object.taskItem.id);
this.insertData(_42,"Name",this.object.taskItem.name);
this.insertData(_42,"Start Time",this.encodeDate(this.object.taskItem.startTime));
this.insertData(_42,"Duration (hours)",this.object.taskItem.duration+" hours");
this.insertData(_42,"Percent Complete (%)",this.object.taskItem.percentage+"%");
this.insertData(_42,"Task Assignee",this.object.taskItem.taskOwner);
this.insertData(_42,"Previous Task Id",this.object.taskItem.previousTaskId);
}else{
this.insertData(_42,"Id",this.object.project.id);
this.insertData(_42,"Name",this.object.project.name);
this.insertData(_42,"Start date",this.encodeDate(this.object.project.startDate));
}
}
}
row=_42.insertRow(_42.rows.length);
_44=row.insertCell(row.cells.length);
_44.colSpan=2;
_44.innerHTML="<hr/>";
row=_42.insertRow(_42.rows.length);
_44=row.insertCell(row.cells.length);
_44.colSpan=2;
_2.addClass(_44,"ganttMenuDialogInputCellHeader");
_44.innerHTML="Customization: "+this.Description;
_2.forEach(this.arrItems,function(_46){
row=_42.insertRow(_42.rows.length);
_44=row.insertCell(row.cells.length);
_2.addClass(_44,"ganttMenuDialogInputCell");
_45=row.insertCell(row.cells.length);
_2.addClass(_45,"ganttMenuDialogInputCellValue");
_44.innerHTML=_46.name;
_45.appendChild(_46.control.domNode);
if(this.withDefaultValue&&this.object){
if(this.object.constructor==_3.gantt.GanttTaskControl){
if(_46.key=="startTime"){
_46.control.textbox.value=this.encodeDate(this.object.taskItem.startTime);
}else{
_46.control.textbox.value=_46.key?this.object.taskItem[_46.key]:"";
}
}else{
if(_46.key=="startDate"){
_46.control.textbox.value=this.encodeDate(this.object.project.startDate);
}else{
_46.control.textbox.value=_46.key?(this.object.project[_46.key]||this.object[_46.key]||""):"";
}
}
}else{
_46.control.textbox.placeholder=_46.required?"---required---":"---optional---";
}
},this);
this.tabMenu.ok.onClick=_2.hitch(this,this.actionFunc);
this.tabMenu.cancel.onClick=_2.hitch(this,this.hide);
},hide:function(){
try{
this.tabMenu.tabPanelDlg.hide();
}
catch(e){
this.tabMenu.tabPanelDlg.destroy();
}
var _47=this.tabMenu.paneContentArea.firstChild.rows[1].cells[0];
_47.firstChild.parentNode.removeChild(_47.firstChild);
_47.innerHTML="<table></table>";
_2.addClass(_47.firstChild,"ganttDialogContentCell");
},insertData:function(_48,_49,_4a){
var _4b,_4c,row=null;
row=_48.insertRow(_48.rows.length);
_4b=row.insertCell(row.cells.length);
_2.addClass(_4b,"ganttMenuDialogDescCell");
_4b.innerHTML=_49;
_4c=row.insertCell(row.cells.length);
_2.addClass(_4c,"ganttMenuDialogDescCellValue");
_4c.innerHTML=_4a;
}});
})();
});
