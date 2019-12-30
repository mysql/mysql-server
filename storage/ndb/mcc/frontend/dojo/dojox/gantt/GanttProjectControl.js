//>>built
define("dojox/gantt/GanttProjectControl",["./GanttTaskItem","./GanttTaskControl","dijit/focus","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/date/locale","dojo/request","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dojo/keys","dojo/domReady!"],function(_1,_2,_3,_4,_5,_6,_7,_8,on,_9,_a,_b,_c,_d,_e,_f){
return _4("dojox.gantt.GanttProjectControl",[],{constructor:function(_10,_11){
this.project=_11;
this.ganttChart=_10;
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
var _12=this.projectNameItem.offsetWidth+this.projectNameItem.offsetLeft-this.ganttChart.maxWidthTaskNames;
var _13=Math.round(_12/(this.projectNameItem.offsetWidth/this.projectNameItem.firstChild.length));
var _14=this.project.name.substring(0,this.projectNameItem.firstChild.length-_13-3);
_14+="...";
this.projectNameItem.innerHTML=_14;
}
},refreshProjectItem:function(_15){
this.percentage=this.getPercentCompleted();
_c.set(_15,{"left":this.posX+"px","width":this.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _16=_15.firstChild;
var _17=this.duration*this.ganttChart.pixelsPerWorkHour;
_16.width=((_17==0)?1:_17)+"px";
_16.style.width=((_17==0)?1:_17)+"px";
var _18=_16.rows[0];
if(this.percentage!=-1){
if(this.percentage!=0){
var _19=_18.firstChild;
_19.width=this.percentage+"%";
var _1a=_19.firstChild;
_c.set(_1a,{width:(!this.duration?1:(this.percentage*this.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.percentage!=100){
var _19=_18.lastChild;
_19.width=(100-this.percentage)+"%";
var _1a=_19.firstChild;
_c.set(_1a,{width:(!this.duration?1:((100-this.percentage)*this.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}else{
var _19=_18.firstChild;
_19.width="1px";
var _1a=_19.firstChild;
_c.set(_1a,{width:"1px",height:this.ganttChart.heightTaskItem+"px"});
}
var _1b=_15.lastChild;
var _1c=_1b.firstChild;
_c.set(_1c,{height:this.ganttChart.heightTaskItem+"px",width:(!this.duration?1:(this.duration*this.ganttChart.pixelsPerWorkHour))+"px"});
var _1d=_1c.rows[0];
var _1e=_1d.firstChild;
_1e.height=this.ganttChart.heightTaskItem+"px";
if(this.project.parentTasks.length==0){
_15.style.display="none";
}
return _15;
},refreshDescrProject:function(_1f){
var _20=(this.posX+this.duration*this.ganttChart.pixelsPerWorkHour+10);
_c.set(_1f,{"left":_20+"px"});
if(this.project.parentTasks.length==0){
this.descrProject.style.visibility="hidden";
}
return _1f;
},postLoadData:function(){
},refresh:function(){
this.posX=(this.project.startDate-this.ganttChart.startDate)/(60*60*1000)*this.ganttChart.pixelsPerHour;
this.refreshProjectItem(this.projectItem[0]);
this.refreshDescrProject(this.projectItem[0].nextSibling);
return this;
},create:function(){
var _21=this.ganttChart.contentData.firstChild;
this.posX=(this.project.startDate-this.ganttChart.startDate)/(60*60*1000)*this.ganttChart.pixelsPerHour;
if(this.previousProject){
if(this.previousProject.arrTasks.length>0){
var _22=this.ganttChart.getLastChildTask(this.previousProject.arrTasks[this.previousProject.arrTasks.length-1]);
this.posY=parseInt(_22.cTaskItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}else{
this.posY=parseInt(this.previousProject.projectItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}
}else{
this.posY=6;
}
var _23=this.ganttChart.panelNames.firstChild;
this.projectNameItem=this.createProjectNameItem();
_23.appendChild(this.projectNameItem);
this.checkWidthProjectNameItem();
this.projectItem=[this.createProjectItem(),[]];
_21.appendChild(this.projectItem[0]);
_21.appendChild(this.createDescrProject());
this.adjustPanelTime();
},getTaskById:function(id){
for(var i=0;i<this.arrTasks.length;i++){
var _24=this.arrTasks[i];
var _25=this.searchTaskInTree(_24,id);
if(_25){
return _25;
}
}
return null;
},searchTaskInTree:function(_26,id){
if(_26.taskItem.id==id){
return _26;
}else{
for(var i=0;i<_26.childTask.length;i++){
var _27=_26.childTask[i];
if(_27.taskItem.id==id){
return _27;
}else{
if(_27.childTask.length>0){
var _27=this.searchTaskInTree(_27,id);
if(_27){
return _27;
}
}
}
}
}
return null;
},shiftProjectItem:function(){
var _28=null;
var _29=null;
var _2a=parseInt(this.projectItem[0].style.left);
for(var i=0;i<this.arrTasks.length;i++){
var _2b=this.arrTasks[i];
var _2c=parseInt(_2b.cTaskItem[0].style.left);
var _2d=parseInt(_2b.cTaskItem[0].style.left)+parseInt(_2b.cTaskItem[0].firstChild.firstChild.width);
if(!_28){
_28=_2c;
}
if(!_29){
_29=_2d;
}
if(_28>_2c){
_28=_2c;
}
if(_29<_2d){
_29=_2d;
}
}
if(_28!=_2a){
this.project.startDate=new Date(this.ganttChart.startDate);
this.project.startDate.setHours(this.project.startDate.getHours()+(_28/this.ganttChart.pixelsPerHour));
}
this.projectItem[0].style.left=_28+"px";
this.resizeProjectItem(_29-_28);
this.duration=Math.round(parseInt(this.projectItem[0].firstChild.width)/(this.ganttChart.pixelsPerWorkHour));
this.shiftDescrProject();
this.adjustPanelTime();
},adjustPanelTime:function(){
var _2e=this.projectItem[0];
var _2f=parseInt(_2e.style.left)+parseInt(_2e.firstChild.style.width)+this.ganttChart.panelTimeExpandDelta;
_2f+=this.descrProject.offsetWidth;
this.ganttChart.adjustPanelTime(_2f);
},resizeProjectItem:function(_30){
var _31=this.percentage,_32=this.projectItem[0];
if(_31>0&&_31<100){
_32.firstChild.style.width=_30+"px";
_32.firstChild.width=_30+"px";
_32.style.width=_30+"px";
var _33=_32.firstChild.rows[0];
_33.cells[0].firstChild.style.width=Math.round(_30*_31/100)+"px";
_33.cells[0].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_33.cells[1].firstChild.style.width=Math.round(_30*(100-_31)/100)+"px";
_33.cells[1].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_32.lastChild.firstChild.width=_30+"px";
}else{
if(_31==0||_31==100){
_32.firstChild.style.width=_30+"px";
_32.firstChild.width=_30+"px";
_32.style.width=_30+"px";
var _33=_32.firstChild.rows[0];
_33.cells[0].firstChild.style.width=_30+"px";
_33.cells[0].firstChild.style.height=this.ganttChart.heightTaskItem+"px";
_32.lastChild.firstChild.width=_30+"px";
}
}
},shiftDescrProject:function(){
var _34=(parseInt(this.projectItem[0].style.left)+this.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrProject.style.left=_34+"px";
this.descrProject.innerHTML=this.getDescStr();
},showDescrProject:function(){
var _35=(parseInt(this.projectItem[0].style.left)+this.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrProject.style.left=_35+"px";
this.descrProject.style.visibility="visible";
this.descrProject.innerHTML=this.getDescStr();
},hideDescrProject:function(){
this.descrProject.style.visibility="hidden";
},getDescStr:function(){
return this.duration/this.ganttChart.hsPerDay+" days,  "+this.duration+" hours";
},createDescrProject:function(){
var _36=(this.posX+this.duration*this.ganttChart.pixelsPerWorkHour+10);
var _37=_b.create("div",{innerHTML:this.getDescStr(),className:"ganttDescProject"});
_c.set(_37,{left:_36+"px",top:this.posY+"px"});
this.descrProject=_37;
if(this.project.parentTasks.length==0){
this.descrProject.style.visibility="hidden";
}
return _37;
},createProjectItem:function(){
this.percentage=this.getPercentCompleted();
this.duration=this.getDuration();
var _38=_b.create("div",{id:this.project.id,className:"ganttProjectItem"});
_c.set(_38,{left:this.posX+"px",top:this.posY+"px",width:this.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _39=_b.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttTblProjectItem"},_38);
var _3a=this.duration*this.ganttChart.pixelsPerWorkHour;
_39.width=((_3a==0)?1:_3a)+"px";
_39.style.width=((_3a==0)?1:_3a)+"px";
var _3b=_39.insertRow(_39.rows.length);
if(this.percentage!=-1){
if(this.percentage!=0){
var _3c=_b.create("td",{width:this.percentage+"%"},_3b);
_3c.style.lineHeight="1px";
var _3d=_b.create("div",{className:"ganttImageProgressFilled"},_3c);
_c.set(_3d,{width:(this.percentage*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.percentage!=100){
var _3c=_b.create("td",{width:(100-this.percentage)+"%"},_3b);
_3c.style.lineHeight="1px";
var _3d=_b.create("div",{className:"ganttImageProgressBg"},_3c);
_c.set(_3d,{width:((100-this.percentage)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}else{
var _3c=_b.create("td",{width:"1px"},_3b);
_3c.style.lineHeight="1px";
var _3d=_b.create("div",{className:"ganttImageProgressBg"},_3c);
_c.set(_3d,{width:"1px",height:this.ganttChart.heightTaskItem+"px"});
}
var _3e=_b.create("div",{className:"ganttDivTaskInfo"});
var _3f=_b.create("table",{cellPadding:"0",cellSpacing:"0",height:this.ganttChart.heightTaskItem+"px",width:((this.duration*this.ganttChart.pixelsPerWorkHour==0)?1:this.duration*this.ganttChart.pixelsPerWorkHour)+"px"},_3e);
var _40=_3f.insertRow(0);
_b.create("td",{align:"center",vAlign:"top",height:this.ganttChart.heightTaskItem+"px",className:"ganttMoveInfo"},_40);
_38.appendChild(_3e);
if(this.project.parentTasks.length==0){
_38.style.display="none";
}
return _38;
},createProjectNameItem:function(){
var _41=_b.create("div",{className:"ganttProjectNameItem",innerHTML:this.project.name,title:this.project.name});
_c.set(_41,{left:"5px",top:this.posY+"px"});
_d.set(_41,"tabIndex",0);
if(this.ganttChart.isShowConMenu){
this.ganttChart._events.push(on(_41,"mouseover",_6.hitch(this,function(_42){
_a.add(_41,"ganttProjectNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_42.target,this);
})));
this.ganttChart._events.push(on(_41,"keydown",_6.hitch(this,function(_43){
if(_43.keyCode==_f.ENTER){
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_43.target,this);
}
if(this.ganttChart.tabMenu.isShow&&(_43.keyCode==_f.LEFT_ARROW||_43.keyCode==_f.RIGHT_ARROW)){
_3(this.ganttChart.tabMenu.menuPanel.firstChild.rows[0].cells[0]);
}
if(this.ganttChart.tabMenu.isShow&&_43.keyCode==_f.ESCAPE){
this.ganttChart.tabMenu.hide();
}
})));
this.ganttChart._events.push(on(_41,"mouseout",_6.hitch(this,function(){
_a.remove(_41,"ganttProjectNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_6.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"mouseover",_6.hitch(this,function(){
clearTimeout(this.ganttChart.menuTimer);
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"keydown",_6.hitch(this,function(){
if(this.ganttChart.tabMenu.isShow&&event.keyCode==_f.ESCAPE){
this.ganttChart.tabMenu.hide();
}
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"mouseout",_6.hitch(this,function(){
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_6.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
})));
}
return _41;
},getPercentCompleted:function(){
var sum=0;
_5.forEach(this.project.parentTasks,function(_44){
sum+=parseInt(_44.percentage);
},this);
if(this.project.parentTasks.length!=0){
return Math.round(sum/this.project.parentTasks.length);
}else{
return -1;
}
},getDuration:function(){
var _45=0,_46=0;
if(this.project.parentTasks.length>0){
_5.forEach(this.project.parentTasks,function(_47){
_46=_47.duration*24/this.ganttChart.hsPerDay+(_47.startTime-this.ganttChart.startDate)/(60*60*1000);
if(_46>_45){
_45=_46;
}
},this);
return ((_45-this.posX)/24)*this.ganttChart.hsPerDay;
}else{
return 0;
}
},deleteTask:function(id){
var _48=this.getTaskById(id);
if(_48){
this.deleteChildTask(_48);
this.ganttChart.checkPosition();
}
},setName:function(_49){
if(_49){
this.project.name=_49;
this.projectNameItem.innerHTML=_49;
this.projectNameItem.title=_49;
this.checkWidthProjectNameItem();
this.descrProject.innerHTML=this.getDescStr();
this.adjustPanelTime();
}
},setPercentCompleted:function(_4a){
_4a=parseInt(_4a);
if(isNaN(_4a)||_4a>100||_4a<0){
return false;
}
var _4b=this.projectItem[0].firstChild.rows[0],rc0=_4b.cells[0],rc1=_4b.cells[1];
if((_4a>0)&&(_4a<100)&&(this.percentage>0)&&(this.percentage<100)){
rc0.width=parseInt(_4a)+"%";
rc0.firstChild.style.width=(_4a*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px";
rc1.width=(100-parseInt(_4a))+"%";
rc1.firstChild.style.width=((100-_4a)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px";
}else{
if(((_4a==0)||(_4a==100))&&(this.percentage>0)&&(this.percentage<100)){
if(_4a==0){
rc0.parentNode.removeChild(rc0);
rc1.width=100+"%";
rc1.firstChild.style.width=this.duration*this.ganttChart.pixelsPerWorkHour+"px";
}else{
if(_4a==100){
rc1.parentNode.removeChild(rc1);
rc0.width=100+"%";
rc0.firstChild.style.width=this.duration*this.ganttChart.pixelsPerWorkHour+"px";
}
}
}else{
if(((_4a==0)||(_4a==100))&&((this.percentage==0)||(this.percentage==100))){
if((_4a==0)&&(this.percentage==100)){
_a.remove(rc0.firstChild,"ganttImageProgressFilled");
_a.add(rc0.firstChild,"ganttImageProgressBg");
}else{
if((_4a==100)&&(this.percentage==0)){
_a.remove(rc0.firstChild,"ganttImageProgressBg");
_a.add(rc0.firstChild,"ganttImageProgressFilled");
}
}
}else{
if(((_4a>0)||(_4a<100))&&((this.percentage==0)||(this.percentage==100))){
rc0.parentNode.removeChild(rc0);
var _4c=_b.create("td",{width:_4a+"%"},_4b);
_4c.style.lineHeight="1px";
var _4d=_b.create("div",{className:"ganttImageProgressFilled"},_4c);
_c.set(_4d,{width:(_4a*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_4c=_b.create("td",{width:(100-_4a)+"%"},_4b);
_4c.style.lineHeight="1px";
_4d=_b.create("div",{className:"ganttImageProgressBg"},_4c);
_c.set(_4d,{width:((100-_4a)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}else{
if(this.percentage==-1){
if(_4a==100){
_a.remove(rc0.firstChild,"ganttImageProgressBg");
_a.add(rc0.firstChild,"ganttImageProgressFilled");
}else{
if(_4a<100&&_4a>0){
rc0.parentNode.removeChild(rc0);
var _4c=_b.create("td",{width:_4a+"%"},_4b);
_4c.style.lineHeight="1px";
_4d=_b.create("div",{className:"ganttImageProgressFilled"},_4c);
_c.set(_4d,{width:(_4a*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_4c=_b.create("td",{width:(100-_4a)+"%"},_4b);
_4c.style.lineHeight="1px";
_4d=_b.create("div",{className:"ganttImageProgressBg"},_4c);
_c.set(_4d,{width:((100-_4a)*this.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}
}
}
}
}
}
this.percentage=_4a;
this.descrProject.innerHTML=this.getDescStr();
return true;
},deleteChildTask:function(_4e){
if(_4e){
var _4f=_4e.cTaskItem[0],_50=_4e.cTaskNameItem[0],_51=_4e.cTaskItem[1],_52=_4e.cTaskNameItem[1],_53=_4e.cTaskNameItem[2];
if(_4f.style.display=="none"){
this.ganttChart.openTree(_4e.parentTask);
}
if(_4e.childPredTask.length>0){
for(var i=0;i<_4e.childPredTask.length;i++){
var _54=_4e.childPredTask[i];
for(var t=0;t<_54.cTaskItem[1].length;t++){
_54.cTaskItem[1][t].parentNode.removeChild(_54.cTaskItem[1][t]);
}
_54.cTaskItem[1]=[];
_54.predTask=null;
}
}
if(_4e.childTask.length>0){
while(_4e.childTask.length>0){
this.deleteChildTask(_4e.childTask[0]);
}
}
var _55=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
if(_4f.style.display!="none"){
_4e.shiftCurrentTasks(_4e,-_55);
}
this.project.deleteTask(_4e.taskItem.id);
if(_4f){
_4f.parentNode.removeChild(_4f);
}
_4e.descrTask.parentNode.removeChild(_4e.descrTask);
if(_51.length>0){
for(var j=0;j<_51.length;j++){
_51[j].parentNode.removeChild(_51[j]);
}
}
if(_50){
_50.parentNode.removeChild(_50);
}
if(_4e.cTaskNameItem[1]){
for(var j=0;j<_52.length;j++){
_52[j].parentNode.removeChild(_52[j]);
}
}
if(_53&&_53.parentNode){
_53.parentNode.removeChild(_53);
}
if(_4e.taskIdentifier){
_4e.taskIdentifier.parentNode.removeChild(_4e.taskIdentifier);
_4e.taskIdentifier=null;
}
if(_4e.parentTask){
if(_4e.previousChildTask){
if(_4e.nextChildTask){
_4e.previousChildTask.nextChildTask=_4e.nextChildTask;
}else{
_4e.previousChildTask.nextChildTask=null;
}
}
var _56=_4e.parentTask;
for(var i=0;i<_56.childTask.length;i++){
if(_56.childTask[i].taskItem.id==_4e.taskItem.id){
_56.childTask[i]=null;
_56.childTask.splice(i,1);
break;
}
}
if(_56.childTask.length==0){
if(_56.cTaskNameItem[2]){
_56.cTaskNameItem[2].parentNode.removeChild(_56.cTaskNameItem[2]);
_56.cTaskNameItem[2]=null;
}
}
}else{
if(_4e.previousParentTask){
if(_4e.nextParentTask){
_4e.previousParentTask.nextParentTask=_4e.nextParentTask;
}else{
_4e.previousParentTask.nextParentTask=null;
}
}
var _57=_4e.project;
for(var i=0;i<_57.arrTasks.length;i++){
if(_57.arrTasks[i].taskItem.id==_4e.taskItem.id){
_57.arrTasks.splice(i,1);
}
}
}
if(_4e.predTask){
var _58=_4e.predTask;
for(var i=0;i<_58.childPredTask.length;i++){
if(_58.childPredTask[i].taskItem.id==_4e.taskItem.id){
_58.childPredTask[i]=null;
_58.childPredTask.splice(i,1);
}
}
}
if(_4e.project.arrTasks.length!=0){
_4e.project.shiftProjectItem();
}else{
_4e.project.projectItem[0].style.display="none";
this.hideDescrProject();
}
this.ganttChart.contentDataHeight-=this.ganttChart.heightTaskItemExtra+this.ganttChart.heightTaskItem;
}
},insertTask:function(id,_59,_5a,_5b,_5c,_5d,_5e,_5f){
var _60=null;
var _61=null;
if(this.project.getTaskById(id)){
return false;
}
if((!_5b)||(_5b<this.ganttChart.minWorkLength)){
_5b=this.ganttChart.minWorkLength;
}
if((!_59)||(_59=="")){
_59=id;
}
if((!_5c)||(_5c=="")){
_5c=0;
}else{
_5c=parseInt(_5c);
if(_5c<0||_5c>100){
return false;
}
}
var _62=false;
if((_5f)&&(_5f!="")){
var _63=this.project.getTaskById(_5f);
if(!_63){
return false;
}
_5a=_5a||_63.startTime;
if(_5a<_63.startTime){
return false;
}
_60=new _1({id:id,name:_59,startTime:_5a,duration:_5b,percentage:_5c,previousTaskId:_5d,taskOwner:_5e});
if(!this.ganttChart.checkPosParentTask(_63,_60)){
return false;
}
_60.parentTask=_63;
var _64=this.getTaskById(_63.id);
var _65=false;
if(_64.cTaskItem[0].style.display=="none"){
_65=true;
}else{
if(_64.cTaskNameItem[2]){
if(!_64.isExpanded){
_65=true;
}
}
}
if(_65){
if(_64.childTask.length==0){
this.ganttChart.openTree(_64.parentTask);
}else{
this.ganttChart.openTree(_64);
}
}
if(_5d!=""){
var _66=this.project.getTaskById(_5d);
if(!_66){
return false;
}
if(_66.parentTask){
if(_66.parentTask.id!=_60.parentTask.id){
return false;
}
}else{
return false;
}
if(!this.ganttChart.checkPosPreviousTask(_66,_60)){
this.ganttChart.correctPosPreviousTask(_66,_60);
}
_60.previousTask=_66;
}
var _67=false;
if(_62){
for(var i=0;i<_63.cldTasks.length;i++){
if(_60.startTime<_63.cldTasks[i].startTime){
_63.cldTasks.splice(i,0,_60);
if(i>0){
_63.cldTasks[i-1].nextChildTask=_63.cldTasks[i];
_63.cldTasks[i].previousChildTask=_63.cldTasks[i-1];
}
if(_63.cldTasks[i+1]){
_63.cldTasks[i+1].previousChildTask=_63.cldTasks[i];
_63.cldTasks[i].nextChildTask=_63.cldTasks[i+1];
}
_67=true;
break;
}
}
}
if(!_67){
if(_63.cldTasks.length>0){
_63.cldTasks[_63.cldTasks.length-1].nextChildTask=_60;
_60.previousChildTask=_63.cldTasks[_63.cldTasks.length-1];
}
_63.cldTasks.push(_60);
}
if(_63.cldTasks.length==1){
_64.cTaskNameItem[2]=_64.createTreeImg();
}
_61=new _2(_60,this,this.ganttChart);
_61.create();
if(_60.nextChildTask){
_61.nextChildTask=_61.project.getTaskById(_60.nextChildTask.id);
}
_61.adjustPanelTime();
var _68=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
_61.shiftCurrentTasks(_61,_68);
}else{
_5a=_5a||this.project.startDate;
_60=new _1({id:id,name:_59,startTime:_5a,duration:_5b,percentage:_5c,previousTaskId:_5d,taskOwner:_5e});
if(_60.startTime<=this.ganttChart.startDate){
return false;
}
if(_5d!=""){
var _66=this.project.getTaskById(_5d);
if(!_66){
return false;
}
if(!this.ganttChart.checkPosPreviousTask(_66,_60)){
this.ganttChart.correctPosPreviousTask(_66,_60);
}
if(_66.parentTask){
return false;
}
_60.previousTask=_66;
}
var _67=false;
if(_62){
for(var i=0;i<this.project.parentTasks.length;i++){
var _69=this.project.parentTasks[i];
if(_5a<_69.startTime){
this.project.parentTasks.splice(i,0,_60);
if(i>0){
this.project.parentTasks[i-1].nextParentTask=_60;
_60.previousParentTask=this.project.parentTasks[i-1];
}
if(this.project.parentTasks[i+1]){
this.project.parentTasks[i+1].previousParentTask=_60;
_60.nextParentTask=this.project.parentTasks[i+1];
}
_67=true;
break;
}
}
}
if(!_67){
if(this.project.parentTasks.length>0){
this.project.parentTasks[this.project.parentTasks.length-1].nextParentTask=_60;
_60.previousParentTask=this.project.parentTasks[this.project.parentTasks.length-1];
}
this.project.parentTasks.push(_60);
}
_61=new _2(_60,this,this.ganttChart);
_61.create();
if(_60.nextParentTask){
_61.nextParentTask=_61.project.getTaskById(_60.nextParentTask.id);
}
_61.adjustPanelTime();
this.arrTasks.push(_61);
var _68=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
_61.shiftCurrentTasks(_61,_68);
this.projectItem[0].style.display="inline";
this.setPercentCompleted(this.getPercentCompleted());
this.shiftProjectItem();
this.showDescrProject();
}
this.ganttChart.checkHeighPanelTasks();
this.ganttChart.checkPosition();
return _61;
},shiftNextProject:function(_6a,_6b){
if(_6a.nextProject){
_6a.nextProject.shiftProject(_6b);
this.shiftNextProject(_6a.nextProject,_6b);
}
},shiftProject:function(_6c){
this.posY=this.posY+_6c;
this.projectItem[0].style.top=parseInt(this.projectItem[0].style.top)+_6c+"px";
this.descrProject.style.top=parseInt(this.descrProject.style.top)+_6c+"px";
this.projectNameItem.style.top=parseInt(this.projectNameItem.style.top)+_6c+"px";
if(this.arrTasks.length>0){
this.shiftNextParentTask(this.arrTasks[0],_6c);
}
},shiftTask:function(_6d,_6e){
_6d.posY=_6d.posY+_6e;
var _6f=_6d.cTaskNameItem[0],_70=_6d.cTaskNameItem[1],_71=_6d.cTaskNameItem[2],_72=_6d.cTaskItem[1];
_6f.style.top=parseInt(_6f.style.top)+_6e+"px";
if(_71){
_71.style.top=parseInt(_71.style.top)+_6e+"px";
}
if(_6d.parentTask){
_70[0].style.top=parseInt(_70[0].style.top)+_6e+"px";
_70[1].style.top=parseInt(_70[1].style.top)+_6e+"px";
}
_6d.cTaskItem[0].style.top=parseInt(_6d.cTaskItem[0].style.top)+_6e+"px";
_6d.descrTask.style.top=parseInt(_6d.descrTask.style.top)+_6e+"px";
if(_72[0]){
_72[0].style.top=parseInt(_72[0].style.top)+_6e+"px";
_72[1].style.top=parseInt(_72[1].style.top)+_6e+"px";
_72[2].style.top=parseInt(_72[2].style.top)+_6e+"px";
}
},shiftNextParentTask:function(_73,_74){
this.shiftTask(_73,_74);
this.shiftChildTasks(_73,_74);
if(_73.nextParentTask){
this.shiftNextParentTask(_73.nextParentTask,_74);
}
},shiftChildTasks:function(_75,_76){
_5.forEach(_75.childTask,function(_77){
this.shiftTask(_77,_76);
if(_77.childTask.length>0){
this.shiftChildTasks(_77,_76);
}
},this);
}});
});
