//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/date/locale,dijit/focus"],function(_1,_2,_3){
_2.provide("dojox.gantt.GanttTaskItem");
_2.require("dojo.date.locale");
_2.require("dijit.focus");
_2.declare("dojox.gantt.GanttTaskControl",null,{constructor:function(_4,_5,_6){
this.ganttChart=_6;
this.project=_5;
this.taskItem=_4;
this.checkMove=false;
this.checkResize=false;
this.moveChild=false;
this.maxPosXMove=-1;
this.minPosXMove=-1;
this.maxWidthResize=-1;
this.minWidthResize=-1;
this.posX=0;
this.posY=0;
this.mouseX=0;
this.taskItemWidth=0;
this.isHide=false;
this.hideTasksHeight=0;
this.isExpanded=true;
this.descrTask=null;
this.cTaskItem=null;
this.cTaskNameItem=null;
this.parentTask=null;
this.predTask=null;
this.childTask=[];
this.childPredTask=[];
this.nextChildTask=null;
this.previousChildTask=null;
this.nextParentTask=null;
this.previousParentTask=null;
},createConnectingLinesPN:function(){
var _7=[];
var _8=_2.create("div",{innerHTML:"&nbsp;",className:"ganttTaskLineVerticalLeft"},this.ganttChart.panelNames.firstChild);
var _9=this.cTaskNameItem[0],_a=this.parentTask.cTaskNameItem[0];
_2.style(_8,{height:(_9.offsetTop-_a.offsetTop)+"px",top:(_a.offsetTop+5)+"px",left:(_a.offsetLeft-9)+"px"});
var _b=_2.create("div",{noShade:true,color:"#000000",className:"ganttTaskLineHorizontalLeft"},this.ganttChart.panelNames.firstChild);
_2.style(_b,{left:(_a.offsetLeft-9)+"px",top:(_9.offsetTop+5)+"px",height:"1px",width:(_9.offsetLeft-_a.offsetLeft+4)+"px"});
_7.push(_8);
_7.push(_b);
return _7;
},createConnectingLinesDS:function(){
var _c=this.ganttChart.contentData.firstChild;
var _d=[];
var _e=new Image();
var _e=_2.create("div",{className:"ganttImageArrow"});
var _f=document.createElement("div");
var _10=document.createElement("div");
var _11=_2.style(this.predTask.cTaskItem[0],"left");
var _12=_2.style(this.predTask.cTaskItem[0],"top");
var _13=_2.style(this.cTaskItem[0],"left");
var _14=this.posY+2;
var _15=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
var _16=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
if(_12<_14){
_2.addClass(_f,"ganttTaskLineVerticalRight");
_2.style(_f,{height:(_14-this.ganttChart.heightTaskItem/2-_12-3)+"px",width:"1px",left:(_11+_16-20)+"px",top:(_12+this.ganttChart.heightTaskItem)+"px"});
_2.addClass(_10,"ganttTaskLineHorizontal");
_2.style(_10,{width:(15+(_13-(_16+_11)))+"px",left:(_11+_16-20)+"px",top:(_14+2)+"px"});
_2.addClass(_e,"ganttTaskArrowImg");
_2.style(_e,{left:(_13-7)+"px",top:(_14-1)+"px"});
}else{
_2.addClass(_f,"ganttTaskLineVerticalRightPlus");
_2.style(_f,{height:(_12+2-_14)+"px",width:"1px",left:(_11+_16-20)+"px",top:(_14+2)+"px"});
_2.addClass(_10,"ganttTaskLineHorizontalPlus");
_2.style(_10,{width:(15+(_13-(_16+_11)))+"px",left:(_11+_16-20)+"px",top:(_14+2)+"px"});
_2.addClass(_e,"ganttTaskArrowImgPlus");
_2.style(_e,{left:(_13-7)+"px",top:(_14-1)+"px"});
}
_c.appendChild(_f);
_c.appendChild(_10);
_c.appendChild(_e);
_d.push(_f);
_d.push(_e);
_d.push(_10);
return _d;
},showChildTasks:function(_17,_18){
if(_18){
for(var i=0;i<_17.childTask.length;i++){
var _19=_17.childTask[i],_1a=_19.cTaskItem[0],_1b=_19.cTaskNameItem[0],_1c=_19.cTaskItem[1],_1d=_19.cTaskNameItem[1],_1e=_19.cTaskItem[2],_1f=_19.cTaskNameItem[2];
if(_1a.style.display=="none"){
_1a.style.display="inline";
_1b.style.display="inline";
_19.showDescTask();
_17.isHide=false;
if(_1f){
_1f.style.display="inline";
_18=_19.isExpanded;
}
for(var k=0;k<_1c.length;k++){
_1c[k].style.display="inline";
}
for(var k=0;k<_1d.length;k++){
_1d[k].style.display="inline";
}
(_19.taskIdentifier)&&(_19.taskIdentifier.style.display="inline");
this.hideTasksHeight+=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
if(_19.childTask.length>0){
this.showChildTasks(_19,_18);
}
}
}
}
},hideChildTasks:function(_20){
for(var i=0;i<_20.childTask.length;i++){
var _21=_20.childTask[i],_22=_21.cTaskItem[0],_23=_21.cTaskNameItem[0],_24=_21.cTaskItem[1],_25=_21.cTaskNameItem[1],_26=_21.cTaskItem[2],_27=_21.cTaskNameItem[2];
if(_22.style.display!="none"){
_22.style.display="none";
_23.style.display="none";
_21.hideDescTask();
_20.isHide=true;
if(_27){
_27.style.display="none";
}
for(var k=0;k<_24.length;k++){
_24[k].style.display="none";
}
for(var k=0;k<_25.length;k++){
_25[k].style.display="none";
}
(_21.taskIdentifier)&&(_21.taskIdentifier.style.display="none");
this.hideTasksHeight+=(this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra);
if(_21.childTask.length>0){
this.hideChildTasks(_21);
}
}
}
},shiftCurrentTasks:function(_28,_29){
this.shiftNextTask(this,_29);
_28.project.shiftNextProject(_28.project,_29);
},shiftTask:function(_2a,_2b){
_2a.posY=_2a.posY+_2b;
var _2c=_2a.cTaskItem[0],_2d=_2a.cTaskNameItem[0],_2e=_2a.cTaskItem[1],_2f=_2a.cTaskNameItem[1],_30=_2a.cTaskItem[2],_31=_2a.cTaskNameItem[2];
_2d.style.top=parseInt(_2d.style.top)+_2b+"px";
if(_31){
_31.style.top=parseInt(_31.style.top)+_2b+"px";
}
if(_2a.parentTask){
if(parseInt(this.cTaskNameItem[0].style.top)>parseInt(_2a.parentTask.cTaskNameItem[0].style.top)&&(_2f[0].style.display!="none")){
_2f[0].style.height=parseInt(_2f[0].style.height)+_2b+"px";
}else{
_2f[0].style.top=parseInt(_2f[0].style.top)+_2b+"px";
}
_2f[1].style.top=parseInt(_2f[1].style.top)+_2b+"px";
}
_2c.style.top=parseInt(_2c.style.top)+_2b+"px";
_2a.descrTask.style.top=parseInt(_2a.descrTask.style.top)+_2b+"px";
if(_2a.predTask){
if(((parseInt(this.cTaskItem[0].style.top)>parseInt(_2a.predTask.cTaskItem[0].style.top))||(this.cTaskItem[0].id==_2a.predTask.taskItem.id))&&_2e[0].style.display!="none"){
_2e[0].style.height=parseInt(_2e[0].style.height)+_2b+"px";
}else{
_2e[0].style.top=parseInt(_2e[0].style.top)+_2b+"px";
}
_2e[1].style.top=parseInt(_2e[1].style.top)+_2b+"px";
_2e[2].style.top=parseInt(_2e[2].style.top)+_2b+"px";
}
},shiftNextTask:function(_32,_33){
if(_32.nextChildTask){
this.shiftTask(_32.nextChildTask,_33);
this.shiftChildTask(_32.nextChildTask,_33);
this.shiftNextTask(_32.nextChildTask,_33);
}else{
if(_32.parentTask){
this.shiftNextTask(_32.parentTask,_33);
}else{
if(_32.nextParentTask){
this.shiftTask(_32.nextParentTask,_33);
this.shiftChildTask(_32.nextParentTask,_33);
this.shiftNextTask(_32.nextParentTask,_33);
}
}
}
},shiftChildTask:function(_34,_35){
_2.forEach(_34.childTask,function(_36){
this.shiftTask(_36,_35);
if(_36.childTask.length>0){
this.shiftChildTask(_36,_35);
}
},this);
},endMove:function(){
var _37=this.cTaskItem[0];
var _38=_2.style(_37,"left")-this.posX;
var _39=this.getDateOnPosition(_2.style(_37,"left"));
_39=this.checkPos(_39);
if(this.checkMove){
_38=this.ganttChart.getPosOnDate(_39)-this.posX;
this.moveCurrentTaskItem(_38,this.moveChild);
this.project.shiftProjectItem();
}
this.checkMove=false;
this.posX=0;
this.maxPosXMove=-1;
this.minPosXMove=-1;
_37.childNodes[1].firstChild.rows[0].cells[0].innerHTML="";
this.adjustPanelTime();
if(this.ganttChart.resource){
this.ganttChart.resource.refresh();
}
},checkPos:function(_3a){
var _3b=this.cTaskItem[0];
var h=_3a.getHours();
if(h>=12){
_3a.setDate(_3a.getDate()+1);
_3a.setHours(0);
if((parseInt(_3b.firstChild.firstChild.width)+this.ganttChart.getPosOnDate(_3a)>this.maxPosXMove)&&(this.maxPosXMove!=-1)){
_3a.setDate(_3a.getDate()-1);
_3a.setHours(0);
}
}else{
if((h<12)&&(h!=0)){
_3a.setHours(0);
if((this.ganttChart.getPosOnDate(_3a)<this.minPosXMove)){
_3a.setDate(_3a.getDate()+1);
}
}
}
_3b.style.left=this.ganttChart.getPosOnDate(_3a)+"px";
return _3a;
},getMaxPosPredChildTaskItem:function(){
var _3c=0;
var _3d=0;
for(var i=0;i<this.childPredTask.length;i++){
_3d=this.getMaxPosPredChildTaskItemInTree(this.childPredTask[i]);
if(_3d>_3c){
_3c=_3d;
}
}
return _3c;
},getMaxPosPredChildTaskItemInTree:function(_3e){
var _3f=_3e.cTaskItem[0];
var _40=parseInt(_3f.firstChild.firstChild.width)+_2.style(_3f,"left");
var _41=0;
var _42=0;
_2.forEach(_3e.childPredTask,function(_43){
_42=this.getMaxPosPredChildTaskItemInTree(_43);
if(_42>_41){
_41=_42;
}
},this);
return _41>_40?_41:_40;
},moveCurrentTaskItem:function(_44,_45){
var _46=this.cTaskItem[0];
this.taskItem.startTime=new Date(this.ganttChart.startDate);
this.taskItem.startTime.setHours(this.taskItem.startTime.getHours()+(parseInt(_46.style.left)/this.ganttChart.pixelsPerHour));
this.showDescTask();
var _47=this.cTaskItem[1];
if(_47.length>0){
_47[2].style.width=parseInt(_47[2].style.width)+_44+"px";
_47[1].style.left=parseInt(_47[1].style.left)+_44+"px";
}
_2.forEach(this.childTask,function(_48){
if(!_48.predTask){
this.moveChildTaskItems(_48,_44,_45);
}
},this);
_2.forEach(this.childPredTask,function(_49){
this.moveChildTaskItems(_49,_44,_45);
},this);
},moveChildTaskItems:function(_4a,_4b,_4c){
var _4d=_4a.cTaskItem[0];
if(_4c){
_4d.style.left=parseInt(_4d.style.left)+_4b+"px";
_4a.adjustPanelTime();
_4a.taskItem.startTime=new Date(this.ganttChart.startDate);
_4a.taskItem.startTime.setHours(_4a.taskItem.startTime.getHours()+(parseInt(_4d.style.left)/this.ganttChart.pixelsPerHour));
var _4e=_4a.cTaskItem[1];
_2.forEach(_4e,function(_4f){
_4f.style.left=parseInt(_4f.style.left)+_4b+"px";
},this);
_2.forEach(_4a.childTask,function(_50){
if(!_50.predTask){
this.moveChildTaskItems(_50,_4b,_4c);
}
},this);
_2.forEach(_4a.childPredTask,function(_51){
this.moveChildTaskItems(_51,_4b,_4c);
},this);
}else{
var _4e=_4a.cTaskItem[1];
if(_4e.length>0){
var _52=_4e[0],_53=_4e[2];
_53.style.left=parseInt(_53.style.left)+_4b+"px";
_53.style.width=parseInt(_53.style.width)-_4b+"px";
_52.style.left=parseInt(_52.style.left)+_4b+"px";
}
}
_4a.moveDescTask();
},adjustPanelTime:function(){
var _54=this.cTaskItem[0];
var _55=parseInt(_54.style.left)+parseInt(_54.firstChild.firstChild.width)+this.ganttChart.panelTimeExpandDelta;
_55+=this.descrTask.offsetWidth;
this.ganttChart.adjustPanelTime(_55);
},getDateOnPosition:function(_56){
var _57=new Date(this.ganttChart.startDate);
_57.setHours(_57.getHours()+(_56/this.ganttChart.pixelsPerHour));
return _57;
},moveItem:function(_58){
var _59=_58.screenX;
var _5a=(this.posX+(_59-this.mouseX));
var _5b=parseInt(this.cTaskItem[0].childNodes[0].firstChild.width);
var _5c=_5a+_5b;
if(this.checkMove){
if(((this.minPosXMove<=_5a))&&((_5c<=this.maxPosXMove)||(this.maxPosXMove==-1))){
this.moveTaskItem(_5a);
}
}
},moveTaskItem:function(_5d){
var _5e=this.cTaskItem[0];
_5e.style.left=_5d+"px";
_5e.childNodes[1].firstChild.rows[0].cells[0].innerHTML=this.getDateOnPosition(_5d).getDate()+"."+(this.getDateOnPosition(_5d).getMonth()+1)+"."+this.getDateOnPosition(_5d).getUTCFullYear();
},resizeItem:function(_5f){
if(this.checkResize){
var _60=this.cTaskItem[0];
var _61=_5f.screenX;
var _62=(_61-this.mouseX);
var _63=this.taskItemWidth+(_61-this.mouseX);
if(_63>=this.taskItemWidth){
if((_63<=this.maxWidthResize)||(this.maxWidthResize==-1)){
this.resizeTaskItem(_63);
}else{
if((this.maxWidthResize!=-1)&&(_63>this.maxWidthResize)){
this.resizeTaskItem(this.maxWidthResize);
}
}
}else{
if(_63<=this.taskItemWidth){
if(_63>=this.minWidthResize){
this.resizeTaskItem(_63);
}else{
if(_63<this.minWidthResize){
this.resizeTaskItem(this.minWidthResize);
}
}
}
}
}
},resizeTaskItem:function(_64){
var _65=this.cTaskItem[0];
var _66=Math.round(_64/this.ganttChart.pixelsPerWorkHour);
var _67=_65.childNodes[0].firstChild.rows[0],rc0=_67.cells[0],rc1=_67.cells[1];
rc0&&(rc0.firstChild.style.width=parseInt(rc0.width)*_64/100+"px");
rc1&&(rc1.firstChild.style.width=parseInt(rc1.width)*_64/100+"px");
_65.childNodes[0].firstChild.width=_64+"px";
_65.childNodes[1].firstChild.width=_64+"px";
this.cTaskItem[0].childNodes[1].firstChild.rows[0].cells[0].innerHTML=_66;
var _68=_65.childNodes[2];
_68.childNodes[0].style.width=_64+"px";
_68.childNodes[1].style.left=_64-10+"px";
},endResizeItem:function(){
var _69=this.cTaskItem[0];
if((this.taskItemWidth!=parseInt(_69.childNodes[0].firstChild.width))){
var _6a=_69.offsetLeft;
var _6b=_69.offsetLeft+parseInt(_69.childNodes[0].firstChild.width);
var _6c=Math.round((_6b-_6a)/this.ganttChart.pixelsPerWorkHour);
this.taskItem.duration=_6c;
if(this.childPredTask.length>0){
for(var j=0;j<this.childPredTask.length;j++){
var _6d=this.childPredTask[j].cTaskItem[1],_6e=_6d[0],_6f=_6d[2],_70=_69.childNodes[0];
_6f.style.width=parseInt(_6f.style.width)-(parseInt(_70.firstChild.width)-this.taskItemWidth)+"px";
_6f.style.left=parseInt(_6f.style.left)+(parseInt(_70.firstChild.width)-this.taskItemWidth)+"px";
_6e.style.left=parseInt(_6e.style.left)+(parseInt(_70.firstChild.width)-this.taskItemWidth)+"px";
}
}
}
this.cTaskItem[0].childNodes[1].firstChild.rows[0].cells[0].innerHTML="";
this.checkResize=false;
this.taskItemWidth=0;
this.mouseX=0;
this.showDescTask();
this.project.shiftProjectItem();
this.adjustPanelTime();
if(this.ganttChart.resource){
this.ganttChart.resource.refresh();
}
},startMove:function(_71){
this.moveChild=_71.ctrlKey;
this.mouseX=_71.screenX;
this.getMoveInfo();
this.checkMove=true;
this.hideDescTask();
},showDescTask:function(){
var _72=(parseInt(this.cTaskItem[0].style.left)+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrTask.style.left=_72+"px";
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
this.descrTask.style.visibility="visible";
},hideDescTask:function(){
_2.style(this.descrTask,"visibility","hidden");
},buildResourceInfo:function(_73){
if(this.childTask&&this.childTask.length>0){
for(var i=0;i<this.childTask.length;i++){
var _74=this.childTask[i];
_74.buildResourceInfo(_73);
}
}
if(_2.trim(this.taskItem.taskOwner).length>0){
var _75=this.taskItem.taskOwner.split(";");
for(var i=0;i<_75.length;i++){
var o=_75[i];
if(_2.trim(o).length<=0){
continue;
}
_73[o]?(_73[o].push(this)):(_73[o]=[this]);
}
}
},objKeyToStr:function(obj,_76){
var _77="";
_76=_76||" ";
if(obj){
for(var key in obj){
_77+=_76+key;
}
}
return _77;
},getTaskOwner:function(){
var _78={};
if(_2.trim(this.taskItem.taskOwner).length>0){
var _79=this.taskItem.taskOwner.split(";");
for(var i=0;i<_79.length;i++){
var o=_79[i];
_78[o]=1;
}
}
_2.forEach(this.childTask,function(_7a){
_2.mixin(_78,_7a.getTaskOwner());
},this);
return _78;
},moveDescTask:function(){
var _7b=(parseInt(this.cTaskItem[0].style.left)+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrTask.style.left=_7b+"px";
},getMoveInfo:function(){
this.posX=parseInt(this.cTaskItem[0].style.left);
var _7c=parseInt(this.cTaskItem[0].childNodes[0].firstChild.width);
var _7d=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].style.left);
var _7e=!this.predTask?0:parseInt(this.predTask.cTaskItem[0].style.left)+parseInt(this.predTask.cTaskItem[0].childNodes[0].firstChild.width);
var _7f=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].childNodes[0].firstChild.width);
var _80=0;
var _81=0;
var _82=0;
if(this.childPredTask.length>0){
var _83=null;
_2.forEach(this.childPredTask,function(_84){
if((!_83)||((_83)&&(_83>parseInt(_84.cTaskItem[0].style.left)))){
_83=parseInt(_84.cTaskItem[0].style.left);
}
},this);
_80=_83;
}
if(this.childTask.length>0){
var _85=null;
_2.forEach(this.childTask,function(_86){
if((!_85)||((_85)&&(_85>(parseInt(_86.cTaskItem[0].style.left))))){
_85=parseInt(_86.cTaskItem[0].style.left);
}
},this);
_82=_85;
var _83=null;
_2.forEach(this.childTask,function(_87){
if((!_83)||((_83)&&(_83<(parseInt(_87.cTaskItem[0].style.left)+parseInt(_87.cTaskItem[0].firstChild.firstChild.width))))){
_83=parseInt(_87.cTaskItem[0].style.left)+parseInt(_87.cTaskItem[0].firstChild.firstChild.width);
}
},this);
_81=_83;
}
if(!this.moveChild){
if(this.childPredTask.length>0){
if(this.maxPosXMove<_80){
this.maxPosXMove=_80;
}
}
if(this.childTask.length>0){
if((this.childPredTask.length>0)&&(this.maxPosXMove-_7c)>_82){
this.maxPosXMove=this.maxPosXMove-((this.maxPosXMove-_7c)-_82);
}
if(!(this.childPredTask.length>0)){
this.maxPosXMove=_82+_7c;
}
this.minPosXMove=(_81-_7c);
}
if(_7d>0){
if((!(this.childPredTask.length>0))&&(this.childTask.length>0)){
if(this.maxPosXMove>_7d+_7f){
this.maxPosXMove=_7d+_7f;
}
}
if(this.minPosXMove<=_7d){
this.minPosXMove=_7d;
}
if((!(this.childTask.length>0))&&(!(this.childPredTask.length>0))){
this.maxPosXMove=_7d+_7f;
}else{
if((!(this.childTask.length>0))&&(this.childPredTask.length>0)){
if((_7d+_7f)>_7e){
this.maxPosXMove=_80;
}
}
}
}
if(_7e>0){
if(this.minPosXMove<=_7e){
this.minPosXMove=_7e;
}
}
if((_7e==0)&&(_7d==0)){
if(this.minPosXMove<=this.ganttChart.initialPos){
this.minPosXMove=this.ganttChart.initialPos;
}
}
}else{
if((_7d>0)&&(_7e==0)){
this.minPosXMove=_7d;
this.maxPosXMove=_7d+_7f;
}else{
if((_7d==0)&&(_7e==0)){
this.minPosXMove=this.ganttChart.initialPos;
this.maxPosXMove=-1;
}else{
if((_7d>0)&&(_7e>0)){
this.minPosXMove=_7e;
this.maxPosXMove=_7d+_7f;
}else{
if((_7d==0)&&(_7e>0)){
this.minPosXMove=_7e;
this.maxPosXMove=-1;
}
}
}
}
if((this.parentTask)&&(this.childPredTask.length>0)){
var _83=this.getMaxPosPredChildTaskItem(this);
var _7d=parseInt(this.parentTask.cTaskItem[0].style.left)+parseInt(this.parentTask.cTaskItem[0].firstChild.firstChild.width);
this.maxPosXMove=this.posX+_7c+_7d-_83;
}
}
},startResize:function(_88){
this.mouseX=_88.screenX;
this.getResizeInfo();
this.hideDescTask();
this.checkResize=true;
this.taskItemWidth=parseInt(this.cTaskItem[0].firstChild.firstChild.width);
},getResizeInfo:function(){
var _89=this.cTaskItem[0];
var _8a=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].style.left);
var _8b=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].childNodes[0].firstChild.width);
var _8c=parseInt(_89.style.left);
var _8d=0;
var _8e=0;
if(this.childPredTask.length>0){
var _8f=null;
_2.forEach(this.childPredTask,function(_90){
if((!_8f)||((_8f)&&(_8f>parseInt(_90.cTaskItem[0].style.left)))){
_8f=parseInt(_90.cTaskItem[0].style.left);
}
},this);
_8d=_8f;
}
if(this.childTask.length>0){
var _8f=null;
_2.forEach(this.childTask,function(_91){
if((!_8f)||((_8f)&&(_8f<(parseInt(_91.cTaskItem[0].style.left)+parseInt(_91.cTaskItem[0].firstChild.firstChild.width))))){
_8f=parseInt(_91.cTaskItem[0].style.left)+parseInt(_91.cTaskItem[0].firstChild.firstChild.width);
}
},this);
_8e=_8f;
}
this.minWidthResize=this.ganttChart.pixelsPerDay;
if(this.childTask.length>0){
this.minWidthResize=_8e-_8c;
}
if((this.childPredTask.length>0)&&(!this.parentTask)){
this.maxWidthResize=_8d-_8c;
}else{
if((this.childPredTask.length>0)&&(this.parentTask)){
var w1=_8a+_8b-_8c;
var w2=_8d-_8c;
this.maxWidthResize=Math.min(w1,w2);
}else{
if((this.childPredTask.length==0)&&(this.parentTask)){
this.maxWidthResize=_8a+_8b-_8c;
}
}
}
},createTaskItem:function(){
this.posX=this.ganttChart.getPosOnDate(this.taskItem.startTime);
var _92=_2.create("div",{id:this.taskItem.id,className:"ganttTaskItemControl"});
_2.style(_92,{left:this.posX+"px",top:this.posY+"px"});
var _93=_2.create("div",{className:"ganttTaskDivTaskItem"},_92);
var _94=_2.create("table",{cellPadding:"0",cellSpacing:"0",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",className:"ganttTaskTblTaskItem"},_93);
var _95=_94.insertRow(_94.rows.length);
if(this.taskItem.percentage!=0){
var _96=_2.create("td",{height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.percentage+"%"},_95);
_96.style.lineHeight="1px";
var _97=_2.create("div",{className:"ganttImageTaskProgressFilled"},_96);
_2.style(_97,{width:(this.taskItem.percentage*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.taskItem.percentage!=100){
var _96=_2.create("td",{height:this.ganttChart.heightTaskItem+"px",width:(100-this.taskItem.percentage)+"%"},_95);
_96.style.lineHeight="1px";
var _98=_2.create("div",{className:"ganttImageTaskProgressBg"},_96);
_2.style(_98,{width:((100-this.taskItem.percentage)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.ganttChart.isContentEditable){
var _99=_2.create("div",{className:"ganttTaskDivTaskInfo"},_92);
var _9a=_2.create("table",{cellPadding:"0",cellSpacing:"0",height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"},_99);
var _9b=_9a.insertRow(0);
var _9c=_2.create("td",{align:"center",vAlign:"top",height:this.ganttChart.heightTaskItem+"px",className:"ganttMoveInfo"},_9b);
var _9d=_2.create("div",{className:"ganttTaskDivTaskName"},_92);
var _9e=_2.create("div",{},_9d);
_2.create("input",{className:"ganttTaskDivMoveInput",type:"text"},_9e);
_2.isIE&&_2.style(_9e,{background:"#000000",filter:"alpha(opacity=0)"});
_2.style(_9e,{height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _9f=_2.create("div",{className:"ganttTaskDivResize"},_9d);
_2.create("input",{className:"ganttTaskDivResizeInput",type:"text"},_9f);
_2.style(_9f,{left:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour-10)+"px",height:this.ganttChart.heightTaskItem+"px",width:"10px"});
this.ganttChart._events.push(_2.connect(_9e,"onmousedown",this,function(_a0){
this.moveMoveConn=_2.connect(document,"onmousemove",this,function(e){
this.checkMove&&this.moveItem(e);
});
this.moveUpConn=_2.connect(document,"onmouseup",this,function(e){
if(this.checkMove){
this.endMove();
this.ganttChart.isMoving=false;
document.body.releaseCapture&&document.body.releaseCapture();
_2.disconnect(this.moveMoveConn);
_2.disconnect(this.moveUpConn);
}
});
this.startMove(_a0);
this.ganttChart.isMoving=true;
document.body.setCapture&&document.body.setCapture(false);
}));
this.ganttChart._events.push(_2.connect(_9e,"onmouseover",this,function(_a1){
_a1.target&&(_a1.target.style.cursor="move");
}));
this.ganttChart._events.push(_2.connect(_9e,"onmouseout",this,function(_a2){
_a2.target.style.cursor="";
}));
this.ganttChart._events.push(_2.connect(_9f,"onmousedown",this,function(_a3){
this.resizeMoveConn=_2.connect(document,"onmousemove",this,function(e){
this.checkResize&&this.resizeItem(e);
});
this.resizeUpConn=_2.connect(document,"onmouseup",this,function(e){
if(this.checkResize){
this.endResizeItem();
this.ganttChart.isResizing=false;
document.body.releaseCapture&&document.body.releaseCapture();
_2.disconnect(this.resizeMoveConn);
_2.disconnect(this.resizeUpConn);
}
});
this.startResize(_a3);
this.ganttChart.isResizing=true;
document.body.setCapture&&document.body.setCapture(false);
}));
this.ganttChart._events.push(_2.connect(_9f,"onmouseover",this,function(_a4){
(!this.ganttChart.isMoving)&&(!this.ganttChart.isResizing)&&_a4.target&&(_a4.target.style.cursor="e-resize");
}));
this.ganttChart._events.push(_2.connect(_9f,"onmouseout",this,function(_a5){
!this.checkResize&&_a5.target&&(_a5.target.style.cursor="");
}));
}
return _92;
},createTaskNameItem:function(){
var _a6=_2.create("div",{id:this.taskItem.id,className:"ganttTaskTaskNameItem",title:this.taskItem.name+", id: "+this.taskItem.id+" ",innerHTML:this.taskItem.name});
_2.style(_a6,"top",this.posY+"px");
_2.attr(_a6,"tabIndex",0);
if(this.ganttChart.isShowConMenu){
this.ganttChart._events.push(_2.connect(_a6,"onmouseover",this,function(_a7){
_2.addClass(_a6,"ganttTaskTaskNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_a7.target,this);
}));
this.ganttChart._events.push(_2.connect(_a6,"onkeydown",this,function(_a8){
if(_a8.keyCode==_2.keys.ENTER){
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_a8.target,this);
}
if(this.ganttChart.tabMenu.isShow&&(_a8.keyCode==_2.keys.LEFT_ARROW||_a8.keyCode==_2.keys.RIGHT_ARROW)){
_1.focus(this.ganttChart.tabMenu.menuPanel.firstChild.rows[0].cells[0]);
}
if(this.ganttChart.tabMenu.isShow&&_a8.keyCode==_2.keys.ESCAPE){
this.ganttChart.tabMenu.hide();
}
}));
this.ganttChart._events.push(_2.connect(_a6,"onmouseout",this,function(){
_2.removeClass(_a6,"ganttTaskTaskNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_2.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
}));
this.ganttChart._events.push(_2.connect(this.ganttChart.tabMenu.menuPanel,"onmouseover",this,function(){
clearTimeout(this.ganttChart.menuTimer);
}));
this.ganttChart._events.push(_2.connect(this.ganttChart.tabMenu.menuPanel,"onkeydown",this,function(_a9){
if(this.ganttChart.tabMenu.isShow&&_a9.keyCode==_2.keys.ESCAPE){
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
return _a6;
},createTaskDescItem:function(){
var _aa=(this.posX+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
var _ab=_2.create("div",{innerHTML:this.objKeyToStr(this.getTaskOwner()),className:"ganttTaskDescTask"});
_2.style(_ab,{left:_aa+"px",top:this.posY+"px"});
return this.descrTask=_ab;
},checkWidthTaskNameItem:function(){
if(this.cTaskNameItem[0].offsetWidth+this.cTaskNameItem[0].offsetLeft>this.ganttChart.maxWidthTaskNames){
var _ac=this.cTaskNameItem[0].offsetWidth+this.cTaskNameItem[0].offsetLeft-this.ganttChart.maxWidthTaskNames;
var _ad=Math.round(_ac/(this.cTaskNameItem[0].offsetWidth/this.cTaskNameItem[0].firstChild.length));
var _ae=this.taskItem.name.substring(0,this.cTaskNameItem[0].firstChild.length-_ad-3);
_ae+="...";
this.cTaskNameItem[0].innerHTML=_ae;
}
},refreshTaskItem:function(_af){
this.posX=this.ganttChart.getPosOnDate(this.taskItem.startTime);
_2.style(_af,{"left":this.posX+"px"});
var _b0=_af.childNodes[0];
var _b1=_b0.firstChild;
_b1.width=(!this.taskItem.duration?1:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)+"px";
var _b2=_b1.rows[0];
if(this.taskItem.percentage!=0){
var _b3=_b2.firstChild;
_b3.height=this.ganttChart.heightTaskItem+"px";
_b3.width=this.taskItem.percentage+"%";
_b3.style.lineHeight="1px";
var _b4=_b3.firstChild;
_2.style(_b4,{width:(!this.taskItem.duration?1:(this.taskItem.percentage*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.taskItem.percentage!=100){
var _b3=_b2.lastChild;
_b3.height=this.ganttChart.heightTaskItem+"px";
_b3.width=(100-this.taskItem.percentage)+"%";
_b3.style.lineHeight="1px";
var _b5=_b3.firstChild;
_2.style(_b5,{width:(!this.taskItem.duration?1:((100-this.taskItem.percentage)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.ganttChart.isContentEditable){
var _b6=_af.childNodes[1];
var _b7=_b6.firstChild;
_b7.height=this.ganttChart.heightTaskItem+"px";
_b7.width=(!this.taskItem.duration?1:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour))+"px";
var _b8=_b7.rows[0];
var _b9=_b8.firstChild;
_b9.height=this.ganttChart.heightTaskItem+"px";
var _ba=_af.childNodes[2];
var _bb=_ba.firstChild;
_bb.style.height=this.ganttChart.heightTaskItem+"px";
_bb.style.width=(!this.taskItem.duration?1:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour))+"px";
var _bc=_ba.lastChild;
_2.style(_bc,{"left":(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour-10)+"px"});
_bc.style.height=this.ganttChart.heightTaskItem+"px";
_bc.style.width="10px";
}
return _af;
},refreshTaskDesc:function(_bd){
var _be=(this.posX+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
_2.style(_bd,{"left":_be+"px"});
return _bd;
},refreshConnectingLinesDS:function(_bf){
var _c0=_bf[1];
var _c1=_bf[0];
var _c2=_bf[2];
var _c3=_2.style(this.predTask.cTaskItem[0],"left");
var _c4=_2.style(this.predTask.cTaskItem[0],"top");
var _c5=_2.style(this.cTaskItem[0],"left");
var _c6=this.posY+2;
var _c7=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
var _c8=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
if(_c4<_c6){
_2.style(_c1,{"height":(_c6-this.ganttChart.heightTaskItem/2-_c4-3)+"px","left":(_c3+_c8-20)+"px"});
_2.style(_c2,{"width":(15+(_c5-(_c8+_c3)))+"px","left":(_c3+_c8-20)+"px"});
_2.style(_c0,{"left":(_c5-7)+"px"});
}else{
_2.style(_c1,{"height":(_c4+2-_c6)+"px","left":(_c3+_c8-20)+"px"});
_2.style(_c2,{"width":(15+(_c5-(_c8+_c3)))+"px","left":(_c3+_c8-20)+"px"});
_2.style(_c0,{"left":(_c5-7)+"px"});
}
return _bf;
},postLoadData:function(){
},refresh:function(){
if(this.childTask&&this.childTask.length>0){
_2.forEach(this.childTask,function(_c9){
_c9.refresh();
},this);
}
this.refreshTaskItem(this.cTaskItem[0]);
this.refreshTaskDesc(this.cTaskItem[0].nextSibling);
var _ca=[];
if(this.taskItem.previousTask&&this.predTask){
this.refreshConnectingLinesDS(this.cTaskItem[1]);
}
return this;
},create:function(){
var _cb=this.ganttChart.contentData.firstChild;
var _cc=this.ganttChart.panelNames.firstChild;
var _cd=this.taskItem.previousTask;
var _ce=this.taskItem.parentTask;
var _cf=(this.taskItem.cldTasks.length>0)?true:false;
this.cTaskItem=[];
this.cTaskNameItem=[];
if(!_ce){
if(this.taskItem.previousParentTask){
this.previousParentTask=this.project.getTaskById(this.taskItem.previousParentTask.id);
var _d0=this.ganttChart.getLastChildTask(this.previousParentTask);
this.posY=parseInt(_d0.cTaskItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
this.previousParentTask.nextParentTask=this;
}else{
this.posY=parseInt(this.project.projectItem[0].style.top)+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}
}
if(_ce){
var _d1=this.project.getTaskById(this.taskItem.parentTask.id);
this.parentTask=_d1;
if(this.taskItem.previousChildTask){
this.previousChildTask=this.project.getTaskById(this.taskItem.previousChildTask.id);
var _d0=this.ganttChart.getLastChildTask(this.previousChildTask);
this.posY=_2.style(_d0.cTaskItem[0],"top")+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
this.previousChildTask.nextChildTask=this;
}else{
this.posY=_2.style(_d1.cTaskItem[0],"top")+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}
_d1.childTask.push(this);
}
if(_cd){
var _d1=this.project.getTaskById(_cd.id);
this.predTask=_d1;
_d1.childPredTask.push(this);
}
this.cTaskItem.push(this.createTaskItem());
_cb.appendChild(this.cTaskItem[0]);
if(this.ganttChart.panelNames){
this.cTaskNameItem.push(this.createTaskNameItem());
this.ganttChart.panelNames.firstChild.appendChild(this.cTaskNameItem[0]);
}
_cb.appendChild(this.createTaskDescItem());
var _d2=[];
if(_cd){
_d2=this.createConnectingLinesDS();
}
this.cTaskItem.push(_d2);
if(this.ganttChart.panelNames){
var _d3=[];
if(_ce){
this.cTaskNameItem[0].style.left=_2.style(this.parentTask.cTaskNameItem[0],"left")+15+"px";
_d3=this.createConnectingLinesPN();
}
this.checkWidthTaskNameItem();
this.checkPosition();
var _d4=null;
if(_cf){
_d4=this.createTreeImg();
}
this.cTaskNameItem.push(_d3);
this.cTaskNameItem.push(_d4);
}
this.adjustPanelTime();
return this;
},checkPosition:function(){
if(!this.ganttChart.withTaskId){
return;
}
var pos=_2.coords(this.cTaskNameItem[0],true);
if(this.taskIdentifier){
if(this.childTask&&this.childTask.length>0){
_2.forEach(this.childTask,function(_d5){
_d5.checkPosition();
},this);
}
_2.style(this.taskIdentifier,{"left":(pos.l+pos.w+4)+"px","top":(pos.t-1)+"px"});
}else{
this.taskIdentifier=_2.create("div",{id:"TaskId_"+this.taskItem.id,className:"ganttTaskIdentifier",title:this.taskItem.id,innerHTML:this.taskItem.id},this.cTaskNameItem[0].parentNode);
_2.style(this.taskIdentifier,{left:(pos.l+pos.w+4)+"px",top:(pos.t-1)+"px"});
}
},createTreeImg:function(){
var _d6=_2.create("div",{id:this.taskItem.id,className:"ganttImageTreeCollapse"});
_2.attr(_d6,"tabIndex",0);
_2.forEach(["onclick","onkeydown"],function(e){
this.ganttChart._events.push(_2.connect(_d6,e,this,function(evt){
if(e=="onkeydown"&&evt.keyCode!=_2.keys.ENTER){
return;
}
if(this.isExpanded){
_2.removeClass(_d6,"ganttImageTreeCollapse");
_2.addClass(_d6,"ganttImageTreeExpand");
this.isExpanded=false;
this.hideChildTasks(this);
this.shiftCurrentTasks(this,-this.hideTasksHeight);
this.ganttChart.checkPosition();
}else{
_2.removeClass(_d6,"ganttImageTreeExpand");
_2.addClass(_d6,"ganttImageTreeCollapse");
this.isExpanded=true;
this.shiftCurrentTasks(this,this.hideTasksHeight);
this.showChildTasks(this,true);
this.hideTasksHeight=0;
this.ganttChart.checkPosition();
}
}));
},this);
this.ganttChart.panelNames.firstChild.appendChild(_d6);
_2.addClass(_d6,"ganttTaskTreeImage");
_2.style(_d6,{left:(_2.style(this.cTaskNameItem[0],"left")-12)+"px",top:(_2.style(this.cTaskNameItem[0],"top")+3)+"px"});
return _d6;
},setPreviousTask:function(_d7){
if(_d7==""){
this.clearPredTask();
}else{
var _d8=this.taskItem;
if(_d8.id==_d7){
return false;
}
var _d9=this.project.getTaskById(_d7);
if(!_d9){
return false;
}
var _da=_d9.taskItem;
var a1=_da.parentTask==null,a2=_d8.parentTask==null;
if(a1&&!a2||!a1&&a2||!a1&&!a2&&(_da.parentTask.id!=_d8.parentTask.id)){
return false;
}
var _db=_d8.startTime.getTime(),_dc=_da.startTime.getTime(),_dd=_da.duration*24*60*60*1000/_d9.ganttChart.hsPerDay;
if((_dc+_dd)>_db){
return false;
}
this.clearPredTask();
if(!this.ganttChart.checkPosPreviousTask(_da,_d8)){
this.ganttChart.correctPosPreviousTask(_da,_d8,this);
}
_d8.previousTaskId=_d7;
_d8.previousTask=_da;
this.predTask=_d9;
_d9.childPredTask.push(this);
this.cTaskItem[1]=this.createConnectingLinesDS();
}
return true;
},clearPredTask:function(){
if(this.predTask){
var ch=this.predTask.childPredTask;
for(var i=0;i<ch.length;i++){
if(ch[i]==this){
ch.splice(i,1);
break;
}
}
for(var i=0;i<this.cTaskItem[1].length;i++){
this.cTaskItem[1][i].parentNode.removeChild(this.cTaskItem[1][i]);
}
this.cTaskItem[1]=[];
this.taskItem.previousTaskId=null;
this.taskItem.previousTask=null;
this.predTask=null;
}
},setStartTime:function(_de,_df){
this.moveChild=_df;
this.getMoveInfo();
var pos=this.ganttChart.getPosOnDate(_de);
if((parseInt(this.cTaskItem[0].firstChild.firstChild.width)+pos>this.maxPosXMove)&&(this.maxPosXMove!=-1)){
this.maxPosXMove=-1;
this.minPosXMove=-1;
return false;
}
if(pos<this.minPosXMove){
this.maxPosXMove=-1;
this.minPosXMove=-1;
return false;
}
this.cTaskItem[0].style.left=pos;
var _e0=pos-this.posX;
this.moveCurrentTaskItem(_e0,_df);
this.project.shiftProjectItem();
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
this.adjustPanelTime();
this.posX=0;
this.maxPosXMove=-1;
this.minPosXMove=-1;
return true;
},setDuration:function(_e1){
this.getResizeInfo();
var _e2=this.ganttChart.getWidthOnDuration(_e1);
if((_e2>this.maxWidthResize)&&(this.maxWidthResize!=-1)){
return false;
}else{
if(_e2<this.minWidthResize){
return false;
}else{
this.taskItemWidth=parseInt(this.cTaskItem[0].firstChild.firstChild.width);
this.resizeTaskItem(_e2);
this.endResizeItem();
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
return true;
}
}
},setTaskOwner:function(_e3){
_e3=(_e3==null||_e3==undefined)?"":_e3;
this.taskItem.taskOwner=_e3;
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
return true;
},setPercentCompleted:function(_e4){
_e4=parseInt(_e4);
if(isNaN(_e4)||_e4>100||_e4<0){
return false;
}
var _e5=this.cTaskItem[0].childNodes[0].firstChild.rows[0],rc0=_e5.cells[0],rc1=_e5.cells[1];
if((_e4!=0)&&(_e4!=100)){
if((this.taskItem.percentage!=0)&&(this.taskItem.percentage!=100)){
rc0.width=_e4+"%";
rc1.width=100-_e4+"%";
}else{
if((this.taskItem.percentage==0)||(this.taskItem.percentage==100)){
rc0.parentNode.removeChild(rc0);
var _e6=_2.create("td",{height:this.ganttChart.heightTaskItem+"px",width:_e4+"%"},_e5);
_e6.style.lineHeight="1px";
var _e7=_2.create("div",{className:"ganttImageTaskProgressFilled"},_e6);
_2.style(_e7,{width:(_e4*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_e6=_2.create("td",{height:this.ganttChart.heightTaskItem+"px",width:(100-_e4)+"%"},_e5);
_e6.style.lineHeight="1px";
_e7=_2.create("div",{className:"ganttImageTaskProgressBg"},_e6);
_2.style(_e7,{width:((100-_e4)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}
}else{
if(_e4==0){
if((this.taskItem.percentage!=0)&&(this.taskItem.percentage!=100)){
rc0.parentNode.removeChild(rc0);
rc1.width=100+"%";
}else{
_2.removeClass(rc0.firstChild,"ganttImageTaskProgressFilled");
_2.addClass(rc0.firstChild,"ganttImageTaskProgressBg");
}
}else{
if(_e4==100){
if((this.taskItem.percentage!=0)&&(this.taskItem.percentage!=100)){
rc1.parentNode.removeChild(rc1);
rc0.width=100+"%";
}else{
_2.removeClass(rc0.firstChild,"ganttImageTaskProgressBg");
_2.addClass(rc0.firstChild,"ganttImageTaskProgressFilled");
}
}
}
}
this.taskItem.percentage=_e4;
this.taskItemWidth=parseInt(this.cTaskItem[0].firstChild.firstChild.width);
this.resizeTaskItem(this.taskItemWidth);
this.endResizeItem();
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
return true;
},setName:function(_e8){
if(_e8){
this.taskItem.name=_e8;
this.cTaskNameItem[0].innerHTML=_e8;
this.cTaskNameItem[0].title=_e8;
this.checkWidthTaskNameItem();
this.checkPosition();
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
this.adjustPanelTime();
}
}});
_2.declare("dojox.gantt.GanttTaskItem",null,{constructor:function(_e9){
this.id=_e9.id;
this.name=_e9.name||this.id;
this.startTime=_e9.startTime||new Date();
this.duration=_e9.duration||8;
this.percentage=_e9.percentage||0;
this.previousTaskId=_e9.previousTaskId||"";
this.taskOwner=_e9.taskOwner||"";
this.cldTasks=[];
this.cldPreTasks=[];
this.parentTask=null;
this.previousTask=null;
this.project=null;
this.nextChildTask=null;
this.previousChildTask=null;
this.nextParentTask=null;
this.previousParentTask=null;
},addChildTask:function(_ea){
this.cldTasks.push(_ea);
_ea.parentTask=this;
},setProject:function(_eb){
this.project=_eb;
for(var j=0;j<this.cldTasks.length;j++){
this.cldTasks[j].setProject(_eb);
}
}});
});
