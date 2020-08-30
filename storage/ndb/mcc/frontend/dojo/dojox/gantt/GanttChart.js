//>>built
define("dojox/gantt/GanttChart",["./GanttProjectItem","./GanttResourceItem","./GanttProjectControl","./GanttTaskControl","./GanttTaskItem","./TabMenu","dijit/Tooltip","dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/date/locale","dojo/request","dojo/request/util","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dojo/keys","dojo/has","dojo/_base/window","dojo/json","dojo/domReady!"],function(_1,_2,_3,_4,_5,_6,_7,_8,_9,_a,_b,_c,_d,on,_e,_f,_10,_11,_12,_13,_14,has,win,_15){
return _8("dojox.gantt.GanttChart",[],{constructor:function(_16,_17){
this.resourceChartHeight=_16.resourceChartHeight!==undefined?_16.resourceChartHeight:false;
this.withResource=_16.withResource!==undefined?_16.withResource:true;
this.correctError=_16.autoCorrectError!==undefined?_16.autoCorrectError:false;
this.isShowConMenu=this.isContentEditable=!_16.readOnly;
this.withTaskId=_16.withTaskId!==undefined?_16.withTaskId:!_16.readOnly;
this.animation=_16.animation!==undefined?_16.animation:true;
this.saveProgramPath=_16.saveProgramPath||"saveGanttData.php";
this.dataFilePath=_16.dataFilePath||"gantt_default.json";
this.contentHeight=_16.height||400;
this.contentWidth=_16.width||600;
this.content=_e.byId(_17);
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
this.months=_b.getNames("months","wide");
this._events=[];
},getProject:function(id){
return _9.filter(this.arrProjects,function(_18){
return _18.project.id==id;
},this)[0];
},checkPosPreviousTask:function(_19,_1a){
var _1b=this.getWidthOnDuration(_19.duration);
var _1c=this.getPosOnDate(_19.startTime);
var _1d=this.getPosOnDate(_1a.startTime);
return (_1b+_1c)<=_1d;
},correctPosPreviousTask:function(_1e,_1f,_20){
var _21=new Date(_1e.startTime);
_21.setHours(_21.getHours()+(_1e.duration/this.hsPerDay*24));
if(_21.getHours()>0){
_21.setHours(0);
_21.setDate(_21.getDate()+1);
}
_20?(_20.setStartTime(_21,true)):(_1f.startTime=_21);
if(_1f.parentTask){
if(!this.checkPosParentTask(_1f.parentTask,_1f)){
var _22=new Date(_1f.parentTask.startTime);
_22.setHours(_22.getHours()+(_1f.parentTask.duration/this.hsPerDay*24));
_1f.duration=parseInt((parseInt((_22-_1f.startTime)/(1000*60*60)))*this.hsPerDay/24);
}
}
},correctPosParentTask:function(_23,_24){
if(!_24.previousTask){
if(_23.startTime>_24.startTime){
_24.startTime=new Date(_23.startTime);
}
if(!this.checkPosParentTask(_23,_24)){
_24.duration=_23.duration;
}
}else{
this.correctPosPreviousTask(_24.previousTask,_24);
}
},checkPosParentTaskInTree:function(_25){
var _26=false;
for(var i=0;i<_25.cldTasks.length;i++){
var _27=_25.cldTasks[i];
if(!this.checkPosParentTask(_25,_27)){
if(!this.correctError){
return true;
}else{
this.correctPosParentTask(_25,_27);
}
}
if(_25.startTime>_27.startTime){
if(!this.correctError){
return true;
}else{
this.correctPosParentTask(_25,_27);
}
}
if(_27.cldTasks.length>0){
_26=this.checkPosParentTaskInTree(_27);
}
}
return _26;
},setPreviousTask:function(_28){
var _29=false;
for(var i=0;i<_28.parentTasks.length;i++){
var _2a=_28.parentTasks[i];
if(_2a.previousTaskId){
_2a.previousTask=_28.getTaskById(_2a.previousTaskId);
if(!_2a.previousTask){
if(!this.correctError){
return true;
}
}
_2a.previousTask.cldPreTasks.push(_2a);
}
if(_2a.previousTask){
if(!this.checkPosPreviousTask(_2a.previousTask,_2a)){
if(!this.correctError){
return true;
}else{
this.correctPosPreviousTask(_2a.previousTask,_2a);
}
}
}
_29=this.setPreviousTaskInTree(_2a);
}
return _29;
},setPreviousTaskInTree:function(_2b){
var _2c=false;
for(var i=0;i<_2b.cldTasks.length;i++){
var _2d=_2b.cldTasks[i];
if(_2d.previousTaskId){
_2d.previousTask=_2b.project.getTaskById(_2d.previousTaskId);
if(!_2d.previousTask){
if(!this.correctError){
return true;
}
}
if(!this.checkPosPreviousTask(_2d.previousTask,_2d)){
if(!this.correctError){
return true;
}else{
this.correctPosPreviousTask(_2d.previousTask,_2d);
}
}
_2d.previousTask.cldPreTasks.push(_2d);
}
if(_2d.cldTasks.length>0){
_2c=this.setPreviousTaskInTree(_2d);
}
}
return _2c;
},checkPosParentTask:function(_2e,_2f){
var _30=this.getWidthOnDuration(_2e.duration);
var _31=this.getPosOnDate(_2e.startTime);
var _32=this.getPosOnDate(_2f.startTime);
var _33=this.getWidthOnDuration(_2f.duration);
return (_30+_31)>=(_32+_33);
},addProject:function(_34){
this.project.push(_34);
},deleteProject:function(id){
var _35=this.getProject(id);
if(_35){
if(_35.arrTasks.length>0){
while(_35.arrTasks.length>0){
_35.deleteChildTask(_35.arrTasks[0]);
}
}
var _36=this.heightTaskItemExtra+this.heightTaskItem;
_35.nextProject&&_35.shiftNextProject(_35,-_36);
this.project=_9.filter(this.project,function(_37){
return _37.id!=_35.project.id;
},this);
if((_35.previousProject)&&(_35.nextProject)){
var _38=_35.previousProject;
_38.nextProject=_35.nextProject;
}
if((_35.previousProject)&&!(_35.nextProject)){
var _38=_35.previousProject;
_38.nextProject=null;
}
if(!(_35.previousProject)&&(_35.nextProject)){
var _39=_35.nextProject;
_39.previousProject=null;
}
for(var i=0;i<this.arrProjects.length;i++){
if(this.arrProjects[i].project.id==id){
this.arrProjects.splice(i,1);
}
}
_35.projectItem[0].parentNode.removeChild(_35.projectItem[0]);
_35.descrProject.parentNode.removeChild(_35.descrProject);
_35.projectNameItem.parentNode.removeChild(_35.projectNameItem);
this.contentDataHeight-=this.heightTaskItemExtra+this.heightTaskItem;
if(this.project.length==0){
var d=new Date(this.startDate);
var t=new Date(d.setDate(d.getDate()+1));
var pi=new _1({id:1,name:"New Project",startDate:t});
this.project.push(pi);
var _35=new _3(this,pi);
_35.create();
this.arrProjects.push(_35);
this.contentDataHeight+=this.heightTaskItemExtra+this.heightTaskItem;
}
this.checkPosition();
}
},insertProject:function(id,_3a,_3b){
if(this.startDate>=_3b){
return false;
}
if(this.getProject(id)){
return false;
}
this.checkHeighPanelTasks();
var _3c=new _1({id:id,name:_3a,startDate:_3b});
this.project.push(_3c);
var _3d=new _3(this,_3c);
for(var i=0;i<this.arrProjects.length;i++){
var _3e=this.arrProjects[i],_3f=this.arrProjects[i-1],_40=this.arrProjects[i+1];
if(_3b<_3e.project.startDate){
this.arrProjects.splice(i,0,_3d);
if(i>0){
_3d.previousProject=_3f;
_3f.nextProject=_3d;
}
if(i+1<=this.arrProjects.length){
_3d.nextProject=_40;
_40.previousProject=_3d;
var _41=this.heightTaskItem+this.heightTaskItemExtra;
_3d.shiftNextProject(_3d,_41);
}
_3d.create();
_3d.hideDescrProject();
this.checkPosition();
return _3d;
}
}
if(this.arrProjects.length>0){
this.arrProjects[this.arrProjects.length-1].nextProject=_3d;
_3d.previousProject=this.arrProjects[this.arrProjects.length-1];
}
this.arrProjects.push(_3d);
_3d.create();
_3d.hideDescrProject();
this.checkPosition();
return _3d;
},openTree:function(_42){
var _43=this.getLastCloseParent(_42);
this.openNode(_43);
_42.taskItem.id!=_43.taskItem.id&&this.openTree(_42);
},openNode:function(_44){
if(!_44.isExpanded){
_f.remove(_44.cTaskNameItem[2],"ganttImageTreeExpand");
_f.add(_44.cTaskNameItem[2],"ganttImageTreeCollapse");
_44.isExpanded=true;
_44.shiftCurrentTasks(_44,_44.hideTasksHeight);
_44.showChildTasks(_44,_44.isExpanded);
_44.hideTasksHeight=0;
}
},getLastCloseParent:function(_45){
if(_45.parentTask){
if((!_45.parentTask.isExpanded)||(_45.parentTask.cTaskNameItem[2].style.display=="none")){
return this.getLastCloseParent(_45.parentTask);
}else{
return _45;
}
}else{
return _45;
}
},getProjectItemById:function(id){
return _9.filter(this.project,function(_46){
return _46.id==id;
},this)[0];
},clearAll:function(){
this.contentDataHeight=0;
this.startDate=null;
this.clearData();
this.clearItems();
this.clearEvents();
},clearEvents:function(){
_9.forEach(this._events,function(e){
e.remove();
});
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
var _47=this.project[i];
for(var k=0;k<_47.parentTasks.length;k++){
var _48=_47.parentTasks[k];
if(_48.startTime){
this.setStartTimeChild(_48);
}else{
return;
}
if(this.setPreviousTask(_47)){
return;
}
}
for(var k=0;k<_47.parentTasks.length;k++){
var _48=_47.parentTasks[k];
if(_48.startTime<_47.startDate){
return;
}
if(this.checkPosParentTaskInTree(_48)){
return;
}
}
this.sortTasksByStartTime(_47);
}
for(var i=0;i<this.project.length;i++){
var _47=this.project[i];
var _49=new _3(this,_47);
if(this.arrProjects.length>0){
var _4a=this.arrProjects[this.arrProjects.length-1];
_49.previousProject=_4a;
_4a.nextProject=_49;
}
_49.create();
this.checkHeighPanelTasks();
this.arrProjects.push(_49);
this.createTasks(_49);
}
this.resource&&this.resource.reConstruct();
this.postLoadData();
this.postBindEvents();
},loadJSONData:function(_4b){
var _4c=this;
_4c.dataFilePath=_4b||_4c.dataFilePath;
_c.get(_4c.dataFilePath,{sync:true}).then(function(_4d){
_4c.loadJSONString(_4d.text);
_4c.buildUIContent();
},function(){
});
},loadJSONString:function(_4e){
if(!_4e){
return;
}
this.clearAll();
var _4f=_15.parse(_4e);
var _50=_4f.items;
_9.forEach(_50,function(_51){
var _52=_51.startdate.split("-");
var _53=new _1({id:_51.id,name:_51.name,startDate:new Date(_52[0],(parseInt(_52[1])-1),_52[2])});
var _54=_51.tasks;
_9.forEach(_54,function(_55){
var id=_55.id,_56=_55.name,_57=_55.starttime.split("-"),_58=_55.duration,_59=_55.percentage,_5a=_55.previousTaskId,_5b=_55.taskOwner;
var _5c=new _5({id:id,name:_56,startTime:new Date(_57[0],(parseInt(_57[1])-1),_57[2]),duration:_58,percentage:_59,previousTaskId:_5a,taskOwner:_5b});
var _5d=_55.children;
if(_5d.length!=0){
this.buildChildTasksData(_5c,_5d);
}
_53.addTask(_5c);
},this);
this.addProject(_53);
},this);
},buildChildTasksData:function(_5e,_5f){
_5f&&_9.forEach(_5f,function(_60){
var id=_60.id,_61=_60.name,_62=_60.starttime.split("-"),_63=_60.duration,_64=_60.percentage,_65=_60.previousTaskId,_66=_60.taskOwner;
var _67=new _5({id:id,name:_61,startTime:new Date(_62[0],(parseInt(_62[1])-1),_62[2]),duration:_63,percentage:_64,previousTaskId:_65,taskOwner:_66});
_67.parentTask=_5e;
_5e.addChildTask(_67);
var _68=_60.children;
if(_68.length!=0){
this.buildChildTasksData(_67,_68);
}
},this);
},getJSONData:function(){
var _69={identifier:"id",items:[]};
_9.forEach(this.project,function(_6a){
var _6b={id:_6a.id,name:_6a.name,startdate:_6a.startDate.getFullYear()+"-"+(_6a.startDate.getMonth()+1)+"-"+_6a.startDate.getDate(),tasks:[]};
_69.items.push(_6b);
_9.forEach(_6a.parentTasks,function(_6c){
var _6d={id:_6c.id,name:_6c.name,starttime:_6c.startTime.getFullYear()+"-"+(_6c.startTime.getMonth()+1)+"-"+_6c.startTime.getDate(),duration:_6c.duration,percentage:_6c.percentage,previousTaskId:(_6c.previousTaskId||""),taskOwner:(_6c.taskOwner||""),children:this.getChildTasksData(_6c.cldTasks)};
_6b.tasks.push(_6d);
},this);
},this);
return _69;
},getChildTasksData:function(_6e){
var _6f=[];
_6e&&_6e.length>0&&_9.forEach(_6e,function(_70){
var _71={id:_70.id,name:_70.name,starttime:_70.startTime.getFullYear()+"-"+(_70.startTime.getMonth()+1)+"-"+_70.startTime.getDate(),duration:_70.duration,percentage:_70.percentage,previousTaskId:(_70.previousTaskId||""),taskOwner:(_70.taskOwner||""),children:this.getChildTasksData(_70.cldTasks)};
_6f.push(_71);
},this);
return _6f;
},saveJSONData:function(_72){
var _73=this;
_73.dataFilePath=(_72&&_a.trim(_72).length>0)?_72:this.dataFilePath;
try{
_c.post(_73.saveProgramPath,{query:{filename:_73.dataFilePath,data:_15.stringify(_73.getJSONData())}}).response.then(function(_74){
if((_d.checkStatus(_74.options.status))||(_74.options.status==405)){
}else{
}
});
}
catch(e){
}
},sortTaskStartTime:function(a,b){
return a.startTime<b.startTime?-1:(a.startTime>b.startTime?1:0);
},sortProjStartDate:function(a,b){
return a.startDate<b.startDate?-1:(a.startDate>b.startDate?1:0);
},setStartTimeChild:function(_75){
_9.forEach(_75.cldTasks,function(_76){
if(!_76.startTime){
_76.startTime=_75.startTime;
}
if(_76.cldTasks.length!=0){
this.setStartTimeChild(_76);
}
},this);
},createPanelTasks:function(){
var _77=_10.create("div",{className:"ganttTaskPanel"});
_11.set(_77,{height:(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px"});
return _77;
},refreshParams:function(_78){
this.pixelsPerDay=_78;
this.pixelsPerWorkHour=this.pixelsPerDay/this.hsPerDay;
this.pixelsPerHour=this.pixelsPerDay/24;
},createPanelNamesTasksHeader:function(){
var _79=_10.create("div",{className:"ganttPanelHeader"});
var _7a=_10.create("table",{cellPadding:"0px",border:"0px",cellSpacing:"0px",bgColor:"#FFFFFF",className:"ganttToolbar"},_79);
var _7b=_7a.insertRow(_7a.rows.length);
var _7c=_7a.insertRow(_7a.rows.length);
var _7d=_7a.insertRow(_7a.rows.length);
var _7e=_7a.insertRow(_7a.rows.length);
var _7f=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarZoomIn"},_7b);
var _80=_a.hitch(this,function(){
if(this.scale*2>5){
return;
}
this.scale=this.scale*2;
this.switchTeleMicroView(this.pixelsPerDay*this.scale);
});
if(this.zoomInClickEvent){
this.zoomInClickEvent.remove();
}
this.zoomInClickEvent=on(_7f,"click",_a.hitch(this,_80));
if(this.zoomInKeyEvent){
this.zoomInKeyEvent.remove();
}
this.zoomInKeyEvent=on(_7f,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
_80();
}));
_12.set(_7f,"tabIndex",0);
var _81=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarZoomOut"},_7b);
var _82=_a.hitch(this,function(){
if(this.scale*0.5<0.2){
return;
}
this.scale=this.scale*0.5;
this.switchTeleMicroView(this.pixelsPerDay*this.scale);
});
if(this.zoomOutClickEvent){
this.zoomOutClickEvent.remove();
}
this.zoomOutClickEvent=on(_81,"click",_a.hitch(this,_82));
if(this.zoomOutKeyEvent){
this.zoomOutKeyEvent.remove();
}
this.zoomOutKeyEvent=on(_81,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
_82();
}));
_12.set(_81,"tabIndex",0);
var _83=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarMicro"},_7c);
if(this.microClickEvent){
this.microClickEvent.remove();
}
this.microClickEvent=on(_83,"click",_a.hitch(this,this.refresh,this.animation?15:1,0,2));
if(this.microKeyEvent){
this.microKeyEvent.remove();
}
this.microKeyEvent=on(_83,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
_83.blur();
this.refresh(this.animation?15:1,0,2);
}));
_12.set(_83,"tabIndex",0);
var _84=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarTele"},_7c);
if(this.teleClickEvent){
this.teleClickEvent.remove();
}
this.teleClickEvent=on(_84,"click",_a.hitch(this,this.refresh,this.animation?15:1,0,0.5));
if(this.teleKeyEvent){
this.teleKeyEvent.remove();
}
this.teleKeyEvent=on(_84,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
_84.blur();
this.refresh(this.animation?15:1,0,0.5);
}));
_12.set(_84,"tabIndex",0);
var _85=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarSave"},_7d);
if(this.saveClickEvent){
this.saveClickEvent.remove();
}
this.saveClickEvent=on(_85,"click",_a.hitch(this,this.saveJSONData,""));
if(this.saveKeyEvent){
this.saveKeyEvent.remove();
}
this.saveKeyEvent=on(_85,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
this.saveJSONData("");
}));
_12.set(_85,"tabIndex",0);
var _86=_10.create("td",{align:"center",vAlign:"middle",className:"ganttToolbarLoad"},_7d);
if(this.loadClickEvent){
this.loadClickEvent.remove();
}
this.loadClickEvent=on(_86,"click",_a.hitch(this,this.loadJSONData,""));
if(this.loadKeyEvent){
this.loadKeyEvent.remove();
}
this.loadKeyEvent=on(_86,"keydown",_a.hitch(this,function(e){
if(e.keyCode!=_14.ENTER){
return;
}
this.loadJSONData("");
}));
_12.set(_86,"tabIndex",0);
var _87=[_7f,_81,_83,_84,_85,_86],_88=["Enlarge timeline","Shrink timeline","Zoom in time zone(microscope view)","Zoom out time zone(telescope view)","Save gantt data to json file","Load gantt data from json file"];
_9.forEach(_87,function(_89,i){
var _8a=_88[i];
var _8b=function(){
_f.add(_89,"ganttToolbarActionHover");
dijit.showTooltip(_8a,_89,["above","below"]);
};
_89.onmouseover=_8b;
_89.onfocus=_8b;
var _8c=function(){
_f.remove(_89,"ganttToolbarActionHover");
_89&&dijit.hideTooltip(_89);
};
_89.onmouseout=_8c;
_89.onblur=_8c;
},this);
return _79;
},createPanelNamesTasks:function(){
var _8d=_10.create("div",{innerHTML:"&nbsp;",className:"ganttPanelNames"});
_11.set(_8d,{height:(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px",width:this.maxWidthPanelNames+"px"});
return _8d;
},createPanelTime:function(){
var _8e=_10.create("div",{className:"ganttPanelTime"});
var _8f=_10.create("table",{cellPadding:"0px",border:"0px",cellSpacing:"0px",bgColor:"#FFFFFF",className:"ganttTblTime"},_8e);
this.totalDays=this.countDays;
var _90=_8f.insertRow(_8f.rows.length),_91,_92,_93=0;
_92=_91=new Date(this.startDate).getFullYear();
for(var i=0;i<this.countDays;i++,_93++){
var _94=new Date(this.startDate);
_94.setDate(_94.getDate()+i);
_92=_94.getFullYear();
if(_92!=_91){
this.addYearInPanelTime(_90,_93,_91);
_93=0;
_91=_92;
}
}
this.addYearInPanelTime(_90,_93,_92);
_11.set(_90,"display","none");
var _95=_8f.insertRow(_8f.rows.length),_96,_97,_98=0,_99=1970;
_97=_96=new Date(this.startDate).getMonth();
for(var i=0;i<this.countDays;i++,_98++){
var _94=new Date(this.startDate);
_94.setDate(_94.getDate()+i);
_97=_94.getMonth();
_99=_94.getFullYear();
if(_97!=_96){
this.addMonthInPanelTime(_95,_98,_96,_96!==11?_99:_99-1);
_98=0;
_96=_97;
}
}
this.addMonthInPanelTime(_95,_98,_97,_99);
var _9a=_8f.insertRow(_8f.rows.length),_9b,_9c,_98=0;
_9c=_9b=_b._getWeekOfYear(new Date(this.startDate));
for(var i=0;i<this.countDays;i++,_98++){
var _94=new Date(this.startDate);
_94.setDate(_94.getDate()+i);
_9c=_b._getWeekOfYear(_94);
if(_9c!=_9b){
this.addWeekInPanelTime(_9a,_98,_9b);
_98=0;
_9b=_9c;
}
}
this.addWeekInPanelTime(_9a,_98,_9c);
var _9d=_8f.insertRow(_8f.rows.length);
for(var i=0;i<this.countDays;i++){
this.addDayInPanelTime(_9d);
}
var _9e=_8f.insertRow(_8f.rows.length);
for(var i=0;i<this.countDays;i++){
this.addHourInPanelTime(_9e);
}
_11.set(_9e,"display","none");
return _8e;
},adjustPanelTime:function(){
var _9f=_9.map(this.arrProjects,function(_a0){
return (parseInt(_a0.projectItem[0].style.left)+parseInt(_a0.projectItem[0].firstChild.style.width)+_a0.descrProject.offsetWidth+this.panelTimeExpandDelta);
},this).sort(function(a,b){
return b-a;
})[0];
if(this.maxTaskEndPos!=_9f){
var _a1=this.panelTime.firstChild.firstChild.rows;
for(var i=0;i<=4;i++){
this.removeCell(_a1[i]);
}
var _a2=Math.round((_9f+this.panelTimeExpandDelta)/this.pixelsPerDay);
this.totalDays=_a2;
var _a3,_a4,_a5=0;
_a4=_a3=new Date(this.startDate).getFullYear();
for(var i=0;i<_a2;i++,_a5++){
var _a6=new Date(this.startDate);
_a6.setDate(_a6.getDate()+i);
_a4=_a6.getFullYear();
if(_a4!=_a3){
this.addYearInPanelTime(_a1[0],_a5,_a3);
_a5=0;
_a3=_a4;
}
}
this.addYearInPanelTime(_a1[0],_a5,_a4);
var _a7,_a8,_a9=0,_aa=1970;
_a8=_a7=new Date(this.startDate).getMonth();
for(var i=0;i<_a2;i++,_a9++){
var _a6=new Date(this.startDate);
_a6.setDate(_a6.getDate()+i);
_a8=_a6.getMonth();
_aa=_a6.getFullYear();
if(_a8!=_a7){
this.addMonthInPanelTime(_a1[1],_a9,_a7,_a7!==11?_aa:_aa-1);
_a9=0;
_a7=_a8;
}
}
this.addMonthInPanelTime(_a1[1],_a9,_a8,_aa);
var _ab,_ac,_a9=0;
_ac=_ab=_b._getWeekOfYear(new Date(this.startDate));
for(var i=0;i<_a2;i++,_a9++){
var _a6=new Date(this.startDate);
_a6.setDate(_a6.getDate()+i);
_ac=_b._getWeekOfYear(_a6);
if(_ac!=_ab){
this.addWeekInPanelTime(_a1[2],_a9,_ab);
_a9=0;
_ab=_ac;
}
}
this.addWeekInPanelTime(_a1[2],_a9,_ac);
for(var i=0;i<_a2;i++){
this.addDayInPanelTime(_a1[3]);
}
for(var i=0;i<_a2;i++){
this.addHourInPanelTime(_a1[4]);
}
this.panelTime.firstChild.firstChild.style.width=this.pixelsPerDay*(_a1[3].cells.length)+"px";
this.contentData.firstChild.style.width=this.pixelsPerDay*(_a1[3].cells.length)+"px";
this.maxTaskEndPos=_9f;
}
},addYearInPanelTime:function(row,_ad,_ae){
var _af="Year   "+_ae;
var _b0=_10.create("td",{colSpan:_ad,align:"center",vAlign:"middle",className:"ganttYearNumber",innerHTML:this.pixelsPerDay*_ad>20?_af:"",innerHTMLData:_af},row);
_11.set(_b0,"width",(this.pixelsPerDay*_ad)+"px");
},addMonthInPanelTime:function(row,_b1,_b2,_b3){
var _b4=this.months[_b2]+(_b3?" of "+_b3:"");
var _b5=_10.create("td",{colSpan:_b1,align:"center",vAlign:"middle",className:"ganttMonthNumber",innerHTML:this.pixelsPerDay*_b1>30?_b4:"",innerHTMLData:_b4},row);
_11.set(_b5,"width",(this.pixelsPerDay*_b1)+"px");
},addWeekInPanelTime:function(row,_b6,_b7){
var _b8="Week   "+_b7;
var _b9=_10.create("td",{colSpan:_b6,align:"center",vAlign:"middle",className:"ganttWeekNumber",innerHTML:this.pixelsPerDay*_b6>20?_b8:"",innerHTMLData:_b8},row);
_11.set(_b9,"width",(this.pixelsPerDay*_b6)+"px");
},addDayInPanelTime:function(row){
var _ba=new Date(this.startDate);
_ba.setDate(_ba.getDate()+parseInt(row.cells.length));
var _bb=_10.create("td",{align:"center",vAlign:"middle",className:"ganttDayNumber",innerHTML:this.pixelsPerDay>20?_ba.getDate():"",innerHTMLData:String(_ba.getDate()),data:row.cells.length},row);
_11.set(_bb,"width",this.pixelsPerDay+"px");
(_ba.getDay()>=5)&&_f.add(_bb,"ganttDayNumberWeekend");
this._events.push(on(_bb,"mouseover",_a.hitch(this,function(_bc){
var _bd=_bc.target||_bc.srcElement;
var _be=new Date(this.startDate.getTime());
_be.setDate(_be.getDate()+parseInt(_12.get(_bd,"data")));
dijit.showTooltip(_be.getFullYear()+"."+(_be.getMonth()+1)+"."+_be.getDate(),_bb,["above","below"]);
})));
this._events.push(on(_bb,"mouseout",_a.hitch(this,function(_bf){
var _c0=_bf.target||_bf.srcElement;
_c0&&dijit.hideTooltip(_c0);
})));
},addHourInPanelTime:function(row){
var _c1=_10.create("td",{align:"center",vAlign:"middle",className:"ganttHourNumber",data:row.cells.length},row);
_11.set(_c1,"width",this.pixelsPerDay+"px");
var _c2=_10.create("table",{cellPadding:"0",cellSpacing:"0"},_c1);
var _c3=_c2.insertRow(_c2.rows.length);
for(var i=0;i<this.hsPerDay;i++){
var _c4=_10.create("td",{className:"ganttHourClass"},_c3);
_11.set(_c4,"width",(this.pixelsPerDay/this.hsPerDay)+"px");
_12.set(_c4,"innerHTMLData",String(9+i));
if(this.pixelsPerDay/this.hsPerDay>5){
_12.set(_c4,"innerHTML",String(9+i));
}
_f.add(_c4,i<=3?"ganttHourNumberAM":"ganttHourNumberPM");
}
},incHeightPanelTasks:function(_c5){
var _c6=this.contentData.firstChild;
_c6.style.height=parseInt(_c6.style.height)+_c5+"px";
},incHeightPanelNames:function(_c7){
var _c8=this.panelNames.firstChild;
_c8.style.height=parseInt(_c8.style.height)+_c7+"px";
},checkPosition:function(){
_9.forEach(this.arrProjects,function(_c9){
_9.forEach(_c9.arrTasks,function(_ca){
_ca.checkPosition();
},this);
},this);
},checkHeighPanelTasks:function(){
this.contentDataHeight+=this.heightTaskItemExtra+this.heightTaskItem;
if((parseInt(this.contentData.firstChild.style.height)<=this.contentDataHeight)){
this.incHeightPanelTasks(this.heightTaskItem+this.heightTaskItemExtra);
this.incHeightPanelNames(this.heightTaskItem+this.heightTaskItemExtra);
}
},sortTasksByStartTime:function(_cb){
_cb.parentTasks.sort(this.sortTaskStartTime);
for(var i=0;i<_cb.parentTasks.length;i++){
_cb.parentTasks[i]=this.sortChildTasks(_cb.parentTasks[i]);
}
},sortChildTasks:function(_cc){
_cc.cldTasks.sort(this.sortTaskStartTime);
for(var i=0;i<_cc.cldTasks.length;i++){
if(_cc.cldTasks[i].cldTasks.length>0){
this.sortChildTasks(_cc.cldTasks[i]);
}
}
return _cc;
},refresh:function(_cd,_ce,_cf){
if(this.arrProjects.length<=0){
return;
}
if(this.arrProjects[0].arrTasks.length<=0){
return;
}
if(!_cd||_ce>_cd){
this.refreshController();
if(this.resource){
this.resource.refresh();
}
this.tempDayInPixels=0;
this.panelNameHeadersCover&&_11.set(this.panelNameHeadersCover,"display","none");
return;
}
if(this.tempDayInPixels==0){
this.tempDayInPixels=this.pixelsPerDay;
}
this.panelNameHeadersCover&&_11.set(this.panelNameHeadersCover,"display","");
var dip=this.tempDayInPixels+this.tempDayInPixels*(_cf-1)*Math.pow((_ce/_cd),2);
this.refreshParams(dip);
_9.forEach(this.arrProjects,function(_d0){
_9.forEach(_d0.arrTasks,function(_d1){
_d1.refresh();
},this);
_d0.refresh();
},this);
setTimeout(_a.hitch(this,function(){
this.refresh(_cd,++_ce,_cf);
}),15);
},switchTeleMicroView:function(dip){
var _d2=this.panelTime.firstChild.firstChild;
for(var i=0;i<5;i++){
if(dip>40){
_11.set(_d2.rows[i],"display",(i==0||i==1)?"none":"");
}else{
if(dip<20){
_11.set(_d2.rows[i],"display",(i==2||i==4)?"none":"");
}else{
_11.set(_d2.rows[i],"display",(i==0||i==4)?"none":"");
}
}
}
},refreshController:function(){
this.contentData.firstChild.style.width=Math.max(1200,this.pixelsPerDay*this.totalDays)+"px";
this.panelTime.firstChild.style.width=this.pixelsPerDay*this.totalDays+"px";
this.panelTime.firstChild.firstChild.style.width=this.pixelsPerDay*this.totalDays+"px";
this.switchTeleMicroView(this.pixelsPerDay);
_9.forEach(this.panelTime.firstChild.firstChild.rows,function(row){
_9.forEach(row.childNodes,function(td){
var cs=parseInt(_12.get(td,"colSpan")||1);
var _d3=_a.trim(_12.get(td,"innerHTMLData")||"");
if(_d3.length>0){
_12.set(td,"innerHTML",this.pixelsPerDay*cs<20?"":_d3);
}else{
_9.forEach(td.firstChild.rows[0].childNodes,function(td){
var _d4=_a.trim(_12.get(td,"innerHTMLData")||"");
_12.set(td,"innerHTML",this.pixelsPerDay/this.hsPerDay>10?_d4:"");
},this);
}
if(cs==1){
_11.set(td,"width",(this.pixelsPerDay*cs)+"px");
if(_d3.length<=0){
_9.forEach(td.firstChild.rows[0].childNodes,function(td){
_11.set(td,"width",(this.pixelsPerDay*cs/this.hsPerDay)+"px");
},this);
}
}
},this);
},this);
},init:function(){
this.startDate=this.getStartDate();
_11.set(this.content,{width:this.contentWidth+"px",height:this.contentHeight+"px"});
this.tableControl=_10.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttTabelControl"});
var _d5=this.tableControl.insertRow(this.tableControl.rows.length);
this.content.appendChild(this.tableControl);
this.countDays=this.getCountDays();
this.panelTime=_10.create("div",{className:"ganttPanelTimeContainer"});
_11.set(this.panelTime,"height",this.panelTimeHeight+"px");
this.panelTime.appendChild(this.createPanelTime());
this.contentData=_10.create("div",{className:"ganttContentDataContainer"});
_11.set(this.contentData,"height",(this.contentHeight-this.panelTimeHeight)+"px");
this.contentData.appendChild(this.createPanelTasks());
var _d6=_10.create("td",{vAlign:"top"});
this.panelNameHeaders=_10.create("div",{className:"ganttPanelNameHeaders"},_d6);
_11.set(this.panelNameHeaders,{height:this.panelTimeHeight+"px",width:this.maxWidthPanelNames+"px"});
this.panelNameHeaders.appendChild(this.createPanelNamesTasksHeader());
this.panelNames=_10.create("div",{className:"ganttPanelNamesContainer"},_d6);
this.panelNames.appendChild(this.createPanelNamesTasks());
_d5.appendChild(_d6);
_d6=_10.create("td",{vAlign:"top"});
var _d7=_10.create("div",{className:"ganttDivCell"});
_d7.appendChild(this.panelTime);
_d7.appendChild(this.contentData);
_d6.appendChild(_d7);
_d5.appendChild(_d6);
_11.set(this.panelNames,"height",(this.contentHeight-this.panelTimeHeight-this.scrollBarWidth)+"px");
_11.set(this.panelNames,"width",this.maxWidthPanelNames+"px");
_11.set(this.contentData,"width",(this.contentWidth-this.maxWidthPanelNames)+"px");
_11.set(this.contentData.firstChild,"width",this.pixelsPerDay*this.countDays+"px");
_11.set(this.panelTime,"width",(this.contentWidth-this.maxWidthPanelNames-this.scrollBarWidth)+"px");
_11.set(this.panelTime.firstChild,"width",this.pixelsPerDay*this.countDays+"px");
if(this.isShowConMenu){
this.tabMenu=new _6(this);
}
var _d8=this;
this.contentData.onscroll=function(){
_d8.panelTime.scrollLeft=this.scrollLeft;
if(_d8.panelNames){
_d8.panelNames.scrollTop=this.scrollTop;
if(_d8.isShowConMenu){
_d8.tabMenu.hide();
}
}
if(_d8.resource){
_d8.resource.contentData.scrollLeft=this.scrollLeft;
}
};
this.project.sort(this.sortProjStartDate);
for(var i=0;i<this.project.length;i++){
var _d9=this.project[i];
for(var k=0;k<_d9.parentTasks.length;k++){
var _da=_d9.parentTasks[k];
if(!_da.startTime){
_da.startTime=_d9.startDate;
}
this.setStartTimeChild(_da);
if(this.setPreviousTask(_d9)){
return;
}
}
for(var k=0;k<_d9.parentTasks.length;k++){
var _da=_d9.parentTasks[k];
if(_da.startTime<_d9.startDate){
if(!this.correctError){
return;
}else{
_da.startTime=_d9.startDate;
}
}
if(this.checkPosParentTaskInTree(_da)){
return;
}
}
this.sortTasksByStartTime(_d9);
}
for(var i=0;i<this.project.length;i++){
var _d9=this.project[i];
var _db=new _3(this,_d9);
if(this.arrProjects.length>0){
var _dc=this.arrProjects[this.arrProjects.length-1];
_db.previousProject=_dc;
_dc.nextProject=_db;
}
_db.create();
this.checkHeighPanelTasks();
this.arrProjects.push(_db);
this.createTasks(_db);
}
if(this.withResource){
this.resource=new _2(this);
this.resource.create();
}
this.postLoadData();
this.postBindEvents();
return this;
},postLoadData:function(){
_9.forEach(this.arrProjects,function(_dd){
_9.forEach(_dd.arrTasks,function(_de){
_de.postLoadData();
},this);
_dd.postLoadData();
},this);
var _df=_13.getMarginBox(this.panelNameHeaders);
if(!this.panelNameHeadersCover){
this.panelNameHeadersCover=_10.create("div",{className:"ganttHeaderCover"},this.panelNameHeaders.parentNode);
_11.set(this.panelNameHeadersCover,{left:_df.l+"px",top:_df.t+"px",height:_df.h+"px",width:_df.w+"px",display:"none"});
}
},postBindEvents:function(){
var pos=_13.position(this.tableControl,true);
has("dom-addeventlistener")&&this._events.push(on(this.tableControl,"mousemove",_a.hitch(this,function(_e0){
var _e1=_e0.srcElement||_e0.target;
if(_e1==this.panelNames.firstChild||_e1==this.contentData.firstChild){
var _e2=this.heightTaskItem+this.heightTaskItemExtra;
var _e3=parseInt(_e0.layerY/_e2)*_e2+this.panelTimeHeight-this.contentData.scrollTop;
if(_e3!=this.oldHLTop&&_e3<(pos.h-50)){
if(this.highLightDiv){
_11.set(this.highLightDiv,"top",(pos.y+_e3)+"px");
}else{
this.highLightDiv=_10.create("div",{className:"ganttRowHighlight"},win.body());
_11.set(this.highLightDiv,{top:(pos.y+_e3)+"px",left:pos.x+"px",width:(pos.w-20)+"px",height:_e2+"px"});
}
}
this.oldHLTop=_e3;
}
})));
},getStartDate:function(){
_9.forEach(this.project,function(_e4){
if(this.startDate){
if(_e4.startDate<this.startDate){
this.startDate=new Date(_e4.startDate);
}
}else{
this.startDate=new Date(_e4.startDate);
}
},this);
this.initialPos=24*this.pixelsPerHour;
return this.startDate?new Date(this.startDate.setHours(this.startDate.getHours()-24)):new Date();
},getCountDays:function(){
return parseInt((this.contentWidth-this.maxWidthPanelNames)/(this.pixelsPerHour*24));
},createTasks:function(_e5){
_9.forEach(_e5.project.parentTasks,function(_e6,i){
if(i>0){
_e5.project.parentTasks[i-1].nextParentTask=_e6;
_e6.previousParentTask=_e5.project.parentTasks[i-1];
}
var _e7=new _4(_e6,_e5,this);
_e5.arrTasks.push(_e7);
_e7.create();
this.checkHeighPanelTasks();
if(_e6.cldTasks.length>0){
this.createChildItemControls(_e6.cldTasks,_e5);
}
},this);
},createChildItemControls:function(_e8,_e9){
_e8&&_9.forEach(_e8,function(_ea,i){
if(i>0){
_ea.previousChildTask=_e8[i-1];
_e8[i-1].nextChildTask=_ea;
}
var _eb=new _4(_ea,_e9,this);
_eb.create();
this.checkHeighPanelTasks();
if(_ea.cldTasks.length>0){
this.createChildItemControls(_ea.cldTasks,_e9);
}
},this);
},getPosOnDate:function(_ec){
return (_ec-this.startDate)/(60*60*1000)*this.pixelsPerHour;
},getWidthOnDuration:function(_ed){
return Math.round(this.pixelsPerWorkHour*_ed);
},getLastChildTask:function(_ee){
return _ee.childTask.length>0?this.getLastChildTask(_ee.childTask[_ee.childTask.length-1]):_ee;
},removeCell:function(row){
while(row.cells[0]){
row.deleteCell(row.cells[0]);
}
}});
});
