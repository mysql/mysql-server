//>>built
define("dojox/gantt/GanttResourceItem",["dojo/_base/declare","dojo/_base/array","dojo/_base/lang","dojo/date/locale","dojo/request","dojo/on","dojo/dom","dojo/dom-class","dojo/dom-construct","dojo/dom-style","dojo/dom-attr","dojo/dom-geometry","dojo/keys","dojo/domReady!"],function(_1,_2,_3,_4,_5,on,_6,_7,_8,_9,_a,_b,_c){
return _1("dojox.gantt.GanttResourceItem",[],{constructor:function(_d){
this.ganttChart=_d;
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
_8.destroy(this.content.firstChild);
}
},buildResource:function(){
var _e={};
_2.forEach(this.ganttChart.arrProjects,function(_f){
_2.forEach(_f.arrTasks,function(_10){
_10.buildResourceInfo(_e);
},this);
},this);
return _e;
},buildOwnerTimeConsume:function(){
var _11={};
for(var _12 in this.resourceInfo){
var _13=this.resourceInfo[_12];
var _14={};
for(var i=0;i<_13.length;i++){
var _15=_13[i];
var _16=_15.taskItem.startTime.getTime(),dur=_15.taskItem.duration*24*60*60*1000/this.ganttChart.hsPerDay;
_14.min=_14.min?Math.min(_14.min,_16):_16;
_14.max=_14.max?Math.max(_14.max,(_16+dur)):(_16+dur);
}
_14.dur=(_14.max-_14.min)*this.ganttChart.hsPerDay/(24*60*60*1000);
_14.min=new Date(_14.min);
_14.max=new Date(_14.max);
_11[_12]=_14;
}
return _11;
},refresh:function(){
this.ownerTimeConsume=this.buildOwnerTimeConsume();
this.contentData.firstChild.style.width=Math.max(1200,this.ganttChart.pixelsPerDay*this.ganttChart.totalDays)+"px";
for(var _17 in this.resourceInfo){
this.refreshOwnerEntry(_17);
}
},reConstruct:function(){
this.clearAll();
this.resourceInfo=this.buildResource();
this.ownerTimeConsume=this.buildOwnerTimeConsume();
this.tableControl=_8.create("table",{cellPadding:"0",cellSpacing:"0",className:"ganttResourceTableControl"});
var _18=this.tableControl.insertRow(this.tableControl.rows.length);
this.contentHeight=this.content.offsetHeight;
this.contentWidth=this.content.offsetWidth;
this.content.appendChild(this.tableControl);
this.contentData=_8.create("div",{className:"ganttResourceContentDataContainer"});
this.contentData.appendChild(this.createPanelOwners());
_9.set(this.contentData,"height",(this.contentHeight-this.ganttChart.panelTimeHeight)+"px");
var _19=_8.create("td",{vAlign:"top"});
this.panelNames=_8.create("div",{className:"ganttResourcePanelNames"});
this.panelNames.appendChild(this.createPanelNamesOwners());
_19.appendChild(this.panelNames);
_18.appendChild(_19);
_19=_8.create("td",{vAlign:"top"});
var _1a=_8.create("div",{className:"ganttResourceDivCell"});
_1a.appendChild(this.contentData);
_19.appendChild(_1a);
_18.appendChild(_19);
_9.set(this.panelNames,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px",width:this.ganttChart.maxWidthPanelNames+"px"});
this.contentData.style.width=(this.contentWidth-this.ganttChart.maxWidthPanelNames)+"px";
this.contentData.firstChild.style.width=this.ganttChart.pixelsPerDay*(this.ganttChart.panelTime.firstChild.firstChild.rows[3].cells.length)+"px";
var _1b=this;
this.contentData.onscroll=function(){
if(_1b.panelNames){
_1b.panelNames.scrollTop=this.scrollTop;
}
};
this.contentData.scrollLeft=this.ganttChart.contentData.scrollLeft;
for(var _1c in this.resourceInfo){
this.createOwnerEntry(_1c);
}
this.postAdjustment();
},create:function(){
var _1d=_8.create("div",{innerHTML:"Resource Chart:",className:"ganttResourceHeader"},this.ganttChart.content,"after");
_9.set(_1d,"width",this.ganttChart.contentWidth+"px");
var _1e=_8.create("div",{className:"ganttResourceContent"},_1d,"after");
_9.set(_1e,{width:this.ganttChart.contentWidth+"px",height:(this.ganttChart.resourceChartHeight||(this.ganttChart.contentHeight*0.8))+"px"});
this.content=_1e||this.content;
this.reConstruct();
},postAdjustment:function(){
this.contentData.firstChild.style.height=(this.ownerItem.length*23)+"px";
this.panelNames.firstChild.style.height=(this.ownerItem.length*23)+"px";
},refreshOwnerEntry:function(_1f){
this.refreshOwnerItem(_1f);
_2.forEach(this.resourceInfo[_1f],function(_20,i){
var _21=this.ownerTaskNodeMapping[_1f].tasks[i][0];
this.refreshDetailedTaskEntry(_1f,_21,_20);
},this);
},createOwnerEntry:function(_22){
var _23=this.contentData.firstChild;
var _24=this.ownerItem[this.ownerItem.length-1];
this.ownerTaskNodeMapping[_22]={};
this.ownerTaskNodeMapping[_22][_22]=[];
var _25=(_24?parseInt(_24.style.top):(6-23))+this.ganttChart.heightTaskItem+11;
var _26=this.createOwnerItem(_22,_25);
_23.appendChild(_26);
this.ownerItem.push(_26);
this.ownerTaskNodeMapping[_22][_22].push(_26);
if(this.panelNames){
var _27=this.createOwnerNameItem(_22,_25);
this.panelNames.firstChild.appendChild(_27);
this.ownerNameItem.push(_27);
this.ownerTaskNodeMapping[_22][_22].push(_27);
}
var _28=this.ownerNameItem[this.ownerNameItem.length-1];
if(this.panelNames){
this.checkWidthTaskNameItem(_28);
var _29=this.createTreeImg(_28);
this.panelNames.firstChild.appendChild(_29);
this.ownerTaskNodeMapping[_22][_22].push(_29);
}
this.ownerTaskNodeMapping[_22]["taskCount"]=this.resourceInfo[_22].length;
this.ownerTaskNodeMapping[_22]["isOpen"]=false;
this.ownerTaskNodeMapping[_22]["tasks"]=[];
_2.forEach(this.resourceInfo[_22],function(_2a){
this.ownerTaskNodeMapping[_22]["tasks"].push(this.createDetailedTaskEntry(_22,_28,_2a));
},this);
return this;
},createOwnerNameItem:function(_2b,_2c){
var _2d=_8.create("div",{id:_2b,title:_2b,innerHTML:_2b,className:"ganttOwnerNameItem"});
_9.set(_2d,"top",_2c+"px");
return _2d;
},refreshOwnerItem:function(_2e){
var _2f=this.ownerTaskNodeMapping[_2e][_2e][0],_30=this.ownerTimeConsume[_2e].min,dur=this.ownerTimeConsume[_2e].dur,_31=this.ganttChart.getPosOnDate(_30);
_2f.style.left=_31+"px";
_2f.style.width=dur*this.ganttChart.pixelsPerWorkHour+"px";
_2.forEach(this.resourceInfo[_2e],function(_32,i){
var _33=this.ganttChart.getPosOnDate(_32.taskItem.startTime);
_9.set(_2f.childNodes[i],{left:(_33-_31)+"px",width:_32.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
},this);
},createOwnerItem:function(_34,_35){
var _36=this.ownerTimeConsume[_34].min,dur=this.ownerTimeConsume[_34].dur;
var _37=this.ganttChart.getPosOnDate(_36);
var _38=_8.create("div",{id:_34,owner:true,className:"ganttOwnerBar"});
_9.set(_38,{left:_37+"px",top:_35+"px",width:dur*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
_2.forEach(this.resourceInfo[_34],function(_39){
var _3a=_8.create("div",{id:_34,className:"ganttOwnerTaskBar"},_38);
var _3b=this.ganttChart.getPosOnDate(_39.taskItem.startTime);
_9.set(_3a,{left:(_3b-_37)+"px",width:_39.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
},this);
return _38;
},refreshDetailedTaskEntry:function(_3c,_3d,_3e){
this.refreshTaskItem(_3d,_3e);
},createDetailedTaskEntry:function(_3f,_40,_41){
var _42=[];
var _43=this.contentData.firstChild;
var _44=parseInt(_40.style.top);
var _45=this.createTaskItem(_41,_44);
_45.style.display="none";
_43.appendChild(_45);
this.ownerItem.push(_45);
_42.push(_45);
if(this.panelNames){
var _46=this.createTaskNameItem(_41.taskItem.name,_44);
this.panelNames.firstChild.appendChild(_46);
_46.style.display="none";
this.ownerNameItem.push(_46);
_42.push(_46);
}
if(this.panelNames){
this.ownerNameItem[this.ownerNameItem.length-1].style.left=_9.get(_40,"left")+15+"px";
var _47=this.createConnectingLinesPN(_40,this.ownerNameItem[this.ownerNameItem.length-1]);
_2.forEach(_47,function(_48){
_48.style.display="none";
},this);
_42.push({"v":_47[0],"h":_47[1]});
this.checkWidthTaskNameItem(this.ownerNameItem[this.ownerNameItem.length-1]);
}
return _42;
},createTaskNameItem:function(_49,_4a){
var _4b=_8.create("div",{id:_49,className:"ganttTaskNameItem",title:_49,innerHTML:_49});
_9.set(_4b,"top",_4a+"px");
return _4b;
},refreshTaskItem:function(_4c,_4d){
var _4e=this.ganttChart.getPosOnDate(_4d.taskItem.startTime);
_9.set(_4c,{left:_4e+"px",width:_4d.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px"});
},createTaskItem:function(_4f,_50){
var _51=this.ganttChart.getPosOnDate(_4f.taskItem.startTime);
var _52=_8.create("div",{id:_4f.taskItem.name,className:"ganttTaskBar"});
_9.set(_52,{left:_51+"px",top:_50+"px",width:_4f.taskItem.duration*this.ganttChart.pixelsPerWorkHour+"px",height:this.ganttChart.heightTaskItem+"px"});
return _52;
},createConnectingLinesPN:function(_53,_54){
var _55=[];
var _56=_8.create("div",{innerHTML:"&nbsp;",className:"ganttResourceLineVerticalLeft"},this.panelNames.firstChild);
_56.cNode=_54;
_56.pNode=_53;
var _57=_8.create("div",{noShade:true,color:"#000",className:"ganttResourceLineHorizontalLeft"},this.panelNames.firstChild);
_57.cNode=_54;
_57.pNode=_53;
this.panelNames.firstChild.appendChild(_57);
_55.push(_56);
_55.push(_57);
return _55;
},createTreeImg:function(_58){
var _59=_8.create("div",{id:_58.id,className:"ganttImageTreeExpand"});
_a.set(_59,"tabIndex",0);
var _5a=this.ownerTaskNodeMapping[_58.id];
_2.forEach(["click","keydown"],function(e){
this.ganttChart._events.push(on(_59,e,_3.hitch(this,function(evt){
var _5b=false,_5c,_5d;
if(e=="keydown"&&evt.keyCode!=_c.ENTER){
return;
}
if(_5a.isOpen){
_7.remove(_59,"ganttImageTreeCollapse");
_7.add(_59,"ganttImageTreeExpand");
_5a.isOpen=false;
for(_5c in this.ownerTaskNodeMapping){
_5d=this.ownerTaskNodeMapping[_5c];
if(_5b){
_2.forEach(_5d[_5c],function(_5e){
_9.set(_5e,"top",_9.get(_5e,"top")-_5a.taskCount*23+"px");
});
_2.forEach(_5d.tasks,function(_5f){
_2.forEach(_5f,function(_60){
var _61=!_60.v&&!_60.h?[_60]:[_60.v,_60.h];
_2.forEach(_61,function(t){
_9.set(t,"top",_9.get(t,"top")-_5a.taskCount*23+"px");
});
});
});
}else{
if(_5c==_58.id){
_5b=true;
_2.forEach(_5d.tasks,function(_62){
_2.forEach(_62,function(_63){
this.styleOwnerItem(_63,_5d[_5c][0],"none",0);
},this);
},this);
}
}
}
}else{
_7.remove(_59,"ganttImageTreeExpand");
_7.add(_59,"ganttImageTreeCollapse");
_5a.isOpen=true;
for(_5c in this.ownerTaskNodeMapping){
_5d=this.ownerTaskNodeMapping[_5c];
if(_5b){
_2.forEach(_5d[_5c],function(_64){
_9.set(_64,"top",_9.get(_64,"top")+_5a.taskCount*23+"px");
});
_2.forEach(_5d.tasks,function(_65){
_2.forEach(_65,function(_66){
var _67=!_66.v&&!_66.h?[_66]:[_66.v,_66.h];
_2.forEach(_67,function(t){
_9.set(t,"top",_9.get(t,"top")+_5a.taskCount*23+"px");
});
});
});
}else{
if(_5c==_58.id){
_5b=true;
_2.forEach(_5d.tasks,function(_68,i){
_2.forEach(_68,function(_69){
this.styleOwnerItem(_69,_5d[_5c][0],"inline",(i+1)*23);
},this);
},this);
}
}
}
}
})));
},this);
_7.add(_59,"ganttResourceTreeImage");
_9.set(_59,{left:(_9.get(_58,"left")-12)+"px",top:(_9.get(_58,"top")+3)+"px"});
return _59;
},styleOwnerItem:function(_6a,_6b,_6c,_6d){
if(_6a.v||_6a.h){
_9.set(_6a.v,{height:Math.max(1,(_6a.v.cNode.offsetTop-_6a.v.pNode.offsetTop))+"px",top:(_6a.v.pNode.offsetTop+5)+"px",left:(_6a.v.pNode.offsetLeft-9)+"px",display:_6c});
_9.set(_6a.h,{width:Math.max(1,(_6a.h.cNode.offsetLeft-_6a.h.pNode.offsetLeft+4))+"px",top:(_6a.h.cNode.offsetTop+5)+"px",left:(_6a.h.pNode.offsetLeft-9)+"px",display:_6c});
}else{
_9.set(_6a,{display:_6c,top:parseInt(_6b.style.top)+_6d+"px"});
}
},checkWidthTaskNameItem:function(_6e){
if(_6e&&_6e.offsetWidth+_6e.offsetLeft>this.ganttChart.maxWidthPanelNames){
var _6f=_6e.offsetWidth+_6e.offsetLeft-this.ganttChart.maxWidthPanelNames,_70=Math.round(_6f/(_6e.offsetWidth/_6e.firstChild.length)),_71=_6e.id.substring(0,_6e.firstChild.length-_70-3);
_6e.innerHTML=_71+"...";
}
},createPanelOwners:function(){
var _72=_8.create("div",{className:"ganttOwnerPanel"});
_9.set(_72,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px"});
return _72;
},createPanelNamesOwners:function(){
var _73=_8.create("div",{innerHTML:"&nbsp;",className:"ganttResourcePanelNamesOwners"});
_9.set(_73,{height:(this.contentHeight-this.ganttChart.panelTimeHeight-this.ganttChart.scrollBarWidth)+"px",width:this.ganttChart.maxWidthPanelNames+"px"});
return _73;
}});
});
