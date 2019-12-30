//>>built
define("dojox/gantt/GanttTaskItem",["./GanttTaskControl","dojo/_base/declare","dojo/domReady!"],function(_1,_2){
return _2("dojox.gantt.GanttTaskItem",[],{constructor:function(_3){
this.id=_3.id;
this.name=_3.name||this.id;
this.startTime=_3.startTime||new Date();
this.duration=_3.duration||8;
this.percentage=_3.percentage||0;
this.previousTaskId=_3.previousTaskId||"";
this.taskOwner=_3.taskOwner||"";
this.cldTasks=[];
this.cldPreTasks=[];
this.parentTask=null;
this.previousTask=null;
this.project=null;
this.nextChildTask=null;
this.previousChildTask=null;
this.nextParentTask=null;
this.previousParentTask=null;
},addChildTask:function(_4){
this.cldTasks.push(_4);
_4.parentTask=this;
},setProject:function(_5){
this.project=_5;
for(var j=0;j<this.cldTasks.length;j++){
this.cldTasks[j].setProject(_5);
}
}});
});
