//>>built
define(["dijit","dojo","dojox","dojo/require!dijit/Tooltip,dojox/gantt/GanttProjectItem,dojox/gantt/GanttResourceItem,dojox/gantt/TabMenu,dojo/date/locale"],function(_1,_2,_3){
_2.provide("dojox.gantt.GanttChart");
_2.require("dijit.Tooltip");
_2.require("dojox.gantt.GanttProjectItem");
_2.require("dojox.gantt.GanttResourceItem");
_2.require("dojox.gantt.TabMenu");
_2.require("dojo.date.locale");
(function(){
_2.declare("dojox.gantt.GanttChart",null,{constructor:function(_4,_5){
this.resourceChartHeight=_4.resourceChartHeight!==undefined?_4.resourceChartHeight:false;
this.withResource=_4.withResource!==undefined?_4.withResource:true;
this.correctError=_4.autoCorrectError!==undefined?_4.autoCorrectError:false;
this.isShowConMenu=this.isContentEditable=!_4.readOnly;
this.withTaskId=_4.withTaskId!==undefined?_4.withTaskId:!_4.readOnly;
this.animation=_4.animation!==undefined?_4.animation:true;
this.saveProgramPath=_4.saveProgramPath||"saveGanttData.php";
this.dataFilePath=_4.dataFilePath||"gantt_default.json";
this.contentHeight=_4.height||400;
this.contentWidth=_4.width||600;
this.content=_2.byId(_5);
this.scrollBarWidth=18;
this.panelTimeHeight=102;
this.maxWidthPanelNames=150;
this.maxWidthTaskNames=150;
this.minWorkLength=8;
this.heightTaskItem=12;
this.heightTaskItemExtra=11;
this.pixelsPerDay=24;
this.hsPerDay=8;
this.pixelsPerWorkHour=this.pixelsPerDay/this.hsPerDay;
this.pixelsPerHour=this.pixelsPerDay/24;
this.countDays=0;
this.totalDays=0;
this.startDate=null;
this.initialPos=0;
this.contentDataHeight=0;
this.panelTimeExpandDelta=20;
this.divTimeInfo=null;
this.panelNames=null;
this.panelTime=null;
this.contentData=null;
this.tabMenu=null;
this.project=[];
this.arrProjects=[];
this.xmlLoader=null;
this.isMoving=false;
this.isResizing=false;
this.animationNodes=[];
this.scale=1;
this.tempDayInPixels=0;
this.resource=null;
this.months=_2.date.locale.getNames("months","wide");
this._events=[];
},getProject:function(id){
return _2.filter(this.arrProjects,function(_6){
return _6.project.id==id;
},this)[0];
},checkPosPreviousTask:function(_7,_8){
var _9=this.getWidthOnDuration(_7.duration);
var _a=this.getPosOnDate(_7.startTime);
var _b=this.getPosOnDate(_8.startTime);
if((_9+_a)>_b){
return false;
}
return true;
},correctPosPreviousTask:function(_c,_d,_e){
var _f=new Date(_c.startTime);
_f.setHours(_f.getHours()+(_c.duration/this.hsPerDay*24));
if(_f.getHours()>0){
_f.setHours(0);
_f.setDate(_f.getDate()+1);
}
_e?(_e.setStartTime(_f,true)):(_d.startTime=_f);
if(_d.parentTask){
if(!this.checkPosParentTask(_d.parentTask,_d)){
var _10=new Date(_d.parentTask.startTime);
_10.setHours(_10.getHours()+(_d.parentTask.duration/this.hsPerDay*24));
_d.duration=parseInt((parseInt((_10-_d.startTime)/(1000*60*60)))*this.hsPerDay/24);
}
}
},correctPosParentTask:function(_11,_12){
if(!_12.previousTask){
if(_11.startTime>_12.startTime){
_12.startTime=new Date(_11.startTime);
}
if(!this.checkPosParentTask(_11,_12)){
_12.duration=_11.duration;
}
}else{
this.correctPosPreviousTask(_12.previousTask,_12);
}
},checkPosParentTaskInTree:function(_13){
var _14=false;
for(var i=0;i<_13.cldTasks.length;i++){
var _15=_13.cldTasks[i];
if(!this.checkPosParentTask(_13,_15)){
if(!this.correctError){
return true;
}else{
this.correctPosParentTask(_13,_15);
}
}
if(_13.startTime>_15.startTime){
if(!this.correctError){
return true;
}else{
this.correctPosParentTask(_13,_15);
}
}
if(_15.cldTasks.length>0){
_14=this.checkPosParentTaskInTree(_15);
}
}
return _14;
},setPreviousTask:function(_16){
var _17=false;
for(var i=0;i<_16.parentTasks.length;i++){
var _18=_16.parentTasks[i];
if(_18.previousTaskId){
_18.previousTask=_16.getTaskById(_18.previousTaskId);
if(!_18.previousTask){
if(!this.correctError){
return true;
}
}
_18.previousTask.cldPreTasks.push(_18);
}
if(_18.previousTask){
if(!this.checkPosPreviousTask(_18.previousTask,_18)){
if(!this.correctError){
return true;
}else{
this.correctPosPreviousTask(_18.previousTask,_18);
}
}
}
_17=this.setPreviousTaskInTree(_18);
}
return _17;
},setPreviousTaskInTree:function(_19){
var _1a=false;
for(var i=0;i<_19.cldTasks.length;i++){
var _1b=_19.cldTasks[i];
if(_1b.previousTaskId){
_1b.previousTask=_19.project.getTaskById(_1b.previousTaskId);
if(!_1b.previousTask){
if(!this.correctError){
return true;
}
}
if(!this.checkPosPreviousTask(_1b.previousTask,_1b)){
if(!this.correctError){
return true;
}else{
this.correctPosPreviousTask(_1b.previousTask,_1b);
}
}
_1b.previousTask.cldPreTasks.push(_1b);
}
if(_1b.cldTasks.length>0){
_1a=this.setPreviousTaskInTree(_1b);
}
}
return _1a;
},checkPosParentTask:function(_1c,_1d){
var _1e=this.getWidthOnDuration(_1c.duration);
var _1f=this.getPosOnDate(_1c.startTime);
var _20=this.getPosOnDate(_1d.startTime);
var _21=this.getWidthOnDuration(_1d.duration);
return (_1e+_1f)>=(_20+_21);
},addProject:function(_22){
this.project.push(_22);
},deleteProject:function(id){
var _23=this.getProject(id);
if(_23){
if(_23.arrTasks.length>0){
while(_23.arrTasks.length>0){
_23.deleteChildTask(_23.arrTasks[0]);
}
}
var _24=this.heightTaskItemExtra+this.heightTaskItem;
_23.nextProject&&_23.shiftNextProject(_23,-_24);
this.project=_2.filter(this.project,function(_25){
return _25.id!=_23.project.id;
},this);
if((_23.previousProject)&&(_23.nextProject)){
var _26=_23.previousProject;
_26.nextProject=_23.nextProject;
}
if((_23.previousProject)&&!(_23.nextProject)){
var _26=_23.previousProject;
_26.nextProject=null;
}
if(!(_23.previousProject)&&(_23.nextProject)){
var _27=_23.nextProject;
_27.previousProject=null;
}
for(var i=0;i<this.arrProjects.length;i++){
if(this.arrProjects[i].project.id==id){
this.arrProjects.splice(i,1);
}
}
_23.projectItem[0].parentNode.removeChild(_23.projectItem[0]);
_23.descrProject.parentNode.removeChild(_23.descrProject);
_23.projectNameItem.parentNode.removeChild(_23.projectNameItem);
this.contentDataHeight-=this.heightTaskItemExtra+this.heightTaskItem;
if(this.project.length==0){
var d=new Date(this.startDate);
var t=new Date(d.setDate(d.getDate()+1));
var pi=new _3.gantt.GanttProjectItem({id:1,name:"New Project",startDate:t});
this.project.push(pi);
var _23=new _3.gantt.GanttProjectControl(this,pi);
_23.create();
this.arrProjects.push(_23);
this.contentDataHeight+=this.heightTaskItemExtra+this.heightTaskItem;
}
this.checkPosition();
}
},insertProject:function(id,_28,_29){
if(this.startDate>=_29){
return false;
}
if(this.getProject(id)){
return false;
}
this.checkHeighPanelTasks();
var _2a=new _3.gantt.GanttProjectItem({id:id,name:_28,startDate:_29});
this.project.push(_2a);
var _2b=new _3.gantt.GanttProjectControl(this,_2a);
for(var i=0;i<this.arrProjects.length;i++){
var _2c=this.arrProjects[i],_2d=this.arrProjects[i-1],_2e=this.arrProjects[i+1];
if(_29<_2c.project.startDate){
this.arrProjects.splice(i,0,_2b);
if(i>0){
_2b.previousProject=_2d;
_2d.nextProject=_2b;
}
if(i+1<=this.arrProjects.length){
_2b.nextProject=_2e;
_2e.previousProject=_2b;
var _2f=this.heightTaskItem+this.heightTaskItemExtra;
_2b.shiftNextProject(_2b,_2f);
}
_2b.create();
_2b.hideDescrProject();
this.checkPosition();
return _2b;
}
}
if(this.arrProjects.length>0){
this.arrProjects[this.arrProjects.length-1].nextProject=_2b;
_2b.previousProject=this.arrProjects[this.arrProjects.length-1];
}
this.arrProjects.push(_2b);
_2b.create();
_2b.hideDescrProject();
this.checkPosition();
return _2b;
},openTree:function(_30){
var _31=this.getLastCloseParent(_30);
this.openNode(_31);
_30.taskItem.id!=_31.taskItem.id&&this.openTree(_30);
},openNode:function(_32){
if(!_32.isExpanded){
_2.removeClass(_32.cTaskNameItem[2],"ganttImageTreeExpand");
_2.addClass(_32.cTaskNameItem[2],"ganttImageTreeCollapse");
_32.isExpanded=true;
_32.shiftCurrentTasks(_32,_32.hideTasksHeight);
_32.showChildTasks(_32,_32.isExpanded);
_32.hideTasksHeight=0;
}
},getLastCloseParent:function(_33){
if(_33.parentTask){
if((!_33.parentTask.isExpanded)||(_33.parentTask.cTaskNameItem[2].style.display=="none")){
return this.getLastCloseParent(_33.parentTask);
}else{
return _33;
}
}else{
return _33;
}
},getProjectItemById:function(id){
return _2.filter(this.project,function(_34){
return _34.id==id;
},this)[0];
},clearAll:function(){
this.contentDataHeight=0;
this.startDate=null;
this.clearData();
this.clearItems();
this.clearEvents();
},clearEvents:function(){
_2.forEach(this._events,_2.disconnect);
this._events=[];
},clearData:function(){
this.project=[];
this.arrProjects=[];
},clearItems:function(){
this.contentData.removeChild(this.contentData.firstChild);
this.contentData.appendChild(this.createPanelTasks());
this.panelNames.removeChild(this.panelNames.firstChild);
this.panelNames.appendChild(this.createPanelNamesTasks());
this.panelTime.removeChild(this.panelTime.firstChild);
},buildUIContent:function(){
this.project.sort(this.sortProjStartDate);
this.startDate=this.getStartDate();
this.panelTime.appendChild(this.createPanelTime());
for(var i=0;i<this.project.length;i++){
var _35=this.project[i];
for(var k=0;k<_35.parentTasks.length;k++){
var _36=_35.parentTasks[k];
if(_36.startTime){
this.setStartTimeChild(_36);
}else{
return;
}
if(this.setPreviousTask(_35)){
return;
}
}
for(var k=0;k<_35.parentTasks.length;k++){
var _36=_35.parentTasks[k];
if(_36.startTime<_35.startDate){
return;
}
if(this.checkPosParentTaskInTree(_36)){
return;
}
}
this.sortTasksByStartTime(_35);
}
for(var i=0;i<this.project.length;i++){
var _35=this.project[i];
var _37=new _3.gantt.GanttProjectControl(this,_35);
if(this.arrProjects.length>0){
var _38=this.arrProjects[this.arrProjects.length-1];
_37.previousProject=_38;
_38.nextProject=_37;
}
_37.create();
this.checkHeighPanelTasks();
this.arrProjects.push(_37);
this.createTasks(_37);
}
this.resource&&this.resource.reConstruct();
this.postLoadData();
this.postBindEvents();
},loadJSONData:function(_39){
var _3a=this;
_3a.dataFilePath=_39||_3a.dataFilePath;
_2.xhrGet({url:_3a.dataFilePath,sync:true,load:function(_3b,_3c){
_3a.loadJSONString(_3b);
_3a.buildUIContent();
alert("Successfully! Loaded data from: "+_3a.dataFilePath);
},error:function(err,_3d){
alert("Failed! Load error: "+_3a.dataFilePath);
}});
},loadJSONString:function(_3e){
if(!_3e){
return;
}
this.clearAll();
var _3f=_2.fromJson(_3e);
var _40=_3f.items;
_2.forEach(_40,function(_41){
var _42=_41.startdate.split("-");
var _43=new _3.gantt.GanttProjectItem({id:_41.id,name:_41.name,startDate:new Date(_42[0],(parseInt(_42[1])-1),_42[2])});
var _44=_41.tasks;
_2.forEach(_44,function(_45){
var id=_45.id,_46=_45.name,_47=_45.starttime.split("-");
duration=_45.duration,percentage=_45.percentage,previousTaskId=_45.previousTaskId,taskOwner=_45.taskOwner;
var _48=new _3.gantt.GanttTaskItem({id:id,name:_46,startTime:new Date(_47[0],(parseInt(_47[1])-1),_47[2]),duration:duration,percentage:percentage,previousTaskId:previousTaskId,taskOwner:taskOwner});
var _49=_45.children;
if(_49.length!=0){
this.buildChildTasksData(_48,_49);
}
_43.addTask(_48);
},this);
this.addProject(_43);
},this);
},buildChildTasksData:function(_4a,_4b){
_4b&&_2.forEach(_4b,function(_4c){
var id=_4c.id,_4d=_4c.name,_4e=_4c.starttime.split("-"),_4f=_4c.duration,_50=_4c.percentage,_51=_4c.previousTaskId,_52=_4c.taskOwner;
var _53=new _3.gantt.GanttTaskItem({id:id,name:_4d,startTime:new Date(_4e[0],(parseInt(_4e[1])-1),_4e[2]),duration:_4f,percentage:_50,previousTaskId:_51,taskOwner:_52});
_53.parentTask=_4a;
_4a.addChildTask(_53);
var _54=_4c.children;
if(_54.length!=0){
this.buildChildTasksData(_53,_54);
}
},this);
},getJSONData:function(){
var _55={identifier:"id",items:[]};
_2.forEach(this.project,function(_56){
var _57={id:_56.id,name:_56.name,startdate:_56.startDate.getFullYear()+"-"+(_56.startDate.getMonth()+1)+"-"+_56.startDate.getDate(),tasks:[]};
_55.items.push(_57);
_2.forEach(_56.parentTasks,function(_58){
var _59={id:_58.id,name:_58.name,starttime:_58.startTime.getFullYear()+"-"+(_58.startTime.getMonth()+1)+"-"+_58.startTime.getDate(),duration:_58.duration,percentage:_58.percentage,previousTaskId:(_58.previousTaskId||""),taskOwner:(_58.taskOwner||""),children:this.getChildTasksData(_58.cldTasks)};
_57.tasks.push(_59);
},this);
},this);
return _55;
},getChildTasksData:function(_5a){
var _5b=[];
_5a&&_5a.length>0&&_2.forEach(_5a,function(_5c){
var _5d={id:_5c.id,name:_5c.name,starttime:_5c.startTime.getFullYear()+"-"+(_5c.startTime.getMonth()+1)+"-"+_5c.startTime.getDate(),duration:_5c.duration,percentage:_5c.percentage,previousTaskId:(_5c.previousTaskId||""),taskOwner:(_5c.taskOwner||""),children:this.getChildTasksData(_5c.cldTasks)};
_5b.push(_5d);
},this);
return _5b;
},saveJSONData:function(_5e){
var _5f=this;
_5f.dataFilePath=(_5e&&_2.trim(_5e).length>0)?_5e:this.dataFilePath;
try{
var td=_2.xhrPost({url:_5f.saveProgramPath,content:{filename:_5f.dataFilePath,data:_2.toJson(_5f.getJSONData())},handle:function(res,_60){
if((_2._isDocumentOk(_60.xhr))||(_60.xhr.status==405)){
alert("Successfully! Saved data to "+_5f.dataFilePath);
}else{
alert("Failed! Saved error");
}
}});
}
catch(e){
alert("exception: "+e.message);
}
},sortTaskStartTime:function(a,b){
return a.startTime<b.startTime?-1:(a.startTime>b.startTime?1:0);
},sortProjStartDate:function(a,b){
return a.startDate<b.startDate?-1:(a.startDate>b.startDate?1:0);
},setStartTimeChild:function(_61){
_2.forEach(_61.cldTasks,function(_62){
if(!_62.startTime){
_62.startTime=_61.startTime;
}
if(_62.cldTasks.length!=0){
this.setStartTimeChild(_62);
}
},this);
},createPanelTasks:function(){
var _63=_2.create("div",{className:"ganttTaskPanel"});
_2.style(_63,{height:(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px"});
return _63;
},refreshParams:function(_64){
this.pixelsPerDay=_64;
this.pixelsPerWorkHour=this.pixelsPerDay/this.hsPerDay;
this.pixelsPerHour=this.pixelsPerDay/24;
},createPanelNamesTasksHeader:function(){
var _65=this;
var _66=_2.create("div",{className:"ganttPanelHeader"});
var _67=_2.create("table",{cellPadding:"0px",border:"0px",cellSpacing:"0px",bgColor:"#FFFFFF",className:"ganttToolbar"},_66);
var _68=_67.insertRow(_67.rows.length);
var _69=_67.insertRow(_67.rows.length);
var _6a=_67.insertRow(_67.rows.length);
var _6b=_67.insertRow(_67.rows.length);
var _6c=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarZoomIn"},_68);
var _6d=_2.hitch(this,function(){
if(this.scale*2>5){
return;
}
this.scale=this.scale*2;
this.switchTeleMicroView(this.pixelsPerDay*this.scale);
});
_2.disconnect(this.zoomInClickEvent);
this.zoomInClickEvent=_2.connect(_6c,"onclick",this,_6d);
_2.disconnect(this.zoomInKeyEvent);
this.zoomInKeyEvent=_2.connect(_6c,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
_6d();
});
_2.attr(_6c,"tabIndex",0);
var _6e=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarZoomOut"},_68);
var _6f=_2.hitch(this,function(){
if(this.scale*0.5<0.2){
return;
}
this.scale=this.scale*0.5;
this.switchTeleMicroView(this.pixelsPerDay*this.scale);
});
_2.disconnect(this.zoomOutClickEvent);
this.zoomOutClickEvent=_2.connect(_6e,"onclick",this,_6f);
_2.disconnect(this.zoomOutKeyEvent);
this.zoomOutKeyEvent=_2.connect(_6e,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
_6f();
});
_2.attr(_6e,"tabIndex",0);
var _70=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarMicro"},_69);
_2.disconnect(this.microClickEvent);
this.microClickEvent=_2.connect(_70,"onclick",this,_2.hitch(this,this.refresh,this.animation?15:1,0,2));
_2.disconnect(this.microKeyEvent);
this.microKeyEvent=_2.connect(_70,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
_70.blur();
this.refresh(this.animation?15:1,0,2);
});
_2.attr(_70,"tabIndex",0);
var _71=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarTele"},_69);
_2.disconnect(this.teleClickEvent);
this.teleClickEvent=_2.connect(_71,"onclick",this,_2.hitch(this,this.refresh,this.animation?15:1,0,0.5));
_2.disconnect(this.teleKeyEvent);
this.teleKeyEvent=_2.connect(_71,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
_71.blur();
this.refresh(this.animation?15:1,0,0.5);
});
_2.attr(_71,"tabIndex",0);
var _72=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarSave"},_6a);
_2.disconnect(this.saveClickEvent);
this.saveClickEvent=_2.connect(_72,"onclick",this,_2.hitch(this,this.saveJSONData,""));
_2.disconnect(this.saveKeyEvent);
this.saveKeyEvent=_2.connect(_72,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
this.saveJSONData("");
});
_2.attr(_72,"tabIndex",0);
var _73=_2.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarLoad"},_6a);
_2.disconnect(this.loadClickEvent);
this.loadClickEvent=_2.connect(_73,"onclick",this,_2.hitch(this,this.loadJSONData,""));
_2.disconnect(this.loadKeyEvent);
this.loadKeyEvent=_2.connect(_73,"onkeydown",this,function(e){
if(e.keyCode!=_2.keys.ENTER){
return;
}
this.loadJSONData("");
});
_2.attr(_73,"tabIndex",0);
var _74=[_6c,_6e,_70,_71,_72,_73],_75=["Enlarge timeline","Shrink timeline","Zoom in time zone(microscope view)","Zoom out time zone(telescope view)","Save gantt data to json file","Load gantt data from json file"];
_2.forEach(_74,function(_76,i){
var _77=_75[i];
var _78=function(){
_2.addClass(_76,"ganttToolbarActionHover");
_1.showTooltip(_77,_76,["above","below"]);
};
_76.onmouseover=_78;
_76.onfocus=_78;
var _79=function(){
_2.removeClass(_76,"ganttToolbarActionHover");
_76&&_1.hideTooltip(_76);
};
_76.onmouseout=_79;
_76.onblur=_79;
},this);
return _66;
},createPanelNamesTasks:function(){
var _7a=_2.create("div",{innerHTML:"&nbsp;",className:"ganttPanelNames"});
_2.style(_7a,{height:(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px",width:this.maxWidthPanelNames+"px"});
return _7a;
},createPanelTime:function(){
var _7b=_2.create("div",{className:"ganttPanelTime"});
var _7c=_2.create("table",{cellPadding:"0px",border:"0px",cellSpacing:"0px",bgColor:"#FFFFFF",className:"ganttTblTime"},_7b);
this.totalDays=this.countDays;
var _7d=_7c.insertRow(_7c.rows.length),_7e=oldYear=new Date(this.startDate).getFullYear(),_7f=0;
for(var i=0;i<this.countDays;i++,_7f++){
var _80=new Date(this.startDate);
_80.setDate(_80.getDate()+i);
_7e=_80.getFullYear();
if(_7e!=oldYear){
this.addYearInPanelTime(_7d,_7f,oldYear);
_7f=0;
oldYear=_7e;
}
}
this.addYearInPanelTime(_7d,_7f,_7e);
_2.style(_7d,"display","none");
var _81=_7c.insertRow(_7c.rows.length),_82=oldMonth=new Date(this.startDate).getMonth(),_83=0,_84=1970;
for(var i=0;i<this.countDays;i++,_83++){
var _80=new Date(this.startDate);
_80.setDate(_80.getDate()+i);
_82=_80.getMonth();
_84=_80.getFullYear();
if(_82!=oldMonth){
this.addMonthInPanelTime(_81,_83,oldMonth,_84);
_83=0;
oldMonth=_82;
}
}
this.addMonthInPanelTime(_81,_83,_82,_84);
var _85=_7c.insertRow(_7c.rows.length),_86=oldWeek=_2.date.locale._getWeekOfYear(new Date(this.startDate)),_83=0;
for(var i=0;i<this.countDays;i++,_83++){
var _80=new Date(this.startDate);
_80.setDate(_80.getDate()+i);
_86=_2.date.locale._getWeekOfYear(_80);
if(_86!=oldWeek){
this.addWeekInPanelTime(_85,_83,oldWeek);
_83=0;
oldWeek=_86;
}
}
this.addWeekInPanelTime(_85,_83,_86);
var _87=_7c.insertRow(_7c.rows.length);
for(var i=0;i<this.countDays;i++){
this.addDayInPanelTime(_87);
}
var _88=_7c.insertRow(_7c.rows.length);
for(var i=0;i<this.countDays;i++){
this.addHourInPanelTime(_88);
}
_2.style(_88,"display","none");
return _7b;
},adjustPanelTime:function(_89){
var _8a=_2.map(this.arrProjects,function(_8b){
return (parseInt(_8b.projectItem[0].style.left)+parseInt(_8b.projectItem[0].firstChild.style.width)+_8b.descrProject.offsetWidth+this.panelTimeExpandDelta);
},this).sort(function(a,b){
return b-a;
})[0];
if(this.maxTaskEndPos!=_8a){
var _8c=this.panelTime.firstChild.firstChild.rows;
for(var i=0;i<=4;i++){
this.removeCell(_8c[i]);
}
var _8d=Math.round((_8a+this.panelTimeExpandDelta)/this.pixelsPerDay);
this.totalDays=_8d;
var _8e=oldYear=new Date(this.startDate).getFullYear(),_8f=0;
for(var i=0;i<_8d;i++,_8f++){
var _90=new Date(this.startDate);
_90.setDate(_90.getDate()+i);
_8e=_90.getFullYear();
if(_8e!=oldYear){
this.addYearInPanelTime(_8c[0],_8f,oldYear);
_8f=0;
oldYear=_8e;
}
}
this.addYearInPanelTime(_8c[0],_8f,_8e);
var _91=oldMonth=new Date(this.startDate).getMonth(),_92=0,_93=1970;
for(var i=0;i<_8d;i++,_92++){
var _90=new Date(this.startDate);
_90.setDate(_90.getDate()+i);
_91=_90.getMonth();
_93=_90.getFullYear();
if(_91!=oldMonth){
this.addMonthInPanelTime(_8c[1],_92,oldMonth,_93);
_92=0;
oldMonth=_91;
}
}
this.addMonthInPanelTime(_8c[1],_92,_91,_93);
var _94=oldWeek=_2.date.locale._getWeekOfYear(new Date(this.startDate)),_92=0;
for(var i=0;i<_8d;i++,_92++){
var _90=new Date(this.startDate);
_90.setDate(_90.getDate()+i);
_94=_2.date.locale._getWeekOfYear(_90);
if(_94!=oldWeek){
this.addWeekInPanelTime(_8c[2],_92,oldWeek);
_92=0;
oldWeek=_94;
}
}
this.addWeekInPanelTime(_8c[2],_92,_94);
for(var i=0;i<_8d;i++){
this.addDayInPanelTime(_8c[3]);
}
for(var i=0;i<_8d;i++){
this.addHourInPanelTime(_8c[4]);
}
this.panelTime.firstChild.firstChild.style.width=this.pixelsPerDay*(_8c[3].cells.length)+"px";
this.contentData.firstChild.style.width=this.pixelsPerDay*(_8c[3].cells.length)+"px";
this.maxTaskEndPos=_8a;
}
},addYearInPanelTime:function(row,_95,_96){
var _97="Year   "+_96;
var _98=_2.create("td",{colSpan:_95,align:"center",vAlign:"middle",className:"ganttYearNumber",innerHTML:this.pixelsPerDay*_95>20?_97:"",innerHTMLData:_97},row);
_2.style(_98,"width",(this.pixelsPerDay*_95)+"px");
},addMonthInPanelTime:function(row,_99,_9a,_9b){
var _9c=this.months[_9a]+(_9b?" of "+_9b:"");
var _9d=_2.create("td",{colSpan:_99,align:"center",vAlign:"middle",className:"ganttMonthNumber",innerHTML:this.pixelsPerDay*_99>30?_9c:"",innerHTMLData:_9c},row);
_2.style(_9d,"width",(this.pixelsPerDay*_99)+"px");
},addWeekInPanelTime:function(row,_9e,_9f){
var _a0="Week   "+_9f;
var _a1=_2.create("td",{colSpan:_9e,align:"center",vAlign:"middle",className:"ganttWeekNumber",innerHTML:this.pixelsPerDay*_9e>20?_a0:"",innerHTMLData:_a0},row);
_2.style(_a1,"width",(this.pixelsPerDay*_9e)+"px");
},addDayInPanelTime:function(row){
var _a2=new Date(this.startDate);
_a2.setDate(_a2.getDate()+parseInt(row.cells.length));
var _a3=_2.create("td",{align:"center",vAlign:"middle",className:"ganttDayNumber",innerHTML:this.pixelsPerDay>20?_a2.getDate():"",innerHTMLData:String(_a2.getDate()),data:row.cells.length},row);
_2.style(_a3,"width",this.pixelsPerDay+"px");
(_a2.getDay()>=5)&&_2.addClass(_a3,"ganttDayNumberWeekend");
this._events.push(_2.connect(_a3,"onmouseover",this,function(_a4){
var _a5=_a4.target||_a4.srcElement;
var _a6=new Date(this.startDate.getTime());
_a6.setDate(_a6.getDate()+parseInt(_2.attr(_a5,"data")));
_1.showTooltip(_a6.getFullYear()+"."+(_a6.getMonth()+1)+"."+_a6.getDate(),_a3,["above","below"]);
}));
this._events.push(_2.connect(_a3,"onmouseout",this,function(_a7){
var _a8=_a7.target||_a7.srcElement;
_a8&&_1.hideTooltip(_a8);
}));
},addHourInPanelTime:function(row){
var _a9=_2.create("td",{align:"center",vAlign:"middle",className:"ganttHourNumber",data:row.cells.length},row);
_2.style(_a9,"width",this.pixelsPerDay+"px");
var _aa=_2.create("table",{cellPadding:"0",cellSpacing:"0"},_a9);
var _ab=_aa.insertRow(_aa.rows.length);
for(var i=0;i<this.hsPerDay;i++){
var _ac=_2.create("td",{className:"ganttHourClass"},_ab);
_2.style(_ac,"width",(this.pixelsPerDay/this.hsPerDay)+"px");
_2.attr(_ac,"innerHTMLData",String(9+i));
if(this.pixelsPerDay/this.hsPerDay>5){
_2.attr(_ac,"innerHTML",String(9+i));
}
_2.addClass(_ac,i<=3?"ganttHourNumberAM":"ganttHourNumberPM");
}
},incHeightPanelTasks:function(_ad){
var _ae=this.contentData.firstChild;
_ae.style.height=parseInt(_ae.style.height)+_ad+"px";
},incHeightPanelNames:function(_af){
var _b0=this.panelNames.firstChild;
_b0.style.height=parseInt(_b0.style.height)+_af+"px";
},checkPosition:function(){
_2.forEach(this.arrProjects,function(_b1){
_2.forEach(_b1.arrTasks,function(_b2){
_b2.checkPosition();
},this);
},this);
},checkHeighPanelTasks:function(){
this.contentDataHeight+=this.heightTaskItemExtra+this.heightTaskItem;
if((parseInt(this.contentData.firstChild.style.height)<=this.contentDataHeight)){
this.incHeightPanelTasks(this.heightTaskItem+this.heightTaskItemExtra);
this.incHeightPanelNames(this.heightTaskItem+this.heightTaskItemExtra);
}
},sortTasksByStartTime:function(_b3){
_b3.parentTasks.sort(this.sortTaskStartTime);
for(var i=0;i<_b3.parentTasks.length;i++){
_b3.parentTasks[i]=this.sortChildTasks(_b3.parentTasks[i]);
}
},sortChildTasks:function(_b4){
_b4.cldTasks.sort(this.sortTaskStartTime);
for(var i=0;i<_b4.cldTasks.length;i++){
if(_b4.cldTasks[i].cldTasks.length>0){
this.sortChildTasks(_b4.cldTasks[i]);
}
}
return _b4;
},refresh:function(_b5,_b6,_b7){
if(this.arrProjects.length<=0){
return;
}
if(this.arrProjects[0].arrTasks.length<=0){
return;
}
if(!_b5||_b6>_b5){
this.refreshController();
if(this.resource){
this.resource.refresh();
}
this.tempDayInPixels=0;
this.panelNameHeadersCover&&_2.style(this.panelNameHeadersCover,"display","none");
return;
}
if(this.tempDayInPixels==0){
this.tempDayInPixels=this.pixelsPerDay;
}
this.panelNameHeadersCover&&_2.style(this.panelNameHeadersCover,"display","");
var dip=this.tempDayInPixels+this.tempDayInPixels*(_b7-1)*Math.pow((_b6/_b5),2);
this.refreshParams(dip);
_2.forEach(this.arrProjects,function(_b8){
_2.forEach(_b8.arrTasks,function(_b9){
_b9.refresh();
},this);
_b8.refresh();
},this);
setTimeout(_2.hitch(this,function(){
this.refresh(_b5,++_b6,_b7);
}),15);
},switchTeleMicroView:function(dip){
var _ba=this.panelTime.firstChild.firstChild;
for(var i=0;i<5;i++){
if(dip>40){
_2.style(_ba.rows[i],"display",(i==0||i==1)?"none":"");
}else{
if(dip<20){
_2.style(_ba.rows[i],"display",(i==2||i==4)?"none":"");
}else{
_2.style(_ba.rows[i],"display",(i==0||i==4)?"none":"");
}
}
}
},refreshController:function(){
this.contentData.firstChild.style.width=Math.max(1200,this.pixelsPerDay*this.totalDays)+"px";
this.panelTime.firstChild.style.width=this.pixelsPerDay*this.totalDays+"px";
this.panelTime.firstChild.firstChild.style.width=this.pixelsPerDay*this.totalDays+"px";
this.switchTeleMicroView(this.pixelsPerDay);
_2.forEach(this.panelTime.firstChild.firstChild.rows,function(row){
_2.forEach(row.childNodes,function(td){
var cs=parseInt(_2.attr(td,"colSpan")||1);
var _bb=_2.trim(_2.attr(td,"innerHTMLData")||"");
if(_bb.length>0){
_2.attr(td,"innerHTML",this.pixelsPerDay*cs<20?"":_bb);
}else{
_2.forEach(td.firstChild.rows[0].childNodes,function(td){
var _bc=_2.trim(_2.attr(td,"innerHTMLData")||"");
_2.attr(td,"innerHTML",this.pixelsPerDay/this.hsPerDay>10?_bc:"");
},this);
}
if(cs==1){
_2.style(td,"width",(this.pixelsPerDay*cs)+"px");
if(_bb.length<=0){
_2.forEach(td.firstChild.rows[0].childNodes,function(td){
_2.style(td,"width",(this.pixelsPerDay*cs/this.hsPerDay)+"px");
},this);
}
}
},this);
},this);
},init:function(){
this.startDate=this.getStartDate();
_2.style(this.content,{width:this.contentWidth+"px",height:this.contentHeight+"px"});
this.tableControl=_2.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttTabelControl"});
var _bd=this.tableControl.insertRow(this.tableControl.rows.length);
this.content.appendChild(this.tableControl);
this.countDays=this.getCountDays();
this.panelTime=_2.create("div",{className:"ganttPanelTimeContainer"});
_2.style(this.panelTime,"height",this.panelTimeHeight+"px");
this.panelTime.appendChild(this.createPanelTime());
this.contentData=_2.create("div",{className:"ganttContentDataContainer"});
_2.style(this.contentData,"height",(this.contentHeight-this.panelTimeHeight)+"px");
this.contentData.appendChild(this.createPanelTasks());
var _be=_2.create("td",{vAlign:"top"});
this.panelNameHeaders=_2.create("div",{className:"ganttPanelNameHeaders"},_be);
_2.style(this.panelNameHeaders,{height:this.panelTimeHeight+"px",width:this.maxWidthPanelNames+"px"});
this.panelNameHeaders.appendChild(this.createPanelNamesTasksHeader());
this.panelNames=_2.create("div",{className:"ganttPanelNamesContainer"},_be);
this.panelNames.appendChild(this.createPanelNamesTasks());
_bd.appendChild(_be);
_be=_2.create("td",{vAlign:"top"});
var _bf=_2.create("div",{className:"ganttDivCell"});
_bf.appendChild(this.panelTime);
_bf.appendChild(this.contentData);
_be.appendChild(_bf);
_bd.appendChild(_be);
_2.style(this.panelNames,"height",(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px");
_2.style(this.panelNames,"width",this.maxWidthPanelNames+"px");
_2.style(this.contentData,"width",(this.contentWidth-this.maxWidthPanelNames)+"px");
_2.style(this.contentData.firstChild,"width",this.pixelsPerDay*this.countDays+"px");
_2.style(this.panelTime,"width",(this.contentWidth-this.maxWidthPanelNames-this.scrollBarWidth)+"px");
_2.style(this.panelTime.firstChild,"width",this.pixelsPerDay*this.countDays+"px");
if(this.isShowConMenu){
this.tabMenu=new _3.gantt.TabMenu(this);
}
var _c0=this;
this.contentData.onscroll=function(){
_c0.panelTime.scrollLeft=this.scrollLeft;
if(_c0.panelNames){
_c0.panelNames.scrollTop=this.scrollTop;
if(_c0.isShowConMenu){
_c0.tabMenu.hide();
}
}
if(_c0.resource){
_c0.resource.contentData.scrollLeft=this.scrollLeft;
}
};
this.project.sort(this.sortProjStartDate);
for(var i=0;i<this.project.length;i++){
var _c1=this.project[i];
for(var k=0;k<_c1.parentTasks.length;k++){
var _c2=_c1.parentTasks[k];
if(!_c2.startTime){
_c2.startTime=_c1.startDate;
}
this.setStartTimeChild(_c2);
if(this.setPreviousTask(_c1)){
return;
}
}
for(var k=0;k<_c1.parentTasks.length;k++){
var _c2=_c1.parentTasks[k];
if(_c2.startTime<_c1.startDate){
if(!this.correctError){
return;
}else{
_c2.startTime=_c1.startDate;
}
}
if(this.checkPosParentTaskInTree(_c2)){
return;
}
}
this.sortTasksByStartTime(_c1);
}
for(var i=0;i<this.project.length;i++){
var _c1=this.project[i];
var _c3=new _3.gantt.GanttProjectControl(this,_c1);
if(this.arrProjects.length>0){
var _c4=this.arrProjects[this.arrProjects.length-1];
_c3.previousProject=_c4;
_c4.nextProject=_c3;
}
_c3.create();
this.checkHeighPanelTasks();
this.arrProjects.push(_c3);
this.createTasks(_c3);
}
if(this.withResource){
this.resource=new _3.gantt.GanttResourceItem(this);
this.resource.create();
}
this.postLoadData();
this.postBindEvents();
return this;
},postLoadData:function(){
_2.forEach(this.arrProjects,function(_c5){
_2.forEach(_c5.arrTasks,function(_c6){
_c6.postLoadData();
},this);
_c5.postLoadData();
},this);
var _c7=_2.coords(this.panelNameHeaders);
if(!this.panelNameHeadersCover){
this.panelNameHeadersCover=_2.create("div",{className:"ganttHeaderCover"},this.panelNameHeaders.parentNode);
_2.style(this.panelNameHeadersCover,{left:_c7.l+"px",top:_c7.t+"px",height:_c7.h+"px",width:_c7.w+"px",display:"none"});
}
},postBindEvents:function(){
var pos=_2.position(this.tableControl,true);
!_2.isIE&&this._events.push(_2.connect(this.tableControl,"onmousemove",this,function(_c8){
var _c9=_c8.srcElement||_c8.target;
if(_c9==this.panelNames.firstChild||_c9==this.contentData.firstChild){
var _ca=this.heightTaskItem+this.heightTaskItemExtra;
var _cb=parseInt(_c8.layerY/_ca)*_ca+this.panelTimeHeight-this.contentData.scrollTop;
if(_cb!=this.oldHLTop&&_cb<(pos.h-50)){
if(this.highLightDiv){
_2.style(this.highLightDiv,"top",(pos.y+_cb)+"px");
}else{
this.highLightDiv=_2.create("div",{className:"ganttRowHighlight"},_2.body());
_2.style(this.highLightDiv,{top:(pos.y+_cb)+"px",left:pos.x+"px",width:(pos.w-20)+"px",height:_ca+"px"});
}
}
this.oldHLTop=_cb;
}
}));
},getStartDate:function(){
_2.forEach(this.project,function(_cc){
if(this.startDate){
if(_cc.startDate<this.startDate){
this.startDate=new Date(_cc.startDate);
}
}else{
this.startDate=new Date(_cc.startDate);
}
},this);
this.initialPos=24*this.pixelsPerHour;
return this.startDate?new Date(this.startDate.setHours(this.startDate.getHours()-24)):new Date();
},getCountDays:function(){
return parseInt((this.contentWidth-this.maxWidthPanelNames)/(this.pixelsPerHour*24));
},createTasks:function(_cd){
_2.forEach(_cd.project.parentTasks,function(_ce,i){
if(i>0){
_cd.project.parentTasks[i-1].nextParentTask=_ce;
_ce.previousParentTask=_cd.project.parentTasks[i-1];
}
var _cf=new _3.gantt.GanttTaskControl(_ce,_cd,this);
_cd.arrTasks.push(_cf);
_cf.create();
this.checkHeighPanelTasks();
if(_ce.cldTasks.length>0){
this.createChildItemControls(_ce.cldTasks,_cd);
}
},this);
},createChildItemControls:function(_d0,_d1){
_d0&&_2.forEach(_d0,function(_d2,i){
if(i>0){
_d2.previousChildTask=_d0[i-1];
_d0[i-1].nextChildTask=_d2;
}
var _d3=new _3.gantt.GanttTaskControl(_d2,_d1,this);
_d3.create();
this.checkHeighPanelTasks();
if(_d2.cldTasks.length>0){
this.createChildItemControls(_d2.cldTasks,_d1);
}
},this);
},getPosOnDate:function(_d4){
return (_d4-this.startDate)/(60*60*1000)*this.pixelsPerHour;
},getWidthOnDuration:function(_d5){
return Math.round(this.pixelsPerWorkHour*_d5);
},getLastChildTask:function(_d6){
return _d6.childTask.length>0?this.getLastChildTask(_d6.childTask[_d6.childTask.length-1]):_d6;
},removeCell:function(row){
while(row.cells[0]){
row.deleteCell(row.cells[0]);
}
}});
})();
});
