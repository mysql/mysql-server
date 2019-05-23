//>>built
define("dojox/gantt/GanttTaskControl",["dijit/focus","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/date/locale","dojo/request","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dojo/keys","dojo/domReady!"],function(_1,_2,_3,_4,_5,_6,on,_7,_8,_9,_a,_b,_c,_d){
return _2("dojox.gantt.GanttTaskControl",[],{constructor:function(_e,_f,_10){
this.ganttChart=_10;
this.project=_f;
this.taskItem=_e;
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
var _11=[];
var _12=_9.create("div",{innerHTML:"&nbsp;",className:"ganttTaskLineVerticalLeft"},this.ganttChart.panelNames.firstChild);
var _13=this.cTaskNameItem[0],_14=this.parentTask.cTaskNameItem[0];
_a.set(_12,{height:(_13.offsetTop-_14.offsetTop)+"px",top:(_14.offsetTop+5)+"px",left:(_14.offsetLeft-9)+"px"});
var _15=_9.create("div",{noShade:true,color:"#000000",className:"ganttTaskLineHorizontalLeft"},this.ganttChart.panelNames.firstChild);
_a.set(_15,{left:(_14.offsetLeft-9)+"px",top:(_13.offsetTop+5)+"px",height:"1px",width:(_13.offsetLeft-_14.offsetLeft+4)+"px"});
_11.push(_12);
_11.push(_15);
return _11;
},createConnectingLinesDS:function(){
var _16=this.ganttChart.contentData.firstChild;
var _17=[];
var _18=_9.create("div",{className:"ganttImageArrow"});
var _19=document.createElement("div");
var _1a=document.createElement("div");
var _1b=_a.get(this.predTask.cTaskItem[0],"left");
var _1c=_a.get(this.predTask.cTaskItem[0],"top");
var _1d=_a.get(this.cTaskItem[0],"left");
var _1e=this.posY+2;
var _1f=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
if(_1c<_1e){
_8.add(_19,"ganttTaskLineVerticalRight");
_a.set(_19,{height:(_1e-this.ganttChart.heightTaskItem/2-_1c-3)+"px",width:"1px",left:(_1b+_1f-20)+"px",top:(_1c+this.ganttChart.heightTaskItem)+"px"});
_8.add(_1a,"ganttTaskLineHorizontal");
_a.set(_1a,{width:(15+(_1d-(_1f+_1b)))+"px",left:(_1b+_1f-20)+"px",top:(_1e+2)+"px"});
_8.add(_18,"ganttTaskArrowImg");
_a.set(_18,{left:(_1d-7)+"px",top:(_1e-1)+"px"});
}else{
_8.add(_19,"ganttTaskLineVerticalRightPlus");
_a.set(_19,{height:(_1c+2-_1e)+"px",width:"1px",left:(_1b+_1f-20)+"px",top:(_1e+2)+"px"});
_8.add(_1a,"ganttTaskLineHorizontalPlus");
_a.set(_1a,{width:(15+(_1d-(_1f+_1b)))+"px",left:(_1b+_1f-20)+"px",top:(_1e+2)+"px"});
_8.add(_18,"ganttTaskArrowImgPlus");
_a.set(_18,{left:(_1d-7)+"px",top:(_1e-1)+"px"});
}
_16.appendChild(_19);
_16.appendChild(_1a);
_16.appendChild(_18);
_17.push(_19);
_17.push(_18);
_17.push(_1a);
return _17;
},showChildTasks:function(_20,_21){
if(_21){
for(var i=0;i<_20.childTask.length;i++){
var _22=_20.childTask[i],_23=_22.cTaskItem[0],_24=_22.cTaskNameItem[0],_25=_22.cTaskItem[1],_26=_22.cTaskNameItem[1],_27=_22.cTaskNameItem[2];
if(_23.style.display=="none"){
_23.style.display="inline";
_24.style.display="inline";
_22.showDescTask();
_20.isHide=false;
if(_27){
_27.style.display="inline";
_21=_22.isExpanded;
}
for(var k=0;k<_25.length;k++){
_25[k].style.display="inline";
}
for(var k=0;k<_26.length;k++){
_26[k].style.display="inline";
}
(_22.taskIdentifier)&&(_22.taskIdentifier.style.display="inline");
this.hideTasksHeight+=this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
if(_22.childTask.length>0){
this.showChildTasks(_22,_21);
}
}
}
}
},hideChildTasks:function(_28){
for(var i=0;i<_28.childTask.length;i++){
var _29=_28.childTask[i],_2a=_29.cTaskItem[0],_2b=_29.cTaskNameItem[0],_2c=_29.cTaskItem[1],_2d=_29.cTaskNameItem[1],_2e=_29.cTaskNameItem[2];
if(_2a.style.display!="none"){
_2a.style.display="none";
_2b.style.display="none";
_29.hideDescTask();
_28.isHide=true;
if(_2e){
_2e.style.display="none";
}
for(var k=0;k<_2c.length;k++){
_2c[k].style.display="none";
}
for(var k=0;k<_2d.length;k++){
_2d[k].style.display="none";
}
(_29.taskIdentifier)&&(_29.taskIdentifier.style.display="none");
this.hideTasksHeight+=(this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra);
if(_29.childTask.length>0){
this.hideChildTasks(_29);
}
}
}
},shiftCurrentTasks:function(_2f,_30){
this.shiftNextTask(this,_30);
_2f.project.shiftNextProject(_2f.project,_30);
},shiftTask:function(_31,_32){
_31.posY=_31.posY+_32;
var _33=_31.cTaskItem[0],_34=_31.cTaskNameItem[0],_35=_31.cTaskItem[1],_36=_31.cTaskNameItem[1],_37=_31.cTaskNameItem[2];
_34.style.top=parseInt(_34.style.top)+_32+"px";
if(_37){
_37.style.top=parseInt(_37.style.top)+_32+"px";
}
if(_31.parentTask){
if(parseInt(this.cTaskNameItem[0].style.top)>parseInt(_31.parentTask.cTaskNameItem[0].style.top)&&(_36[0].style.display!="none")){
_36[0].style.height=parseInt(_36[0].style.height)+_32+"px";
}else{
_36[0].style.top=parseInt(_36[0].style.top)+_32+"px";
}
_36[1].style.top=parseInt(_36[1].style.top)+_32+"px";
}
_33.style.top=parseInt(_33.style.top)+_32+"px";
_31.descrTask.style.top=parseInt(_31.descrTask.style.top)+_32+"px";
if(_31.predTask){
if(((parseInt(this.cTaskItem[0].style.top)>parseInt(_31.predTask.cTaskItem[0].style.top))||(this.cTaskItem[0].id==_31.predTask.taskItem.id))&&_35[0].style.display!="none"){
_35[0].style.height=parseInt(_35[0].style.height)+_32+"px";
}else{
_35[0].style.top=parseInt(_35[0].style.top)+_32+"px";
}
_35[1].style.top=parseInt(_35[1].style.top)+_32+"px";
_35[2].style.top=parseInt(_35[2].style.top)+_32+"px";
}
},shiftNextTask:function(_38,_39){
if(_38.nextChildTask){
this.shiftTask(_38.nextChildTask,_39);
this.shiftChildTask(_38.nextChildTask,_39);
this.shiftNextTask(_38.nextChildTask,_39);
}else{
if(_38.parentTask){
this.shiftNextTask(_38.parentTask,_39);
}else{
if(_38.nextParentTask){
this.shiftTask(_38.nextParentTask,_39);
this.shiftChildTask(_38.nextParentTask,_39);
this.shiftNextTask(_38.nextParentTask,_39);
}
}
}
},shiftChildTask:function(_3a,_3b){
_3.forEach(_3a.childTask,function(_3c){
this.shiftTask(_3c,_3b);
if(_3c.childTask.length>0){
this.shiftChildTask(_3c,_3b);
}
},this);
},endMove:function(){
var _3d=this.cTaskItem[0];
var _3e=_a.get(_3d,"left")-this.posX;
var _3f=this.getDateOnPosition(_a.get(_3d,"left"));
_3f=this.checkPos(_3f);
if(this.checkMove){
_3e=this.ganttChart.getPosOnDate(_3f)-this.posX;
this.moveCurrentTaskItem(_3e,this.moveChild);
this.project.shiftProjectItem();
}
this.checkMove=false;
this.posX=0;
this.maxPosXMove=-1;
this.minPosXMove=-1;
_3d.childNodes[1].firstChild.rows[0].cells[0].innerHTML="";
this.adjustPanelTime();
if(this.ganttChart.resource){
this.ganttChart.resource.refresh();
}
},checkPos:function(_40){
var _41=this.cTaskItem[0];
var h=_40.getHours();
if(h>=12){
_40.setDate(_40.getDate()+1);
_40.setHours(0);
if((parseInt(_41.firstChild.firstChild.width)+this.ganttChart.getPosOnDate(_40)>this.maxPosXMove)&&(this.maxPosXMove!=-1)){
_40.setDate(_40.getDate()-1);
_40.setHours(0);
}
}else{
if((h<12)&&(h!=0)){
_40.setHours(0);
if((this.ganttChart.getPosOnDate(_40)<this.minPosXMove)){
_40.setDate(_40.getDate()+1);
}
}
}
_41.style.left=this.ganttChart.getPosOnDate(_40)+"px";
return _40;
},getMaxPosPredChildTaskItem:function(){
var _42=0;
var _43=0;
for(var i=0;i<this.childPredTask.length;i++){
_43=this.getMaxPosPredChildTaskItemInTree(this.childPredTask[i]);
if(_43>_42){
_42=_43;
}
}
return _42;
},getMaxPosPredChildTaskItemInTree:function(_44){
var _45=_44.cTaskItem[0];
var _46=parseInt(_45.firstChild.firstChild.width)+_a.get(_45,"left");
var _47=0;
var _48=0;
_3.forEach(_44.childPredTask,function(_49){
_48=this.getMaxPosPredChildTaskItemInTree(_49);
if(_48>_47){
_47=_48;
}
},this);
return _47>_46?_47:_46;
},moveCurrentTaskItem:function(_4a,_4b){
var _4c=this.cTaskItem[0];
this.taskItem.startTime=new Date(this.ganttChart.startDate);
this.taskItem.startTime.setHours(this.taskItem.startTime.getHours()+(parseInt(_4c.style.left)/this.ganttChart.pixelsPerHour));
this.showDescTask();
var _4d=this.cTaskItem[1];
if(_4d.length>0){
_4d[2].style.width=parseInt(_4d[2].style.width)+_4a+"px";
_4d[1].style.left=parseInt(_4d[1].style.left)+_4a+"px";
}
_3.forEach(this.childTask,function(_4e){
if(!_4e.predTask){
this.moveChildTaskItems(_4e,_4a,_4b);
}
},this);
_3.forEach(this.childPredTask,function(_4f){
this.moveChildTaskItems(_4f,_4a,_4b);
},this);
},moveChildTaskItems:function(_50,_51,_52){
var _53=_50.cTaskItem[0];
if(_52){
_53.style.left=parseInt(_53.style.left)+_51+"px";
_50.adjustPanelTime();
_50.taskItem.startTime=new Date(this.ganttChart.startDate);
_50.taskItem.startTime.setHours(_50.taskItem.startTime.getHours()+(parseInt(_53.style.left)/this.ganttChart.pixelsPerHour));
var _54=_50.cTaskItem[1];
_3.forEach(_54,function(_55){
_55.style.left=parseInt(_55.style.left)+_51+"px";
},this);
_3.forEach(_50.childTask,function(_56){
if(!_56.predTask){
this.moveChildTaskItems(_56,_51,_52);
}
},this);
_3.forEach(_50.childPredTask,function(_57){
this.moveChildTaskItems(_57,_51,_52);
},this);
}else{
var _54=_50.cTaskItem[1];
if(_54.length>0){
var _58=_54[0],_59=_54[2];
_59.style.left=parseInt(_59.style.left)+_51+"px";
_59.style.width=parseInt(_59.style.width)-_51+"px";
_58.style.left=parseInt(_58.style.left)+_51+"px";
}
}
_50.moveDescTask();
},adjustPanelTime:function(){
var _5a=this.cTaskItem[0];
var _5b=parseInt(_5a.style.left)+parseInt(_5a.firstChild.firstChild.width)+this.ganttChart.panelTimeExpandDelta;
_5b+=this.descrTask.offsetWidth;
this.ganttChart.adjustPanelTime(_5b);
},getDateOnPosition:function(_5c){
var _5d=new Date(this.ganttChart.startDate);
_5d.setHours(_5d.getHours()+(_5c/this.ganttChart.pixelsPerHour));
return _5d;
},moveItem:function(_5e){
var _5f=_5e.screenX;
var _60=(this.posX+(_5f-this.mouseX));
var _61=parseInt(this.cTaskItem[0].childNodes[0].firstChild.width);
var _62=_60+_61;
if(this.checkMove){
if(((this.minPosXMove<=_60))&&((_62<=this.maxPosXMove)||(this.maxPosXMove==-1))){
this.moveTaskItem(_60);
}
}
},moveTaskItem:function(_63){
var _64=this.cTaskItem[0];
_64.style.left=_63+"px";
_64.childNodes[1].firstChild.rows[0].cells[0].innerHTML=this.getDateOnPosition(_63).getDate()+"."+(this.getDateOnPosition(_63).getMonth()+1)+"."+this.getDateOnPosition(_63).getUTCFullYear();
},resizeItem:function(_65){
if(this.checkResize){
var _66=_65.screenX;
var _67=this.taskItemWidth+(_66-this.mouseX);
if(_67>=this.taskItemWidth){
if((_67<=this.maxWidthResize)||(this.maxWidthResize==-1)){
this.resizeTaskItem(_67);
}else{
if((this.maxWidthResize!=-1)&&(_67>this.maxWidthResize)){
this.resizeTaskItem(this.maxWidthResize);
}
}
}else{
if(_67<=this.taskItemWidth){
if(_67>=this.minWidthResize){
this.resizeTaskItem(_67);
}else{
if(_67<this.minWidthResize){
this.resizeTaskItem(this.minWidthResize);
}
}
}
}
}
},resizeTaskItem:function(_68){
var _69=this.cTaskItem[0];
var _6a=Math.round(_68/this.ganttChart.pixelsPerWorkHour);
var _6b=_69.childNodes[0].firstChild.rows[0],rc0=_6b.cells[0],rc1=_6b.cells[1];
rc0&&(rc0.firstChild.style.width=parseInt(rc0.width)*_68/100+"px");
rc1&&(rc1.firstChild.style.width=parseInt(rc1.width)*_68/100+"px");
_69.childNodes[0].firstChild.width=_68+"px";
_69.childNodes[1].firstChild.width=_68+"px";
this.cTaskItem[0].childNodes[1].firstChild.rows[0].cells[0].innerHTML=_6a;
var _6c=_69.childNodes[2];
_6c.childNodes[0].style.width=_68+"px";
_6c.childNodes[1].style.left=_68-10+"px";
},endResizeItem:function(){
var _6d=this.cTaskItem[0];
if((this.taskItemWidth!=parseInt(_6d.childNodes[0].firstChild.width))){
var _6e=_6d.offsetLeft;
var _6f=_6d.offsetLeft+parseInt(_6d.childNodes[0].firstChild.width);
this.taskItem.duration=Math.round((_6f-_6e)/this.ganttChart.pixelsPerWorkHour);
if(this.childPredTask.length>0){
for(var j=0;j<this.childPredTask.length;j++){
var _70=this.childPredTask[j].cTaskItem[1],_71=_70[0],_72=_70[2],_73=_6d.childNodes[0];
_72.style.width=parseInt(_72.style.width)-(parseInt(_73.firstChild.width)-this.taskItemWidth)+"px";
_72.style.left=parseInt(_72.style.left)+(parseInt(_73.firstChild.width)-this.taskItemWidth)+"px";
_71.style.left=parseInt(_71.style.left)+(parseInt(_73.firstChild.width)-this.taskItemWidth)+"px";
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
},startMove:function(_74){
this.moveChild=_74.ctrlKey;
this.mouseX=_74.screenX;
this.getMoveInfo();
this.checkMove=true;
this.hideDescTask();
},showDescTask:function(){
var _75=(parseInt(this.cTaskItem[0].style.left)+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrTask.style.left=_75+"px";
this.descrTask.innerHTML=this.objKeyToStr(this.getTaskOwner());
this.descrTask.style.visibility="visible";
},hideDescTask:function(){
_a.set(this.descrTask,"visibility","hidden");
},buildResourceInfo:function(_76){
if(this.childTask&&this.childTask.length>0){
for(var i=0;i<this.childTask.length;i++){
var _77=this.childTask[i];
_77.buildResourceInfo(_76);
}
}
if(_4.trim(this.taskItem.taskOwner).length>0){
var _78=this.taskItem.taskOwner.split(";");
for(var i=0;i<_78.length;i++){
var o=_78[i];
if(_4.trim(o).length<=0){
continue;
}
_76[o]?(_76[o].push(this)):(_76[o]=[this]);
}
}
},objKeyToStr:function(obj,_79){
var _7a="";
_79=_79||" ";
if(obj){
for(var key in obj){
_7a+=_79+key;
}
}
return _7a;
},getTaskOwner:function(){
var _7b={};
if(_4.trim(this.taskItem.taskOwner).length>0){
var _7c=this.taskItem.taskOwner.split(";");
for(var i=0;i<_7c.length;i++){
var o=_7c[i];
_7b[o]=1;
}
}
_3.forEach(this.childTask,function(_7d){
_4.mixin(_7b,_7d.getTaskOwner());
},this);
return _7b;
},moveDescTask:function(){
var _7e=(parseInt(this.cTaskItem[0].style.left)+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
this.descrTask.style.left=_7e+"px";
},getMoveInfo:function(){
this.posX=parseInt(this.cTaskItem[0].style.left);
var _7f=parseInt(this.cTaskItem[0].childNodes[0].firstChild.width);
var _80=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].style.left);
var _81=!this.predTask?0:parseInt(this.predTask.cTaskItem[0].style.left)+parseInt(this.predTask.cTaskItem[0].childNodes[0].firstChild.width);
var _82=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].childNodes[0].firstChild.width);
var _83=0;
var _84=0;
var _85=0;
if(this.childPredTask.length>0){
var _86=null;
_3.forEach(this.childPredTask,function(_87){
if((!_86)||((_86)&&(_86>parseInt(_87.cTaskItem[0].style.left)))){
_86=parseInt(_87.cTaskItem[0].style.left);
}
},this);
_83=_86;
}
if(this.childTask.length>0){
var _88=null;
_3.forEach(this.childTask,function(_89){
if((!_88)||((_88)&&(_88>(parseInt(_89.cTaskItem[0].style.left))))){
_88=parseInt(_89.cTaskItem[0].style.left);
}
},this);
_85=_88;
var _86=null;
_3.forEach(this.childTask,function(_8a){
if((!_86)||((_86)&&(_86<(parseInt(_8a.cTaskItem[0].style.left)+parseInt(_8a.cTaskItem[0].firstChild.firstChild.width))))){
_86=parseInt(_8a.cTaskItem[0].style.left)+parseInt(_8a.cTaskItem[0].firstChild.firstChild.width);
}
},this);
_84=_86;
}
if(!this.moveChild){
if(this.childPredTask.length>0){
if(this.maxPosXMove<_83){
this.maxPosXMove=_83;
}
}
if(this.childTask.length>0){
if((this.childPredTask.length>0)&&(this.maxPosXMove-_7f)>_85){
this.maxPosXMove=this.maxPosXMove-((this.maxPosXMove-_7f)-_85);
}
if(!(this.childPredTask.length>0)){
this.maxPosXMove=_85+_7f;
}
this.minPosXMove=(_84-_7f);
}
if(_80>0){
if((!(this.childPredTask.length>0))&&(this.childTask.length>0)){
if(this.maxPosXMove>_80+_82){
this.maxPosXMove=_80+_82;
}
}
if(this.minPosXMove<=_80){
this.minPosXMove=_80;
}
if((!(this.childTask.length>0))&&(!(this.childPredTask.length>0))){
this.maxPosXMove=_80+_82;
}else{
if((!(this.childTask.length>0))&&(this.childPredTask.length>0)){
if((_80+_82)>_81){
this.maxPosXMove=_83;
}
}
}
}
if(_81>0){
if(this.minPosXMove<=_81){
this.minPosXMove=_81;
}
}
if((_81==0)&&(_80==0)){
if(this.minPosXMove<=this.ganttChart.initialPos){
this.minPosXMove=this.ganttChart.initialPos;
}
}
}else{
if((_80>0)&&(_81==0)){
this.minPosXMove=_80;
this.maxPosXMove=_80+_82;
}else{
if((_80==0)&&(_81==0)){
this.minPosXMove=this.ganttChart.initialPos;
this.maxPosXMove=-1;
}else{
if((_80>0)&&(_81>0)){
this.minPosXMove=_81;
this.maxPosXMove=_80+_82;
}else{
if((_80==0)&&(_81>0)){
this.minPosXMove=_81;
this.maxPosXMove=-1;
}
}
}
}
if((this.parentTask)&&(this.childPredTask.length>0)){
var _86=this.getMaxPosPredChildTaskItem(this);
var _80=parseInt(this.parentTask.cTaskItem[0].style.left)+parseInt(this.parentTask.cTaskItem[0].firstChild.firstChild.width);
this.maxPosXMove=this.posX+_7f+_80-_86;
}
}
},startResize:function(_8b){
this.mouseX=_8b.screenX;
this.getResizeInfo();
this.hideDescTask();
this.checkResize=true;
this.taskItemWidth=parseInt(this.cTaskItem[0].firstChild.firstChild.width);
},getResizeInfo:function(){
var _8c=this.cTaskItem[0];
var _8d=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].style.left);
var _8e=!this.parentTask?0:parseInt(this.parentTask.cTaskItem[0].childNodes[0].firstChild.width);
var _8f=parseInt(_8c.style.left);
var _90=0;
var _91=0;
if(this.childPredTask.length>0){
var _92=null;
_3.forEach(this.childPredTask,function(_93){
if((!_92)||((_92)&&(_92>parseInt(_93.cTaskItem[0].style.left)))){
_92=parseInt(_93.cTaskItem[0].style.left);
}
},this);
_90=_92;
}
if(this.childTask.length>0){
var _92=null;
_3.forEach(this.childTask,function(_94){
if((!_92)||((_92)&&(_92<(parseInt(_94.cTaskItem[0].style.left)+parseInt(_94.cTaskItem[0].firstChild.firstChild.width))))){
_92=parseInt(_94.cTaskItem[0].style.left)+parseInt(_94.cTaskItem[0].firstChild.firstChild.width);
}
},this);
_91=_92;
}
this.minWidthResize=this.ganttChart.pixelsPerDay;
if(this.childTask.length>0){
this.minWidthResize=_91-_8f;
}
if((this.childPredTask.length>0)&&(!this.parentTask)){
this.maxWidthResize=_90-_8f;
}else{
if((this.childPredTask.length>0)&&(this.parentTask)){
var w1=_8d+_8e-_8f;
var w2=_90-_8f;
this.maxWidthResize=Math.min(w1,w2);
}else{
if((this.childPredTask.length==0)&&(this.parentTask)){
this.maxWidthResize=_8d+_8e-_8f;
}
}
}
},createTaskItem:function(){
this.posX=this.ganttChart.getPosOnDate(this.taskItem.startTime);
var _95=_9.create("div",{id:this.taskItem.id,className:"ganttTaskItemControl"});
_a.set(_95,{left:this.posX+"px",top:this.posY+"px"});
var _96=_9.create("div",{className:"ganttTaskDivTaskItem"},_95);
var _97=_9.create("table",{cellPadding:"0",cellSpacing:"0",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",className:"ganttTaskTblTaskItem"},_96);
var _98=_97.insertRow(_97.rows.length);
if(this.taskItem.percentage!=0){
var _99=_9.create("td",{height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.percentage+"%"},_98);
_99.style.lineHeight="1px";
var _9a=_9.create("div",{className:"ganttImageTaskProgressFilled"},_99);
_a.set(_9a,{width:(this.taskItem.percentage*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.taskItem.percentage!=100){
var _99=_9.create("td",{height:this.ganttChart.heightTaskItem+"px",width:(100-this.taskItem.percentage)+"%"},_98);
_99.style.lineHeight="1px";
var _9b=_9.create("div",{className:"ganttImageTaskProgressBg"},_99);
_a.set(_9b,{width:((100-this.taskItem.percentage)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.ganttChart.isContentEditable){
var _9c=_9.create("div",{className:"ganttTaskDivTaskInfo"},_95);
var _9d=_9.create("table",{cellPadding:"0",cellSpacing:"0",height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"},_9c);
var _9e=_9d.insertRow(0);
var _9f=_9.create("td",{align:"center",vAlign:"top",height:this.ganttChart.heightTaskItem+"px",className:"ganttMoveInfo"},_9e);
var _a0=_9.create("div",{className:"ganttTaskDivTaskName"},_95);
var _a1=_9.create("div",{},_a0);
_9.create("input",{className:"ganttTaskDivMoveInput",type:"text"},_a1);
_a.set(_a1,{background:"#000000",opacity:0});
_a.set(_a1,{height:this.ganttChart.heightTaskItem+"px",width:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
var _a2=_9.create("div",{className:"ganttTaskDivResize"},_a0);
_9.create("input",{className:"ganttTaskDivResizeInput",type:"text"},_a2);
_a.set(_a2,{left:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour-10)+"px",height:this.ganttChart.heightTaskItem+"px",width:"10px"});
this.ganttChart._events.push(on(_a1,"mousedown",_4.hitch(this,function(_a3){
this.moveMoveConn=on(document,"mousemove",_4.hitch(this,function(e){
this.checkMove&&this.moveItem(e);
}));
this.moveUpConn=on(document,"mouseup",_4.hitch(this,function(){
if(this.checkMove){
this.endMove();
this.ganttChart.isMoving=false;
document.body.releaseCapture&&document.body.releaseCapture();
this.moveMoveConn.remove();
this.moveUpConn.remove();
}
}));
this.startMove(_a3);
this.ganttChart.isMoving=true;
document.body.setCapture&&document.body.setCapture(false);
})));
this.ganttChart._events.push(on(_a1,"mouseover",_4.hitch(this,function(_a4){
_a4.target&&(_a4.target.style.cursor="move");
})));
this.ganttChart._events.push(on(_a1,"mouseout",_4.hitch(this,function(_a5){
_a5.target.style.cursor="";
})));
this.ganttChart._events.push(on(_a2,"mousedown",_4.hitch(this,function(_a6){
this.resizeMoveConn=on(document,"mousemove",_4.hitch(this,function(e){
this.checkResize&&this.resizeItem(e);
}));
this.resizeUpConn=on(document,"mouseup",_4.hitch(this,function(){
if(this.checkResize){
this.endResizeItem();
this.ganttChart.isResizing=false;
document.body.releaseCapture&&document.body.releaseCapture();
this.resizeMoveConn.remove();
this.resizeUpConn.remove();
}
}));
this.startResize(_a6);
this.ganttChart.isResizing=true;
document.body.setCapture&&document.body.setCapture(false);
})));
this.ganttChart._events.push(on(_a2,"mouseover",_4.hitch(this,function(_a7){
(!this.ganttChart.isMoving)&&(!this.ganttChart.isResizing)&&_a7.target&&(_a7.target.style.cursor="e-resize");
})));
this.ganttChart._events.push(on(_a2,"mouseout",_4.hitch(this,function(_a8){
!this.checkResize&&_a8.target&&(_a8.target.style.cursor="");
})));
}
return _95;
},createTaskNameItem:function(){
var _a9=_9.create("div",{id:this.taskItem.id,className:"ganttTaskTaskNameItem",title:this.taskItem.name+", id: "+this.taskItem.id+" ",innerHTML:this.taskItem.name});
_a.set(_a9,"top",this.posY+"px");
_b.set(_a9,"tabIndex",0);
if(this.ganttChart.isShowConMenu){
this.ganttChart._events.push(on(_a9,"mouseover",_4.hitch(this,function(_aa){
_8.add(_a9,"ganttTaskTaskNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_aa.target,this);
})));
this.ganttChart._events.push(on(_a9,"keydown",_4.hitch(this,function(_ab){
if(_ab.keyCode==_d.ENTER){
this.ganttChart.tabMenu.clear();
this.ganttChart.tabMenu.show(_ab.target,this);
}
if(this.ganttChart.tabMenu.isShow&&(_ab.keyCode==_d.LEFT_ARROW||_ab.keyCode==_d.RIGHT_ARROW)){
_1(this.ganttChart.tabMenu.menuPanel.firstChild.rows[0].cells[0]);
}
if(this.ganttChart.tabMenu.isShow&&_ab.keyCode==_d.ESCAPE){
this.ganttChart.tabMenu.hide();
}
})));
this.ganttChart._events.push(on(_a9,"mouseout",_4.hitch(this,function(){
_8.remove(_a9,"ganttTaskTaskNameItemHover");
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_4.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"mouseover",_4.hitch(this,function(){
clearTimeout(this.ganttChart.menuTimer);
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"keydown",_4.hitch(this,function(_ac){
if(this.ganttChart.tabMenu.isShow&&_ac.keyCode==_d.ESCAPE){
this.ganttChart.tabMenu.hide();
}
})));
this.ganttChart._events.push(on(this.ganttChart.tabMenu.menuPanel,"mouseout",_4.hitch(this,function(){
clearTimeout(this.ganttChart.menuTimer);
this.ganttChart.menuTimer=setTimeout(_4.hitch(this,function(){
this.ganttChart.tabMenu.hide();
}),200);
})));
}
return _a9;
},createTaskDescItem:function(){
var _ad=(this.posX+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
var _ae=_9.create("div",{innerHTML:this.objKeyToStr(this.getTaskOwner()),className:"ganttTaskDescTask"});
_a.set(_ae,{left:_ad+"px",top:this.posY+"px"});
return this.descrTask=_ae;
},checkWidthTaskNameItem:function(){
if(this.cTaskNameItem[0].offsetWidth+this.cTaskNameItem[0].offsetLeft>this.ganttChart.maxWidthTaskNames){
var _af=this.cTaskNameItem[0].offsetWidth+this.cTaskNameItem[0].offsetLeft-this.ganttChart.maxWidthTaskNames;
var _b0=Math.round(_af/(this.cTaskNameItem[0].offsetWidth/this.cTaskNameItem[0].firstChild.length));
var _b1=this.taskItem.name.substring(0,this.cTaskNameItem[0].firstChild.length-_b0-3);
_b1+="...";
this.cTaskNameItem[0].innerHTML=_b1;
}
},refreshTaskItem:function(_b2){
this.posX=this.ganttChart.getPosOnDate(this.taskItem.startTime);
_a.set(_b2,{"left":this.posX+"px"});
var _b3=_b2.childNodes[0];
var _b4=_b3.firstChild;
_b4.width=(!this.taskItem.duration?1:this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)+"px";
var _b5=_b4.rows[0];
if(this.taskItem.percentage!=0){
var _b6=_b5.firstChild;
_b6.height=this.ganttChart.heightTaskItem+"px";
_b6.width=this.taskItem.percentage+"%";
_b6.style.lineHeight="1px";
var _b7=_b6.firstChild;
_a.set(_b7,{width:(!this.taskItem.duration?1:(this.taskItem.percentage*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.taskItem.percentage!=100){
var _b6=_b5.lastChild;
_b6.height=this.ganttChart.heightTaskItem+"px";
_b6.width=(100-this.taskItem.percentage)+"%";
_b6.style.lineHeight="1px";
var _b8=_b6.firstChild;
_a.set(_b8,{width:(!this.taskItem.duration?1:((100-this.taskItem.percentage)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour/100))+"px",height:this.ganttChart.heightTaskItem+"px"});
}
if(this.ganttChart.isContentEditable){
var _b9=_b2.childNodes[1];
var _ba=_b9.firstChild;
_ba.height=this.ganttChart.heightTaskItem+"px";
_ba.width=(!this.taskItem.duration?1:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour))+"px";
var _bb=_ba.rows[0];
var _bc=_bb.firstChild;
_bc.height=this.ganttChart.heightTaskItem+"px";
var _bd=_b2.childNodes[2];
var _be=_bd.firstChild;
_be.style.height=this.ganttChart.heightTaskItem+"px";
_be.style.width=(!this.taskItem.duration?1:(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour))+"px";
var _bf=_bd.lastChild;
_a.set(_bf,{"left":(this.taskItem.duration*this.ganttChart.pixelsPerWorkHour-10)+"px"});
_bf.style.height=this.ganttChart.heightTaskItem+"px";
_bf.style.width="10px";
}
return _b2;
},refreshTaskDesc:function(_c0){
var _c1=(this.posX+this.taskItem.duration*this.ganttChart.pixelsPerWorkHour+10);
_a.set(_c0,{"left":_c1+"px"});
return _c0;
},refreshConnectingLinesDS:function(_c2){
var _c3=_c2[1];
var _c4=_c2[0];
var _c5=_c2[2];
var _c6=_a.get(this.predTask.cTaskItem[0],"left");
var _c7=_a.get(this.predTask.cTaskItem[0],"top");
var _c8=_a.get(this.cTaskItem[0],"left");
var _c9=this.posY+2;
var _ca=parseInt(this.predTask.cTaskItem[0].firstChild.firstChild.width);
if(_c7<_c9){
_a.set(_c4,{"height":(_c9-this.ganttChart.heightTaskItem/2-_c7-3)+"px","left":(_c6+_ca-20)+"px"});
_a.set(_c5,{"width":(15+(_c8-(_ca+_c6)))+"px","left":(_c6+_ca-20)+"px"});
_a.set(_c3,{"left":(_c8-7)+"px"});
}else{
_a.set(_c4,{"height":(_c7+2-_c9)+"px","left":(_c6+_ca-20)+"px"});
_a.set(_c5,{"width":(15+(_c8-(_ca+_c6)))+"px","left":(_c6+_ca-20)+"px"});
_a.set(_c3,{"left":(_c8-7)+"px"});
}
return _c2;
},postLoadData:function(){
},refresh:function(){
if(this.childTask&&this.childTask.length>0){
_3.forEach(this.childTask,function(_cb){
_cb.refresh();
},this);
}
this.refreshTaskItem(this.cTaskItem[0]);
this.refreshTaskDesc(this.cTaskItem[0].nextSibling);
if(this.taskItem.previousTask&&this.predTask){
this.refreshConnectingLinesDS(this.cTaskItem[1]);
}
return this;
},create:function(){
var _cc=this.ganttChart.contentData.firstChild;
var _cd=this.taskItem.previousTask;
var _ce=this.taskItem.parentTask;
var _cf=(this.taskItem.cldTasks.length>0);
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
this.posY=_a.get(_d0.cTaskItem[0],"top")+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
this.previousChildTask.nextChildTask=this;
}else{
this.posY=_a.get(_d1.cTaskItem[0],"top")+this.ganttChart.heightTaskItem+this.ganttChart.heightTaskItemExtra;
}
_d1.childTask.push(this);
}
if(_cd){
var _d1=this.project.getTaskById(_cd.id);
this.predTask=_d1;
_d1.childPredTask.push(this);
}
this.cTaskItem.push(this.createTaskItem());
_cc.appendChild(this.cTaskItem[0]);
if(this.ganttChart.panelNames){
this.cTaskNameItem.push(this.createTaskNameItem());
this.ganttChart.panelNames.firstChild.appendChild(this.cTaskNameItem[0]);
}
_cc.appendChild(this.createTaskDescItem());
var _d2=[];
if(_cd){
_d2=this.createConnectingLinesDS();
}
this.cTaskItem.push(_d2);
if(this.ganttChart.panelNames){
var _d3=[];
if(_ce){
this.cTaskNameItem[0].style.left=_a.get(this.parentTask.cTaskNameItem[0],"left")+15+"px";
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
var pos=_c.getMarginBox(this.cTaskNameItem[0],true);
if(this.taskIdentifier){
if(this.childTask&&this.childTask.length>0){
_3.forEach(this.childTask,function(_d5){
_d5.checkPosition();
},this);
}
_a.set(this.taskIdentifier,{"left":(pos.l+pos.w+4)+"px","top":(pos.t-1)+"px"});
}else{
this.taskIdentifier=_9.create("div",{id:"TaskId_"+this.taskItem.id,className:"ganttTaskIdentifier",title:this.taskItem.id,innerHTML:this.taskItem.id},this.cTaskNameItem[0].parentNode);
_a.set(this.taskIdentifier,{left:(pos.l+pos.w+4)+"px",top:(pos.t-1)+"px"});
}
},createTreeImg:function(){
var _d6=_9.create("div",{id:this.taskItem.id,className:"ganttImageTreeCollapse"});
_b.set(_d6,"tabIndex",0);
_3.forEach(["click","keydown"],function(e){
this.ganttChart._events.push(on(_d6,e,_4.hitch(this,function(evt){
if(e=="keydown"&&evt.keyCode!=_d.ENTER){
return;
}
if(this.isExpanded){
_8.remove(_d6,"ganttImageTreeCollapse");
_8.add(_d6,"ganttImageTreeExpand");
this.isExpanded=false;
this.hideChildTasks(this);
this.shiftCurrentTasks(this,-this.hideTasksHeight);
this.ganttChart.checkPosition();
}else{
_8.remove(_d6,"ganttImageTreeExpand");
_8.add(_d6,"ganttImageTreeCollapse");
this.isExpanded=true;
this.shiftCurrentTasks(this,this.hideTasksHeight);
this.showChildTasks(this,true);
this.hideTasksHeight=0;
this.ganttChart.checkPosition();
}
})));
},this);
this.ganttChart.panelNames.firstChild.appendChild(_d6);
_8.add(_d6,"ganttTaskTreeImage");
_a.set(_d6,{left:(_a.get(this.cTaskNameItem[0],"left")-12)+"px",top:(_a.get(this.cTaskNameItem[0],"top")+3)+"px"});
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
var _e6=_9.create("td",{height:this.ganttChart.heightTaskItem+"px",width:_e4+"%"},_e5);
_e6.style.lineHeight="1px";
var _e7=_9.create("div",{className:"ganttImageTaskProgressFilled"},_e6);
_a.set(_e7,{width:(_e4*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
_e6=_9.create("td",{height:this.ganttChart.heightTaskItem+"px",width:(100-_e4)+"%"},_e5);
_e6.style.lineHeight="1px";
_e7=_9.create("div",{className:"ganttImageTaskProgressBg"},_e6);
_a.set(_e7,{width:((100-_e4)*this.taskItem.duration*this.ganttChart.pixelsPerWorkHour)/100+"px",height:this.ganttChart.heightTaskItem+"px"});
}
}
}else{
if(_e4==0){
if((this.taskItem.percentage!=0)&&(this.taskItem.percentage!=100)){
rc0.parentNode.removeChild(rc0);
rc1.width=100+"%";
}else{
_8.remove(rc0.firstChild,"ganttImageTaskProgressFilled");
_8.add(rc0.firstChild,"ganttImageTaskProgressBg");
}
}else{
if(_e4==100){
if((this.taskItem.percentage!=0)&&(this.taskItem.percentage!=100)){
rc1.parentNode.removeChild(rc1);
rc0.width=100+"%";
}else{
_8.remove(rc0.firstChild,"ganttImageTaskProgressBg");
_8.add(rc0.firstChild,"ganttImageTaskProgressFilled");
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
});
