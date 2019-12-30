//>>built
define("dojox/gantt/contextMenuTab",["./GanttTaskControl","dijit/Menu","dijit/Dialog","dijit/form/NumberSpinner","dijit/form/Button","dijit/form/CheckBox","dijit/form/DateTextBox","dijit/form/TimeTextBox","dijit/form/TextBox","dijit/form/Form","dijit/registry","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/_base/html","dojo/date/locale","dojo/request","dojo/dom","dojo/dom-class","dojo/domReady!"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,_e,_f,_10,_11,dom,_12){
return _c("dojox.gantt.contextMenuTab",[],{constructor:function(id,_13,_14,_15,_16,_17){
this.id=id;
this.arrItems=[];
this.TabItemContainer=null;
this.Description=_13;
this.tabMenu=_16;
this.type=_14;
this.object=null;
this.showObjectInfo=_15;
this.withDefaultValue=_17;
},preValueValidation:function(_18){
for(var i=0;i<_18.length;i++){
var _19=_18[i];
if(_19.required&&!_19.control.textbox.value){
return false;
}
}
return true;
},encodeDate:function(_1a){
return _1a.getFullYear()+"."+(_1a.getMonth()+1)+"."+_1a.getDate();
},decodeDate:function(_1b){
var arr=_1b.split(".");
return (arr.length<3)?"":(new Date(arr[0],parseInt(arr[1])-1,arr[2]));
},renameTaskAction:function(){
var _1c=this.arrItems[0].control.textbox.value;
if(_e.trim(_1c).length<=0){
return;
}
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.setName(_1c);
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
}
},renameProjectAction:function(){
var _1d=this.arrItems[0].control.textbox.value;
if(_e.trim(_1d).length<=0){
return;
}
if(!this.preValueValidation(this.arrItems)){
return;
}
this.object.setName(_1d);
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
}
},addTaskAction:function(){
if(!this.preValueValidation(this.arrItems)){
return;
}
var id=this.arrItems[0].control.textbox.value,_1e=this.arrItems[1].control.textbox.value,_1f=this.decodeDate(this.arrItems[2].control.textbox.value),_20=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_21=this.arrItems[5].control.textbox.value,_22=this.arrItems[6].control.textbox.value,_23=this.arrItems[7].control.textbox.value;
if(_e.trim(id).length<=0){
return;
}
if(this.object.insertTask(id,_1e,_1f,_20,pc,_23,_21,_22)){
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
var pr=this.object.project,id=this.arrItems[0].control.textbox.value,_24=this.arrItems[1].control.textbox.value,_25=this.decodeDate(this.arrItems[2].control.textbox.value),_26=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_27=this.arrItems[5].control.textbox.value;
if(_e.trim(id).length<=0){
return;
}
var _28=!this.object.parentTask?"":this.object.parentTask.taskItem.id;
var _29=this.object.taskItem.id;
if(pr.insertTask(id,_24,_25,_26,pc,_29,_27,_28)){
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
var pr=this.object.project,id=this.arrItems[0].control.textbox.value,_2a=this.arrItems[1].control.textbox.value,_2b=this.decodeDate(this.arrItems[2].control.textbox.value),_2c=this.arrItems[3].control.textbox.value,pc=this.arrItems[4].control.textbox.value,_2d=this.arrItems[5].control.textbox.value,_2e=this.object.taskItem.id,_2f="";
if(_e.trim(id).length<=0){
return;
}
if(pr.insertTask(id,_2a,_2b,_2c,pc,_2f,_2d,_2e)){
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
var id=this.arrItems[0].control.textbox.value,_30=this.arrItems[1].control.textbox.value,_31=this.decodeDate(this.arrItems[2].control.textbox.value);
if(_e.trim(id).length<=0||_e.trim(_30).length<=0){
return;
}
if(this.tabMenu.ganttChart.insertProject(id,_30,_31)){
this.hide();
}else{
alert("Please adjust your Customization");
return;
}
this.tabMenu.ganttChart.resource&&this.tabMenu.ganttChart.resource.reConstruct();
},addAction:function(_32){
this.actionFunc=this[_32];
},addItem:function(id,_33,key,_34){
var _35;
if(key=="startTime"||key=="startDate"){
_35=new _7({type:"text",constraints:{datePattern:"yyyy.M.d",strict:true}});
}else{
if(key=="percentage"){
_35=new _4({constraints:{max:100,min:0}});
}else{
if(key=="duration"){
_35=new _4({constraints:{min:0}});
}else{
_35=new _9();
}
}
}
this.arrItems.push({id:id,name:_33,control:_35,tab:this,key:key,required:_34});
},show:function(){
this.tabMenu.tabPanelDlg=this.tabMenu.tabPanelDlg||_b.byId(this.tabMenu.tabPanelDlgId)||new _3({title:"Settings"});
try{
this.tabMenu.tabPanelDlg.show();
}
catch(e){
return;
}
this.tabMenu.tabPanelDlg.titleNode.innerHTML=this.Description;
var _36=this.tabMenu.paneContentArea.firstChild.rows[1].cells[0].firstChild;
var _37,_38,row=null;
if(this.showObjectInfo){
if(this.object){
if(this.object.constructor==_1){
this.insertData(_36,"Id",this.object.taskItem.id);
this.insertData(_36,"Name",this.object.taskItem.name);
this.insertData(_36,"Start Time",this.encodeDate(this.object.taskItem.startTime));
this.insertData(_36,"Duration (hours)",this.object.taskItem.duration+" hours");
this.insertData(_36,"Percent Complete (%)",this.object.taskItem.percentage+"%");
this.insertData(_36,"Task Assignee",this.object.taskItem.taskOwner);
this.insertData(_36,"Previous Task Id",this.object.taskItem.previousTaskId);
}else{
this.insertData(_36,"Id",this.object.project.id);
this.insertData(_36,"Name",this.object.project.name);
this.insertData(_36,"Start date",this.encodeDate(this.object.project.startDate));
}
}
}
row=_36.insertRow(_36.rows.length);
_37=row.insertCell(row.cells.length);
_37.colSpan=2;
_37.innerHTML="<hr/>";
row=_36.insertRow(_36.rows.length);
_37=row.insertCell(row.cells.length);
_37.colSpan=2;
_12.add(_37,"ganttMenuDialogInputCellHeader");
_37.innerHTML="Customization: "+this.Description;
_d.forEach(this.arrItems,function(_39){
row=_36.insertRow(_36.rows.length);
_37=row.insertCell(row.cells.length);
_12.add(_37,"ganttMenuDialogInputCell");
_38=row.insertCell(row.cells.length);
_12.add(_38,"ganttMenuDialogInputCellValue");
_37.innerHTML=_39.name;
_38.appendChild(_39.control.domNode);
if(this.withDefaultValue&&this.object){
if(this.object.constructor==_1){
if(_39.key=="startTime"){
_39.control.textbox.value=this.encodeDate(this.object.taskItem.startTime);
}else{
_39.control.textbox.value=_39.key?this.object.taskItem[_39.key]:"";
}
}else{
if(_39.key=="startDate"){
_39.control.textbox.value=this.encodeDate(this.object.project.startDate);
}else{
_39.control.textbox.value=_39.key?(this.object.project[_39.key]||this.object[_39.key]||""):"";
}
}
}else{
_39.control.textbox.placeholder=_39.required?"---required---":"---optional---";
}
},this);
this.tabMenu.ok.onClick=_e.hitch(this,this.actionFunc);
this.tabMenu.cancel.onClick=_e.hitch(this,this.hide);
},hide:function(){
try{
this.tabMenu.tabPanelDlg.hide();
}
catch(e){
this.tabMenu.tabPanelDlg.destroy();
}
var _3a=this.tabMenu.paneContentArea.firstChild.rows[1].cells[0];
_3a.firstChild.parentNode.removeChild(_3a.firstChild);
_3a.innerHTML="<table></table>";
_12.add(_3a.firstChild,"ganttDialogContentCell");
},insertData:function(_3b,_3c,_3d){
var _3e,row=_3b.insertRow(_3b.rows.length),_3f=row.insertCell(row.cells.length);
_12.add(_3f,"ganttMenuDialogDescCell");
_3f.innerHTML=_3c;
_3e=row.insertCell(row.cells.length);
_12.add(_3e,"ganttMenuDialogDescCellValue");
_3e.innerHTML=_3d;
}});
});
