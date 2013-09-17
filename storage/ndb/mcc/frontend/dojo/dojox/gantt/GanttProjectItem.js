//>>built
define(["dijit","dojo","dojox","dojo/require!dojox/gantt/GanttTaskItem,dojo/date/locale,dijit/focus"],function(_1,_2,_3){
_2.provide("dojox.gantt.GanttProjectItem");
_2.require("dojox.gantt.GanttTaskItem");
_2.require("dojo.date.locale");
_2.require("dijit.focus");
_2.declare("dojox.gantt.GanttProjectControl",null,{constructor:function(_4,_5){
this.project=_5;
this.ganttChart=_4;
this.descrProject=null;
this.projectItem=null;
this.projectNameItem=null;
this.posY=0;
this.posX=0;
this.nextProject=null;
this.previousProject=null;
this.arrTasks=[];
this.percentage=0;
this.duration=0;
},checkWidthProjectNameItem:function(){
if(this.projectNameItem.offsetWidth+this.projectNameItem.offsetLeft>this.ganttChart.maxWidthTaskNames){
var _6=this.projectNameItem.offsetWidth+this.projectNameItem.offsetLeft-this.ganttChart.maxWidthTaskNames;
var _7=Math.round(_6/(this.projectNameItem.offsetWidth/this.projectNameItem.firstChild.length));
var _8=this.project.name.substring(0,this.projectNameItem.firstChild.length-_7-3);
_8+="...";
this.projectNameItem.innerHTML=_8;
}
},refreshProjectItem:function(_9){
this.percentage=this.getPercentCompleted();
_2.style(_9,{"left":this.posX+"px","width":this.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _a=_9.firstChild;
var _b=this.duration*this.ganttChart.pixelsPerWorkHour;
_a.width=((_b==0)?1:_b)+"px";
_a.style.width=((_b==0)?1:_b)+"px";
var _c=_a.rows[0];
if(this.percentage!=-1){
if(this.percentage!=0){
var _d=_c.firstChild;
_d.width=this.percentage+"%";
var _e=_d.firstChild;
_2.style(_e,{width:(!this.duration?1:(this.percentage*this.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.percentage!=100){
var _d=_c.lastChild;
_d.width=(100-this.percentage)+"%";
var _e=_d.firstChild;
_2.style(_e,{width:(!this.duration?1:((100-this.percentage)*this.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}else{
var _d=_c.firstChild;
_d.width="1px";
var _e=_d.firstChild;
_2.style(_e,{width:"1px",height:this.ganttChart.heightTaskItem+"px"});
}
var _f=_9.lastChild;
var _10=_f.firstChild;
_2.style(_10,{height:this.ganttChart.heightTaskItem+"px",width:(!this.duration?1:(this.duration*this.ganttChart.pixelsPerWorkHour))+"px"});
var _11=_10.rows[0];
var _12=_11.firstChild;
_12.height=this.ganttChart.heightTaskItem+"px";
if(this.project.parentTasks.length==0){
_9.style.display="none";
}
return _9;
},refreshDescrProject:function(_13){
var _14=(this.posX+this.duration*this.ganttChart.pixelsPerWorkHour+10);
_2.style(_13,{"left":_14+"px"});
if(this.project.parentTasks.length==0){
this.descrProject.style.visibility="hidden";
}
return _13;
},postLoadData:function(){
},refresh:function(){
var _15=this.ganttChart.contentData.firstChild;
this.posX=(this.project.startDate-this.ganttChart.startDate)/(60*60*1000)*this.ganttChart.pixelsPerHour;
this.refreshProjectItem(this.projectItem[0]);
this.refreshDescrProject(this.projectItem[0].nextSibling);
return this;
},create:function(){
var _16=this.ganttChart.contentData.firstChild;
this.posX=(this.project.startDate-this.ganttChart.startDate)/(60*60*1000)*this.ganttChart.pixelsPerHour;
if(this.previousProject){
if(this.previousProject.arrTasks.length>0){
var _17=this.ganttChart.getLastChildTask(this.previousProject.arrTasks[this.previousProject.arrTasks.length-1]);
this.posY=parseInt(_17.cTaskItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}else{
this.posY=parseInt(this.previousProject.projectItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}
}else{
this.posY=6;
}
var _18=this.ganttChart.panelNames.firstChild;
this.projectNameItem=this.createProjectNameItem();
_18.appendChild(this.projectNameItem);
this.checkWidthProjectNameItem();
this.projectItem=[this.createProjectItem(),[]];
_16.appendChild(this.projectItem[0]);
_16.appendChild(this.createDescrProject());
this.adjustPanelTime();
},getTaskById:function(id){
for(var i=0;i<this.arrTasks.length;i++){
var _19=this.arrTasks[i];
var _1a=this.searchTaskInTree(_19,id);
if(_1a){
return _1a;
}
}
return null;
},searchTaskInTree:function(_1b,id){
if(_1b.taskItem.id==id){
return _1b;
}else{
for(var i=0;i<_1b.childTask.length;i++){
var _1c=_1b.childTask[i];
if(_1c.taskItem.id==id){
return _1c;
}else{
if(_1c.childTask.length>0){
var _1c=this.searchTaskInTree(_1c,id);
if(_1c){
return _1c;
}
}
}
}
}
return null;
},shiftProjectItem:function(){
var _1d=null;
var _1e=null;
var _1f=parseInt(this.projectItem[0].style.left);
var _20=parseInt(this.projectItem[0].firstChild.style.width)+parseInt(this.projectItem[0].style.left);
var _21=parseInt(this.projectItem[0].firstChild.style.width);
for(var i=0;i<this.arrTasks.length;i++){
var _22=this.arrTasks[i];
var _23=parseInt(_22.cTaskItem[0].style.left);
var _24=parseInt(_22.cTaskItem[0].style.left)+parseInt(_22.cTaskItem[0].firstChild.firstChild.width);
if(!_1d){
_1d=_23;
}
if(!_1e){
_1e=_24;
}
if(_1d>_23){
_1d=_23;
}
if(_1e<_24){
_1e=_24;
}
}
if(_1d!=_1f){
this.project.startDate=new Date(this.ganttChart.startDate);
this.project.startDate.setHours(this.project.startDate.getHours()+(_1d/this.ganttChart.pixelsPerHour));
}
this.projectItem[0].style.left=_1d+"px";
this.resizeProjectItem(_1e-_1d);
this.duration=Math.round(parseInt(this.projectItem[0].firstChild.width)/(this.ganttChart.pixelsPerWorkHour));
this.shiftDescrProject();
this.adjustPanelTime();
},adjustPanelTime:function(){
var _25=this.projectItem[0];
var _26=parseInt(_25.style.left)+parseInt(_25.firstChild.style.width)+this.ganttChart.panelTimeExpandDelta;
_26+=this.descrProject.offsetWidth;
this.ganttChart.adjustPanelTime(_26);
},resizeProjectItem:function(_27){
var _28=this.percentage,_29=this.projectItem[0];
if(_28>0&&_28<100){
_29.firstChild.style.width=_27+"px";
_29.firstChild.width=_27+"px";
_29.style.width=_27+"px";
var _2a=_29.firstChild.rows[0];
_2a.cells[0].firstChild.style.width=Math.round(_27*_28/100)+"px";
_2a.cells[0].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_2a.cells[1].firstChild.style.width=Math.round(_27*(100-_28)/100)+"px";
_2a.cells[1].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_29.lastChild.firstChild.width=_27+"px";
}else{
if(_28==0||_28==100){
_29.firstChild.style.width=_27+"px";
_29.firstChild.width=_27+"px";
_29.style.width=_27+"px";
var _2a=_29.firstChild.rows[0];
_2a.cells[0].firstChild.style.width=_27+"px";
_2a.cells[0].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_29.lastChild.firstChild.width=_27+"px";
}
}
},shiftDescrProject:function(){
var _2b=(parseInt(this.projectItem[0].style.left)+this.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrProject.style.left=_2b+"px";
this.descrProject.innerHTML=this.getDescStr();
},showDescrProject:function(){
var _2c=(parseInt(this.projectItem[0].style.left)+this.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrProject.style.left=_2c+"px";
this.descrProject.style.visibility="visible";
this.descrProject.innerHTML=this.getDescStr();
},hideDescrProject:function(){
this.descrProject.style.visibility="hidden";
},getDescStr:function(){
return this.duration/this.ganttChart.hsPerDay+" days,  "+this.duration+" hours";
},createDescrProject:function(){
var _2d=(this.posX+this.duration*this.ganttChart.pixelsPerWorkHour+10);
var _2e=_2.create("div",{innerHTML:this.getDescStr(),className:"ganttDescProject"});
_2.style(_2e,{left:_2d+"px",top:this.posY+"px"});
this.descrProject=_2e;
if(this.project.parentTasks.length==0){
this.descrProject.style.visibility="hidden";
}
return _2e;
},createProjectItem:function(){
this.percentage=this.getPercentCompleted();
this.duration=this.getDuration();
var _2f=_2.create("div",{id:this.project.id,className:"ganttProjectItem"});
_2.style(_2f,{left:this.posX+"px",top:this.posY+"px",width:this.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _30=_2.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttTblProjectItem"},_2f);
var _31=this.duration*this.ganttChart.pixelsPerWorkHour;
_30.width=((_31==0)?1:_31)+"px";
_30.style.width=((_31==0)?1:_31)+"px";
var _32=_30.insertRow(_30.rows.length);
if(this.percentage!=-1){
if(this.percentage!=0){
var _33=_2.create("td",{width:this.percentage+"%"},_32);
_33.style.lineHeight="1px";
var _34=_2.create("div",{className:"ganttImageProgressFilled"},_33);
_2.style(_34,{width:(this.percentage*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.percentage!=100){
var _33=_2.create("td",{width:(100-this.percentage)+"%"},_32);
_33.style.lineHeight="1px";
var _34=_2.create("div",{className:"ganttImageProgressBg"},_33);
_2.style(_34,{width:((100-this.percentage)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}else{
var _33=_2.create("td",{width:"1px"},_32);
_33.style.lineHeight="1px";
var _34=_2.create("div",{className:"ganttImageProgressBg"},_33);
_2.style(_34,{width:"1px",height:this.ganttChart.heightTaskItem+"px"});
}
var _35=_2.create("div",{className:"ganttDivTaskInfo"});
var _36=_2.create("table",{cellPadding:"0",cellSpacing:"0",height:this.ganttChart.heightTaskItem+"px",width:((this.duration*this.ganttChart.pixelsPerWorkHour==0)?1:this.duration*this.ganttChart.pixelsPerWorkHour)+"px"},_35);
var _37=_36.insertRow(0);
var _38=_2.create("td",{align:"center",vAlign:"top",height:this.ganttChart.heightTaskItem+"px",className:"ganttMoveInfo"},_37);
_2f.appendChild(_35);
if(this.project.parentTasks.length==0){
_2f.style.display="none";
}
return _2f;
},createProjectNameItem:function(){
var _39=_2.create("div",{className:"ganttProjectNameItem",innerHTML:this.project.name,title:this.project.name});
_2.style(_39,{left:"5px",top:this.posY+"px"});
_2.attr(_39,"tabIndex",0);
if(this.ganttChart.isShowConMenu){
this.ganttChart._events.push(_2.connect(_39,"onmouseover",this,function(_3a){
_2.addClass(_39,"ganttProjectNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_3a.target,this);
}));
this.ganttChart._events.push(_2.connect(_39,"onkeydown",this,function(_3b){
if(_3b.keyCode==_2.keys.ENTER){
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_3b.target,this);
}
if(this.ganttChart.tabMenu.isShow&&(_3b.keyCode==_2.keys.LEFT_ARROW||_3b.keyCode==_2.keys.RIGHT_ARROW)){
_1.focus(this.ganttChart.tabMenu.menuPanel.firstChild.rows[0].cells[0]);
}
if(this.ganttChart.tabMenu.isShow&&_3b.keyCode==_2.keys.ESCAPE){
this.ganttChart.tabMenu.hide();
}
}));
this.ganttChart._events.push(_2.connect(_39,"onmouseout",this,function(){
_2.removeClass(_39,"ganttProjectNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_2.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
}));
this.ganttChart._events.push(_2.connect(this.ganttChart.tabMenu.menuPanel,"onmouseover",this,function(){
clearTimeout(this.ganttChart.menuTimer);
}));
this.ganttChart._events.push(_2.connect(this.ganttChart.tabMenu.menuPanel,"onkeydown",this,function(_3c){
if(this.ganttChart.tabMenu.isShow&&_3c.keyCode==_2.keys.ESCAPE){
this.ganttChart.tabMenu.hide();
}
}));
this.ganttChart._events.push(_2.connect(this.ganttChart.tabMenu.menuPanel,"onmouseout",this,function(){
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_2.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
}));
}
return _39;
},getPercentCompleted:function(){
var sum=0,_3d=0;
_2.forEach(this.project.parentTasks,function(_3e){
sum+=parseInt(_3e.percentage);
},this);
if(this.project.parentTasks.length!=0){
return _3d=Math.round(sum/this.project.parentTasks.length);
}else{
return _3d=-1;
}
},getDuration:function(){
var _3f=0,_40=0;
if(this.project.parentTasks.length>0){
_2.forEach(this.project.parentTasks,function(_41){
_40=_41.duration*24/this.ganttChart.hsPerDay+(_41.startTime-this.ganttChart.startDate)/(60*60*1000);
if(_40>_3f){
_3f=_40;
}
},this);
return ((_3f-this.posX)/24)*this.ganttChart.hsPerDay;
}else{
return 0;
}
},deleteTask:function(id){
var _42=this.getTaskById(id);
if(_42){
this.deleteChildTask(_42);
this.ganttChart.checkPosition();
}
},setName:function(_43){
if(_43){
this.project.name=_43;
this.projectNameItem.innerHTML=_43;
this.projectNameItem.title=_43;
this.checkWidthProjectNameItem();
this.descrProject.innerHTML=this.getDescStr();
this.adjustPanelTime();
}
},setPercentCompleted:function(_44){
_44=parseInt(_44);
if(isNaN(_44)||_44>100||_44<0){
return false;
}
var _45=this.projectItem[0].firstChild.rows[0],rc0=_45.cells[0],rc1=_45.cells[1];
if((_44>0)&&(_44<100)&&(this.percentage>0)&&(this.percentage<100)){
rc0.width=parseInt(_44)+"%";
rc0.firstChild.style.width=(_44*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px";
rc1.width=(100-parseInt(_44))+"%";
rc1.firstChild.style.width=((100-_44)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px";
}else{
if(((_44==0)||(_44==100))&&(this.percentage>0)&&(this.percentage<100)){
if(_44==0){
rc0.parentNode.removeChild(rc0);
rc1.width=100+"%";
rc1.firstChild.style.width=this.duration*this.ganttChart.pixelsPerWorkHour+"px";
}else{
if(_44==100){
rc1.parentNode.removeChild(rc1);
rc0.width=100+"%";
rc0.firstChild.style.width=this.duration*this.ganttChart.pixelsPerWorkHour+"px";
}
}
}else{
if(((_44==0)||(_44==100))&&((this.percentage==0)||(this.percentage==100))){
if((_44==0)&&(this.percentage==100)){
_2.removeClass(rc0.firstChild,"ganttImageProgressFilled");
_2.addClass(rc0.firstChild,"ganttImageProgressBg");
}else{
if((_44==100)&&(this.percentage==0)){
_2.removeClass(rc0.firstChild,"ganttImageProgressBg");
_2.addClass(rc0.firstChild,"ganttImageProgressFilled");
}
}
}else{
if(((_44>0)||(_44<100))&&((this.percentage==0)||(this.percentage==100))){
rc0.parentNode.removeChild(rc0);
var _46=_2.create("td",{width:_44+"%"},_45);
_46.style.lineHeight="1px";
var _47=_2.create("div",{className:"ganttImageProgressFilled"},_46);
_2.style(_47,{width:(_44*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_46=_2.create("td",{width:(100-_44)+"%"},_45);
_46.style.lineHeight="1px";
_47=_2.create("div",{className:"ganttImageProgressBg"},_46);
_2.style(_47,{width:((100-_44)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}else{
if(this.percentage==-1){
if(_44==100){
_2.removeClass(rc0.firstChild,"ganttImageProgressBg");
_2.addClass(rc0.firstChild,"ganttImageProgressFilled");
}else{
if(_44<100&&_44>0){
rc0.parentNode.removeChild(rc0);
var _46=_2.create("td",{width:_44+"%"},_45);
_46.style.lineHeight="1px";
_47=_2.create("div",{className:"ganttImageProgressFilled"},_46);
_2.style(_47,{width:(_44*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_46=_2.create("td",{width:(100-_44)+"%"},_45);
_46.style.lineHeight="1px";
_47=_2.create("div",{className:"ganttImageProgressBg"},_46);
_2.style(_47,{width:((100-_44)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}
}
}
}
}
}
this.percentage=_44;
this.descrProject.innerHTML=this.getDescStr();
return true;
},deleteChildTask:function(_48){
if(_48){
var _49=_48.cTaskItem[0],_4a=_48.cTaskNameItem[0],_4b=_48.cTaskItem[1],_4c=_48.cTaskNameItem[1],_4d=_48.cTaskItem[2],_4e=_48.cTaskNameItem[2];
if(_49.style.display=="none"){
this.ganttChart.openTree(_48.parentTask);
}
if(_48.childPredTask.length>0){
for(var i=0;i<_48.childPredTask.length;i++){
var _4f=_48.childPredTask[i];
for(var t=0;t<_4f.cTaskItem[1].length;t++){
_4f.cTaskItem[1][t].parentNode.removeChild(_4f.cTaskItem[1][t]);
}
_4f.cTaskItem[1]=[];
_4f.predTask=null;
}
}
if(_48.childTask.length>0){
while(_48.childTask.length>0){
this.deleteChildTask(_48.childTask[0]);
}
}
var _50=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
if(_49.style.display!="none"){
_48.shiftCurrentTasks(_48,-_50);
}
this.project.deleteTask(_48.taskItem.id);
if(_49){
_49.parentNode.removeChild(_49);
}
_48.descrTask.parentNode.removeChild(_48.descrTask);
if(_4b.length>0){
for(var j=0;j<_4b.length;j++){
_4b[j].parentNode.removeChild(_4b[j]);
}
}
if(_4a){
_4a.parentNode.removeChild(_4a);
}
if(_48.cTaskNameItem[1]){
for(var j=0;j<_4c.length;j++){
_4c[j].parentNode.removeChild(_4c[j]);
}
}
if(_4e&&_4e.parentNode){
_4e.parentNode.removeChild(_4e);
}
if(_48.taskIdentifier){
_48.taskIdentifier.parentNode.removeChild(_48.taskIdentifier);
_48.taskIdentifier=null;
}
if(_48.parentTask){
if(_48.previousChildTask){
if(_48.nextChildTask){
_48.previousChildTask.nextChildTask=_48.nextChildTask;
}else{
_48.previousChildTask.nextChildTask=null;
}
}
var _51=_48.parentTask;
for(var i=0;i<_51.childTask.length;i++){
if(_51.childTask[i].taskItem.id==_48.taskItem.id){
_51.childTask[i]=null;
_51.childTask.splice(i,1);
break;
}
}
if(_51.childTask.length==0){
if(_51.cTaskNameItem[2]){
_51.cTaskNameItem[2].parentNode.removeChild(_51.cTaskNameItem[2]);
_51.cTaskNameItem[2]=null;
}
}
}else{
if(_48.previousParentTask){
if(_48.nextParentTask){
_48.previousParentTask.nextParentTask=_48.nextParentTask;
}else{
_48.previousParentTask.nextParentTask=null;
}
}
var _52=_48.project;
for(var i=0;i<_52.arrTasks.length;i++){
if(_52.arrTasks[i].taskItem.id==_48.taskItem.id){
_52.arrTasks.splice(i,1);
}
}
}
if(_48.predTask){
var _53=_48.predTask;
for(var i=0;i<_53.childPredTask.length;i++){
if(_53.childPredTask[i].taskItem.id==_48.taskItem.id){
_53.childPredTask[i]=null;
_53.childPredTask.splice(i,1);
}
}
}
if(_48.project.arrTasks.length!=0){
_48.project.shiftProjectItem();
}else{
_48.project.projectItem[0].style.display="none";
this.hideDescrProject();
}
this.ganttChart.contentDataHeight-=this.ganttChart.heightTaskItemExtra+this.ganttChart.heightTaskItem;
}
},insertTask:function(id,_54,_55,_56,_57,_58,_59,_5a){
var _5b=null;
var _5c=null;
if(this.project.getTaskById(id)){
return false;
}
if((!_56)||(_56<this.ganttChart.minWorkLength)){
_56=this.ganttChart.minWorkLength;
}
if((!_54)||(_54=="")){
_54=id;
}
if((!_57)||(_57=="")){
_57=0;
}else{
_57=parseInt(_57);
if(_57<0||_57>100){
return false;
}
}
var _5d=false;
if((_5a)&&(_5a!="")){
var _5e=this.project.getTaskById(_5a);
if(!_5e){
return false;
}
_55=_55||_5e.startTime;
if(_55<_5e.startTime){
return false;
}
_5b=new _3.gantt.GanttTaskItem({id:id,name:_54,startTime:_55,duration:_56,percentage:_57,previousTaskId:_58,taskOwner:_59});
if(!this.ganttChart.checkPosParentTask(_5e,_5b)){
return false;
}
_5b.parentTask=_5e;
var _5f=this.getTaskById(_5e.id);
var _60=false;
if(_5f.cTaskItem[0].style.display=="none"){
_60=true;
}else{
if(_5f.cTaskNameItem[2]){
if(!_5f.isExpanded){
_60=true;
}
}
}
if(_60){
if(_5f.childTask.length==0){
this.ganttChart.openTree(_5f.parentTask);
}else{
this.ganttChart.openTree(_5f);
}
}
if(_58!=""){
var _61=this.project.getTaskById(_58);
if(!_61){
return false;
}
if(_61.parentTask){
if(_61.parentTask.id!=_5b.parentTask.id){
return false;
}
}else{
return false;
}
if(!this.ganttChart.checkPosPreviousTask(_61,_5b)){
this.ganttChart.correctPosPreviousTask(_61,_5b);
}
_5b.previousTask=_61;
}
var _62=false;
if(_5d){
for(var i=0;i<_5e.cldTasks.length;i++){
if(_5b.startTime<_5e.cldTasks[i].startTime){
_5e.cldTasks.splice(i,0,_5b);
if(i>0){
_5e.cldTasks[i-1].nextChildTask=_5e.cldTasks[i];
_5e.cldTasks[i].previousChildTask=_5e.cldTasks[i-1];
}
if(_5e.cldTasks[i+1]){
_5e.cldTasks[i+1].previousChildTask=_5e.cldTasks[i];
_5e.cldTasks[i].nextChildTask=_5e.cldTasks[i+1];
}
_62=true;
break;
}
}
}
if(!_62){
if(_5e.cldTasks.length>0){
_5e.cldTasks[_5e.cldTasks.length-1].nextChildTask=_5b;
_5b.previousChildTask=_5e.cldTasks[_5e.cldTasks.length-1];
}
_5e.cldTasks.push(_5b);
}
if(_5e.cldTasks.length==1){
var _63=_5f.createTreeImg();
_5f.cTaskNameItem[2]=_63;
}
_5c=new _3.gantt.GanttTaskControl(_5b,this,this.ganttChart);
_5c.create();
if(_5b.nextChildTask){
_5c.nextChildTask=_5c.project.getTaskById(_5b.nextChildTask.id);
}
_5c.adjustPanelTime();
var _64=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
_5c.shiftCurrentTasks(_5c,_64);
}else{
_55=_55||this.project.startDate;
_5b=new _3.gantt.GanttTaskItem({id:id,name:_54,startTime:_55,duration:_56,percentage:_57,previousTaskId:_58,taskOwner:_59});
if(_5b.startTime<=this.ganttChart.startDate){
return false;
}
if(_58!=""){
var _61=this.project.getTaskById(_58);
if(!_61){
return false;
}
if(!this.ganttChart.checkPosPreviousTask(_61,_5b)){
this.ganttChart.correctPosPreviousTask(_61,_5b);
}
if(_61.parentTask){
return false;
}
_5b.previousTask=_61;
}
var _62=false;
if(_5d){
for(var i=0;i<this.project.parentTasks.length;i++){
var _65=this.project.parentTasks[i];
if(_55<_65.startTime){
this.project.parentTasks.splice(i,0,_5b);
if(i>0){
this.project.parentTasks[i-1].nextParentTask=_5b;
_5b.previousParentTask=this.project.parentTasks[i-1];
}
if(this.project.parentTasks[i+1]){
this.project.parentTasks[i+1].previousParentTask=_5b;
_5b.nextParentTask=this.project.parentTasks[i+1];
}
_62=true;
break;
}
}
}
if(!_62){
if(this.project.parentTasks.length>0){
this.project.parentTasks[this.project.parentTasks.length-1].nextParentTask=_5b;
_5b.previousParentTask=this.project.parentTasks[this.project.parentTasks.length-1];
}
this.project.parentTasks.push(_5b);
}
_5c=new _3.gantt.GanttTaskControl(_5b,this,this.ganttChart);
_5c.create();
if(_5b.nextParentTask){
_5c.nextParentTask=_5c.project.getTaskById(_5b.nextParentTask.id);
}
_5c.adjustPanelTime();
this.arrTasks.push(_5c);
var _64=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
_5c.shiftCurrentTasks(_5c,_64);
this.projectItem[0].style.display="inline";
this.setPercentCompleted(this.getPercentCompleted());
this.shiftProjectItem();
this.showDescrProject();
}
this.ganttChart.checkHeighPanelTasks();
this.ganttChart.checkPosition();
return _5c;
},shiftNextProject:function(_66,_67){
if(_66.nextProject){
_66.nextProject.shiftProject(_67);
this.shiftNextProject(_66.nextProject,_67);
}
},shiftProject:function(_68){
this.posY=this.posY+_68;
this.projectItem[0].style.top=parseInt(this.projectItem[0].style.top)+_68+"px";
this.descrProject.style.top=parseInt(this.descrProject.style.top)+_68+"px";
this.projectNameItem.style.top=parseInt(this.projectNameItem.style.top)+_68+"px";
if(this.arrTasks.length>0){
this.shiftNextParentTask(this.arrTasks[0],_68);
}
},shiftTask:function(_69,_6a){
_69.posY=_69.posY+_6a;
var _6b=_69.cTaskNameItem[0],_6c=_69.cTaskNameItem[1],_6d=_69.cTaskNameItem[2],_6e=_69.cTaskItem[0],_6f=_69.cTaskItem[1],_70=_69.cTaskItem[2];
_6b.style.top=parseInt(_6b.style.top)+_6a+"px";
if(_6d){
_6d.style.top=parseInt(_6d.style.top)+_6a+"px";
}
if(_69.parentTask){
_6c[0].style.top=parseInt(_6c[0].style.top)+_6a+"px";
_6c[1].style.top=parseInt(_6c[1].style.top)+_6a+"px";
}
_69.cTaskItem[0].style.top=parseInt(_69.cTaskItem[0].style.top)+_6a+"px";
_69.descrTask.style.top=parseInt(_69.descrTask.style.top)+_6a+"px";
if(_6f[0]){
_6f[0].style.top=parseInt(_6f[0].style.top)+_6a+"px";
_6f[1].style.top=parseInt(_6f[1].style.top)+_6a+"px";
_6f[2].style.top=parseInt(_6f[2].style.top)+_6a+"px";
}
},shiftNextParentTask:function(_71,_72){
this.shiftTask(_71,_72);
this.shiftChildTasks(_71,_72);
if(_71.nextParentTask){
this.shiftNextParentTask(_71.nextParentTask,_72);
}
},shiftChildTasks:function(_73,_74){
_2.forEach(_73.childTask,function(_75){
this.shiftTask(_75,_74);
if(_75.childTask.length>0){
this.shiftChildTasks(_75,_74);
}
},this);
}});
_2.declare("dojox.gantt.GanttProjectItem",null,{constructor:function(_76){
this.id=_76.id;
this.name=_76.name||this.id;
this.startDate=_76.startDate||new Date();
this.parentTasks=[];
},getTaskById:function(id){
for(var i=0;i<this.parentTasks.length;i++){
var _77=this.parentTasks[i];
var _78=this.getTaskByIdInTree(_77,id);
if(_78){
return _78;
}
}
return null;
},getTaskByIdInTree:function(_79,id){
if(_79.id==id){
return _79;
}else{
for(var i=0;i<_79.cldTasks.length;i++){
var _7a=_79.cldTasks[i];
if(_7a.id==id){
return _7a;
}
if(_7a.cldTasks.length>0){
if(_7a.cldTasks.length>0){
var _7b=this.getTaskByIdInTree(_7a,id);
if(_7b){
return _7b;
}
}
}
}
}
return null;
},addTask:function(_7c){
this.parentTasks.push(_7c);
_7c.setProject(this);
},deleteTask:function(id){
var _7d=this.getTaskById(id);
if(!_7d){
return;
}
if(!_7d.parentTask){
for(var i=0;i<this.parentTasks.length;i++){
var _7e=this.parentTasks[i];
if(_7e.id==id){
if(_7e.nextParentTask){
if(_7e.previousParentTask){
_7e.previousParentTask.nextParentTask=_7e.nextParentTask;
_7e.nextParentTask.previousParentTask=_7e.previousParentTask;
}else{
_7e.nextParentTask.previousParentTask=null;
}
}else{
if(_7e.previousParentTask){
_7e.previousParentTask.nextParentTask=null;
}
}
_7e=null;
this.parentTasks.splice(i,1);
break;
}
}
}else{
var _7f=_7d.parentTask;
for(var i=0;i<_7f.cldTasks.length;i++){
var _80=_7f.cldTasks[i];
if(_80.id==id){
if(_80.nextChildTask){
if(_80.previousChildTask){
_80.previousChildTask.nextChildTask=_80.nextChildTask;
_80.nextChildTask.previousChildTask=_80.previousChildTask;
}else{
_80.nextChildTask.previousChildTask=null;
}
}else{
if(_80.previousChildTask){
_80.previousChildTask.nextChildTask=null;
}
}
_80=null;
_7f.cldTasks.splice(i,1);
break;
}
}
}
}});
});
