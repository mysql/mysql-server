//>>built
define(["dijit","dojo","dojox","dojo/require!dojo/date/locale"],function(_1,_2,_3){
_2.provide("dojox.gantt.GanttResourceItem");
_2.require("dojo.date.locale");
_2.declare("dojox.gantt.GanttResourceItem",null,{constructor:function(_4){
this.ganttChart=_4;
this.ownerItem=[];
this.ownerNameItem=[];
this.ownerTaskNodeMapping={};
this.ownerTaskNodeMapping_time={};
this.resourceInfo={};
this.ownerTimeConsume={};
},clearAll:function(){
this.clearData();
this.clearItems();
},clearData:function(){
this.ownerItem=[];
this.ownerNameItem=[];
this.ownerTaskNodeMapping={};
this.ownerTaskNodeMapping_time={};
this.resourceInfo={};
this.ownerTimeConsume={};
},clearItems:function(){
if(this.content.firstChild){
_2.destroy(this.content.firstChild);
}
},buildResource:function(){
var _5={};
_2.forEach(this.ganttChart.arrProjects,function(_6){
_2.forEach(_6.arrTasks,function(_7){
_7.buildResourceInfo(_5);
},this);
},this);
return _5;
},buildOwnerTimeConsume:function(){
var _8={};
for(var _9 in this.resourceInfo){
var _a=this.resourceInfo[_9];
var _b={};
for(var i=0;i<_a.length;i++){
var _c=_a[i];
var _d=_c.taskItem.startTime.getTime(),_e=_c.taskItem.duration*24*60*60*1000/this.ganttChart.hsPerDay;
_b.min=_b.min?Math.min(_b.min,_d):_d;
_b.max=_b.max?Math.max(_b.max,(_d+_e)):(_d+_e);
}
_b.dur=(_b.max-_b.min)*this.ganttChart.hsPerDay/(24*60*60*1000);
_b.min=new Date(_b.min);
_b.max=new Date(_b.max);
_8[_9]=_b;
}
return _8;
},refresh:function(){
this.ownerTimeConsume=this.buildOwnerTimeConsume();
this.contentData.firstChild.style.width=Math.max(1200,this.ganttChart.pixelsPerDay*this.ganttChart.totalDays)+"px";
for(var _f in this.resourceInfo){
this.refreshOwnerEntry(_f);
}
},reConstruct:function(){
this.clearAll();
this.resourceInfo=this.buildResource();
this.ownerTimeConsume=this.buildOwnerTimeConsume();
this.tableControl=_2.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttResourceTableControl"});
var _10=this.tableControl.insertRow(this.tableControl.rows.length);
this.contentHeight=this.content.offsetHeight;
this.contentWidth=this.content.offsetWidth;
this.content.appendChild(this.tableControl);
this.contentData=_2.create("div",{className:"ganttResourceContentDataContainer"});
this.contentData.appendChild(this.createPanelOwners());
_2.style(this.contentData,"height",(this.contentHeight-this.ganttChart.panelTimeHeight)+"px");
var _11=_2.create("td",{vAlign:"top"});
this.panelNames=_2.create("div",{className:"ganttResourcePanelNames"});
this.panelNames.appendChild(this.createPanelNamesOwners());
_11.appendChild(this.panelNames);
_10.appendChild(_11);
_11=_2.create("td",{vAlign:"top"});
var _12=_2.create("div",{className:"ganttResourceDivCell"});
_12.appendChild(this.contentData);
_11.appendChild(_12);
_10.appendChild(_11);
_2.style(this.panelNames,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px",width:this.ganttChart.maxWidthPanelNames+"px"});
this.contentData.style.width=(this.contentWidth-this.ganttChart.maxWidthPanelNames)+"px";
this.contentData.firstChild.style.width=this.ganttChart.pixelsPerDay*(this.ganttChart.panelTime.firstChild.firstChild.rows[3].cells.length)+"px";
var _13=this;
this.contentData.onscroll=function(){
if(_13.panelNames){
_13.panelNames.scrollTop=this.scrollTop;
}
};
this.contentData.scrollLeft=this.ganttChart.contentData.scrollLeft;
for(var _14 in this.resourceInfo){
this.createOwnerEntry(_14);
}
this.postAdjustment();
},create:function(){
var _15=_2.create("div",{innerHTML:"Resource Chart:",className:"ganttResourceHeader"},this.ganttChart.content,"after");
_2.style(_15,"width",this.ganttChart.contentWidth+"px");
var _16=_2.create("div",{className:"ganttResourceContent"},_15,"after");
_2.style(_16,{width:this.ganttChart.contentWidth+"px",height:(this.ganttChart.resourceChartHeight||(this.ganttChart.contentHeight*0.8))+"px"});
this.content=_16||this.content;
this.reConstruct();
},postAdjustment:function(){
this.contentData.firstChild.style.height=(this.ownerItem.length*23)+"px";
this.panelNames.firstChild.style.height=(this.ownerItem.length*23)+"px";
},refreshOwnerEntry:function(_17){
this.refreshOwnerItem(_17);
_2.forEach(this.resourceInfo[_17],function(_18,i){
var _19=this.ownerTaskNodeMapping[_17].tasks[i][0];
this.refreshDetailedTaskEntry(_17,_19,_18);
},this);
},createOwnerEntry:function(_1a){
var _1b=this.contentData.firstChild;
var _1c=this.ownerItem[this.ownerItem.length-1];
this.ownerTaskNodeMapping[_1a]={};
this.ownerTaskNodeMapping[_1a][_1a]=[];
var pos=_2.position(_1b);
var _1d=(_1c?parseInt(_1c.style.top):(6-23))+this.ganttChart.heightTaskItem+11;
var _1e=this.createOwnerItem(_1a,_1d);
_1b.appendChild(_1e);
this.ownerItem.push(_1e);
this.ownerTaskNodeMapping[_1a][_1a].push(_1e);
if(this.panelNames){
var _1f=this.createOwnerNameItem(_1a,_1d);
this.panelNames.firstChild.appendChild(_1f);
this.ownerNameItem.push(_1f);
this.ownerTaskNodeMapping[_1a][_1a].push(_1f);
}
var _20=this.ownerItem[this.ownerNameItem.length-1],_21=this.ownerNameItem[this.ownerNameItem.length-1];
if(this.panelNames){
this.checkWidthTaskNameItem(_21);
var _22=this.createTreeImg(_21);
this.panelNames.firstChild.appendChild(_22);
this.ownerTaskNodeMapping[_1a][_1a].push(_22);
}
this.ownerTaskNodeMapping[_1a]["taskCount"]=this.resourceInfo[_1a].length;
this.ownerTaskNodeMapping[_1a]["isOpen"]=false;
this.ownerTaskNodeMapping[_1a]["tasks"]=[];
_2.forEach(this.resourceInfo[_1a],function(_23){
this.ownerTaskNodeMapping[_1a]["tasks"].push(this.createDetailedTaskEntry(_1a,_21,_23));
},this);
return this;
},createOwnerNameItem:function(_24,_25){
var _26=_2.create("div",{id:_24,title:_24,innerHTML:_24,className:"ganttOwnerNameItem"});
_2.style(_26,"top",_25+"px");
return _26;
},refreshOwnerItem:function(_27){
var _28=this.ownerTaskNodeMapping[_27][_27][0],_29=this.ownerTimeConsume[_27].min,end=this.ownerTimeConsume[_27].max,dur=this.ownerTimeConsume[_27].dur,_2a=this.ganttChart.getPosOnDate(_29);
_28.style.left=_2a+"px";
_28.style.width=dur*this.ganttChart.pixelsPerWorkHour+"px";
_2.forEach(this.resourceInfo[_27],function(_2b,i){
var _2c=this.ganttChart.getPosOnDate(_2b.taskItem.startTime);
_2.style(_28.childNodes[i],{left:(_2c-_2a)+"px",width:_2b.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
},this);
},createOwnerItem:function(_2d,_2e){
var _2f=this.ownerTimeConsume[_2d].min,end=this.ownerTimeConsume[_2d].max,dur=this.ownerTimeConsume[_2d].dur;
var _30=this.ganttChart.getPosOnDate(_2f);
var _31=_2.create("div",{id:_2d,owner:true,className:"ganttOwnerBar"});
_2.style(_31,{left:_30+"px",top:_2e+"px",width:dur*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
_2.forEach(this.resourceInfo[_2d],function(_32){
var _33=_2.create("div",{id:_2d,className:"ganttOwnerTaskBar"},_31);
var _34=this.ganttChart.getPosOnDate(_32.taskItem.startTime);
_2.style(_33,{left:(_34-_30)+"px",width:_32.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
},this);
return _31;
},refreshDetailedTaskEntry:function(_35,_36,_37){
this.refreshTaskItem(_36,_37);
},createDetailedTaskEntry:function(_38,_39,_3a){
var _3b=[];
var _3c=this.contentData.firstChild;
var _3d=parseInt(_39.style.top);
var _3e=this.createTaskItem(_3a,_3d);
_3e.style.display="none";
_3c.appendChild(_3e);
this.ownerItem.push(_3e);
_3b.push(_3e);
if(this.panelNames){
var _3f=this.createTaskNameItem(_3a.taskItem.name,_3d);
this.panelNames.firstChild.appendChild(_3f);
_3f.style.display="none";
this.ownerNameItem.push(_3f);
_3b.push(_3f);
}
if(this.panelNames){
this.ownerNameItem[this.ownerNameItem.length-1].style.left=_2.style(_39,"left")+15+"px";
var _40=this.createConnectingLinesPN(_39,this.ownerNameItem[this.ownerNameItem.length-1]);
_2.forEach(_40,function(_41){
_41.style.display="none";
},this);
_3b.push({"v":_40[0],"h":_40[1]});
this.checkWidthTaskNameItem(this.ownerNameItem[this.ownerNameItem.length-1]);
}
return _3b;
},createTaskNameItem:function(_42,_43){
var _44=_2.create("div",{id:_42,className:"ganttTaskNameItem",title:_42,innerHTML:_42});
_2.style(_44,"top",_43+"px");
return _44;
},refreshTaskItem:function(_45,_46){
var _47=this.ganttChart.getPosOnDate(_46.taskItem.startTime);
_2.style(_45,{left:_47+"px",width:_46.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
},createTaskItem:function(_48,_49){
var _4a=this.ganttChart.getPosOnDate(_48.taskItem.startTime);
var _4b=_2.create("div",{id:_48.taskItem.name,className:"ganttTaskBar"});
_2.style(_4b,{left:_4a+"px",top:_49+"px",width:_48.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
return _4b;
},createConnectingLinesPN:function(_4c,_4d){
var _4e=[];
var _4f=_2.create("div",{innerHTML:"&nbsp;",className:"ganttResourceLineVerticalLeft"},this.panelNames.firstChild);
_4f.cNode=_4d;
_4f.pNode=_4c;
var _50=_2.create("div",{noShade:true,color:"#000",className:"ganttResourceLineHorizontalLeft"},this.panelNames.firstChild);
_50.cNode=_4d;
_50.pNode=_4c;
this.panelNames.firstChild.appendChild(_50);
_4e.push(_4f);
_4e.push(_50);
return _4e;
},createTreeImg:function(_51){
var _52=_2.create("div",{id:_51.id,className:"ganttImageTreeExpand"});
_2.attr(_52,"tabIndex",0);
var _53=this.ownerTaskNodeMapping[_51.id];
_2.forEach(["onclick","onkeydown"],function(e){
this.ganttChart._events.push(_2.connect(_52,e,this,function(evt){
var _54=false,_55,_56;
if(e=="onkeydown"&&evt.keyCode!=_2.keys.ENTER){
return;
}
if(_53.isOpen){
_2.removeClass(_52,"ganttImageTreeCollapse");
_2.addClass(_52,"ganttImageTreeExpand");
_53.isOpen=false;
for(_55 in this.ownerTaskNodeMapping){
_56=this.ownerTaskNodeMapping[_55];
if(_54){
_2.forEach(_56[_55],function(_57){
_2.style(_57,"top",_2.style(_57,"top")-_53.taskCount*23+"px");
});
_2.forEach(_56.tasks,function(_58){
_2.forEach(_58,function(_59){
var _5a=!_59.v&&!_59.h?[_59]:[_59.v,_59.h];
_2.forEach(_5a,function(t){
_2.style(t,"top",_2.style(t,"top")-_53.taskCount*23+"px");
});
});
});
}else{
if(_55==_51.id){
_54=true;
_2.forEach(_56.tasks,function(_5b,i){
_2.forEach(_5b,function(_5c){
this.styleOwnerItem(_5c,_56[_55][0],"none",0);
},this);
},this);
}
}
}
}else{
_2.removeClass(_52,"ganttImageTreeExpand");
_2.addClass(_52,"ganttImageTreeCollapse");
_53.isOpen=true;
for(_55 in this.ownerTaskNodeMapping){
_56=this.ownerTaskNodeMapping[_55];
if(_54){
_2.forEach(_56[_55],function(_5d){
_2.style(_5d,"top",_2.style(_5d,"top")+_53.taskCount*23+"px");
});
_2.forEach(_56.tasks,function(_5e){
_2.forEach(_5e,function(_5f){
var _60=!_5f.v&&!_5f.h?[_5f]:[_5f.v,_5f.h];
_2.forEach(_60,function(t){
_2.style(t,"top",_2.style(t,"top")+_53.taskCount*23+"px");
});
});
});
}else{
if(_55==_51.id){
_54=true;
_2.forEach(_56.tasks,function(_61,i){
_2.forEach(_61,function(_62){
this.styleOwnerItem(_62,_56[_55][0],"inline",(i+1)*23);
},this);
},this);
}
}
}
}
}));
},this);
_2.addClass(_52,"ganttResourceTreeImage");
_2.style(_52,{left:(_2.style(_51,"left")-12)+"px",top:(_2.style(_51,"top")+3)+"px"});
return _52;
},styleOwnerItem:function(_63,_64,_65,_66){
if(_63.v||_63.h){
_2.style(_63.v,{height:Math.max(1,(_63.v.cNode.offsetTop-_63.v.pNode.offsetTop))+"px",top:(_63.v.pNode.offsetTop+5)+"px",left:(_63.v.pNode.offsetLeft-9)+"px",display:_65});
_2.style(_63.h,{width:Math.max(1,(_63.h.cNode.offsetLeft-_63.h.pNode.offsetLeft+4))+"px",top:(_63.h.cNode.offsetTop+5)+"px",left:(_63.h.pNode.offsetLeft-9)+"px",display:_65});
}else{
_2.style(_63,{display:_65,top:parseInt(_64.style.top)+_66+"px"});
}
},checkWidthTaskNameItem:function(_67){
if(_67&&_67.offsetWidth+_67.offsetLeft>this.ganttChart.maxWidthPanelNames){
var _68=_67.offsetWidth+_67.offsetLeft-this.ganttChart.maxWidthPanelNames,_69=Math.round(_68/(_67.offsetWidth/_67.firstChild.length)),_6a=_67.id.substring(0,_67.firstChild.length-_69-3);
_67.innerHTML=_6a+"...";
}
},createPanelOwners:function(){
var _6b=_2.create("div",{className:"ganttOwnerPanel"});
_2.style(_6b,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px"});
return _6b;
},createPanelNamesOwners:function(){
var _6c=_2.create("div",{innerHTML:"&nbsp;",className:"ganttResourcePanelNamesOwners"});
_2.style(_6c,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px",width:this.ganttChart.maxWidthPanelNames+"px"});
return _6c;
}});
});
