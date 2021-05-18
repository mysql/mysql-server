//>>built
define("dojox/gantt/GanttProjectItem",["./GanttTaskItem","dojo/_base/declare","./GanttProjectControl","dojo/domReady!"],function(_1,_2){
return _2("dojox.gantt.GanttProjectItem",[_1],{constructor:function(_3){
this.id=_3.id;
this.name=_3.name||this.id;
this.startDate=_3.startDate||new Date();
this.parentTasks=[];
},getTaskById:function(id){
for(var i=0;i<this.parentTasks.length;i++){
var _4=this.parentTasks[i];
var _5=this.getTaskByIdInTree(_4,id);
if(_5){
return _5;
}
}
return null;
},getTaskByIdInTree:function(_6,id){
if(_6.id==id){
return _6;
}else{
for(var i=0;i<_6.cldTasks.length;i++){
var _7=_6.cldTasks[i];
if(_7.id==id){
return _7;
}
if(_7.cldTasks.length>0){
if(_7.cldTasks.length>0){
var _8=this.getTaskByIdInTree(_7,id);
if(_8){
return _8;
}
}
}
}
}
return null;
},addTask:function(_9){
this.parentTasks.push(_9);
_9.setProject(this);
},deleteTask:function(id){
var _a=this.getTaskById(id);
if(!_a){
return;
}
if(!_a.parentTask){
for(var i=0;i<this.parentTasks.length;i++){
var _b=this.parentTasks[i];
if(_b.id==id){
if(_b.nextParentTask){
if(_b.previousParentTask){
_b.previousParentTask.nextParentTask=_b.nextParentTask;
_b.nextParentTask.previousParentTask=_b.previousParentTask;
}else{
_b.nextParentTask.previousParentTask=null;
}
}else{
if(_b.previousParentTask){
_b.previousParentTask.nextParentTask=null;
}
}
_b=null;
this.parentTasks.splice(i,1);
break;
}
}
}else{
var _c=_a.parentTask;
for(var i=0;i<_c.cldTasks.length;i++){
var _d=_c.cldTasks[i];
if(_d.id==id){
if(_d.nextChildTask){
if(_d.previousChildTask){
_d.previousChildTask.nextChildTask=_d.nextChildTask;
_d.nextChildTask.previousChildTask=_d.previousChildTask;
}else{
_d.nextChildTask.previousChildTask=null;
}
}else{
if(_d.previousChildTask){
_d.previousChildTask.nextChildTask=null;
}
}
_d=null;
_c.cldTasks.splice(i,1);
break;
}
}
}
}});
});
