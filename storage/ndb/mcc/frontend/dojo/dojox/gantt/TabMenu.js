//>>built
define("dojox/gantt/TabMenu",["./contextMenuTab","./GanttTaskControl","./GanttProjectControl","dijit/Dialog","dijit/form/Button","dijit/form/Form","dijit/registry","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/date/locale","dojo/request","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dojo/keys","dojo/domReady!"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,on,_d,_e,_f,_10,_11,_12,_13){
return _8("dojox.gantt.TabMenu",[],{constructor:function(_14){
this.ganttChart=_14;
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
var _15=this.createTab(11,"Add Successor Task","t",true,this);
_15.addItem(1,"Id","id",true);
_15.addItem(2,"Name","name");
_15.addItem(3,"Start Time","startTime");
_15.addItem(4,"Duration (hours)","duration");
_15.addItem(5,"Percent Complete (%)","percentage");
_15.addItem(6,"Task Assignee","taskOwner");
_15.addAction("addSuccessorTaskAction");
var _16=this.createTab(10,"Add Child Task","t",true,this);
_16.addItem(1,"Id","id",true);
_16.addItem(2,"Name","name");
_16.addItem(3,"Start Time","startTime");
_16.addItem(4,"Duration (hours)","duration");
_16.addItem(5,"Percent Complete (%)","percentage");
_16.addItem(6,"Task Assignee","taskOwner");
_16.addAction("addChildTaskAction");
var _17=this.createTab(4,"Set Duration(hours)","t",true,this,true);
_17.addItem(1,"Duration (hours)","duration",true);
_17.addAction("durationUpdateAction");
var _18=this.createTab(5,"Set Complete Percentage (%)","t",true,this,true);
_18.addItem(1,"Percent Complete (%)","percentage",true);
_18.addAction("cpUpdateAction");
var _19=this.createTab(20,"Set Owner","t",true,this,true);
_19.addItem(1,"Task Assignee","taskOwner",true);
_19.addAction("ownerUpdateAction");
var _1a=this.createTab(13,"Set Previous Task","t",true,this);
_1a.addItem(1,"Previous Task Id","previousTaskId",true);
_1a.addAction("ptUpdateAction");
var _1b=this.createTab(1,"Rename Task","t",true,this,true);
_1b.addItem(1,"New Name","name",true);
_1b.addAction("renameTaskAction");
var _1c=this.createTab(2,"Delete Task","t",true,this);
_1c.addAction("deleteAction");
var _1d=this.createTab(12,"Add New Project","p",false,this);
_1d.addItem(1,"Id","id",true);
_1d.addItem(2,"Name","name",true);
_1d.addItem(3,"Start Date","startDate",true);
_1d.addAction("addProjectAction");
var _1e=this.createTab(8,"Set Complete Percentage (%)","p",true,this,true);
_1e.addItem(1,"Percent Complete (%)","percentage",true);
_1e.addAction("cpProjectAction");
var _1f=this.createTab(6,"Rename Project","p",true,this,true);
_1f.addItem(1,"New Name","name",true);
_1f.addAction("renameProjectAction");
var _20=this.createTab(7,"Delete Project","p",true,this);
_20.addAction("deleteProjectAction");
var _21=this.createTab(9,"Add New Task","p",true,this);
_21.addItem(1,"Id","id",true);
_21.addItem(2,"Name","name");
_21.addItem(3,"Start Time","startTime");
_21.addItem(4,"Duration (hours)","duration");
_21.addItem(5,"Percent Complete (%)","percentage");
_21.addItem(6,"Task Assignee","taskOwner");
_21.addItem(7,"Parent Task Id","parentTaskId");
_21.addItem(8,"Previous Task Id","previousTaskId");
_21.addAction("addTaskAction");
},createMenuPanel:function(){
this.menuPanel=_f.create("div",{innerHTML:"<table></table>",className:"ganttMenuPanel"},this.ganttChart.content);
_e.add(this.menuPanel.firstChild,"ganttContextMenu");
this.menuPanel.firstChild.cellPadding=0;
this.menuPanel.firstChild.cellSpacing=0;
},createTabPanel:function(){
this.tabPanelDlg=_7.byId(this.tabPanelDlgId)||new _4({title:"Settings"});
this.tabPanelDlgId=this.tabPanelDlg.id;
this.tabPanelDlg.closeButtonNode.style.display="none";
var _22=this.tabPanelDlg.containerNode;
this.paneContentArea=_f.create("div",{className:"dijitDialogPaneContentArea"},_22);
this.paneActionBar=_f.create("div",{className:"dijitDialogPaneActionBar"},_22);
this.paneContentArea.innerHTML="<table cellpadding=0 cellspacing=0><tr><th></th></tr><tr><td></td></tr></table>";
var _23=this.paneContentArea.firstChild.rows[0].cells[0];
_23.colSpan=2;
_23.innerHTML="Description: ";
_e.add(_23,"ganttDialogContentHeader");
var _24=this.paneContentArea.firstChild.rows[1].cells[0];
_24.innerHTML="<table></table>";
_e.add(_24.firstChild,"ganttDialogContentCell");
_24.align="center";
this.ok=new _5({label:"OK"});
this.cancel=new _5({label:"Cancel"});
this.paneActionBar.appendChild(this.ok.domNode);
this.paneActionBar.appendChild(this.cancel.domNode);
},addItemMenuPanel:function(tab){
var row=this.menuPanel.firstChild.insertRow(this.menuPanel.firstChild.rows.length);
var _25=_f.create("td",{className:"ganttContextMenuItem",innerHTML:tab.Description});
_11.set(_25,"tabIndex",0);
this.ganttChart._events.push(on(_25,"click",_a.hitch(this,function(){
try{
this.hide();
tab.show();
}
catch(e){
}
})));
this.ganttChart._events.push(on(_25,"keydown",_a.hitch(this,function(_26){
if(_26.keyCode!=_13.ENTER){
return;
}
try{
this.hide();
tab.show();
}
catch(e){
}
})));
this.ganttChart._events.push(on(_25,"mouseover",_a.hitch(this,function(){
_e.add(_25,"ganttContextMenuItemHover");
})));
this.ganttChart._events.push(on(_25,"mouseout",_a.hitch(this,function(){
_e.remove(_25,"ganttContextMenuItemHover");
})));
row.appendChild(_25);
},show:function(_27,_28){
if(_28.constructor==_2){
_9.forEach(this.arrTabs,function(tab){
if(tab.type=="t"){
tab.object=_28;
this.addItemMenuPanel(tab);
}
},this);
}else{
if(_28.constructor==_3){
_9.forEach(this.arrTabs,function(tab){
if(tab.type=="p"){
tab.object=_28;
this.addItemMenuPanel(tab);
}
},this);
}
}
this.isShow=true;
_10.set(this.menuPanel,{zIndex:15,visibility:"visible"});
var _29=_12.position(this.menuPanel,true),_2a=_12.position(this.ganttChart.content,true),pos=_12.position(_27,true);
if((pos.y+_29.h)>(_2a.y+_2a.h+50)){
this.menuPanel.style.top=pos.y-_29.h+pos.h+"px";
}else{
this.menuPanel.style.top=pos.y+"px";
}
if(_12.isBodyLtr()){
this.menuPanel.style.left=pos.x+pos.w+5+"px";
}else{
this.menuPanel.style.left=pos.x-_29.w-5+"px";
}
},hide:function(){
this.isShow=false;
this.menuPanel.style.visibility="hidden";
},clear:function(){
this.menuPanel.removeChild(this.menuPanel.firstChild);
this.menuPanel.innerHTML="<table></table>";
_e.add(this.menuPanel.firstChild,"ganttContextMenu");
this.menuPanel.firstChild.cellPadding=0;
this.menuPanel.firstChild.cellSpacing=0;
},createTab:function(id,_2b,_2c,_2d,_2e,_2f){
var tab=new _1(id,_2b,_2c,_2d,_2e,_2f);
this.arrTabs.push(tab);
return tab;
}});
});
