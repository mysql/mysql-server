//>>built
// wrapped by build app
define("dojox/gantt/GanttProjectItem", ["dijit","dojo","dojox","dojo/require!dojox/gantt/GanttTaskItem,dojo/date/locale,dijit/focus"], function(dijit,dojo,dojox){
dojo.provide("dojox.gantt.GanttProjectItem");

dojo.require("dojox.gantt.GanttTaskItem");
dojo.require("dojo.date.locale");
dojo.require("dijit.focus");		// dijit.focus()

dojo.declare("dojox.gantt.GanttProjectControl", null, {
	constructor: function(ganttChart, projectItem){
		this.project = projectItem;
		this.ganttChart = ganttChart;
		this.descrProject = null;
		this.projectItem = null;
		this.projectNameItem = null;
		this.posY = 0;
		this.posX = 0;
		this.nextProject = null;
		this.previousProject = null;
		this.arrTasks = [];
		this.percentage = 0;
		this.duration = 0;
	},
	checkWidthProjectNameItem: function(){
		if(this.projectNameItem.offsetWidth + this.projectNameItem.offsetLeft > this.ganttChart.maxWidthTaskNames){
			var width = this.projectNameItem.offsetWidth + this.projectNameItem.offsetLeft - this.ganttChart.maxWidthTaskNames;
			var countChar = Math.round(width / (this.projectNameItem.offsetWidth / this.projectNameItem.firstChild.length));
			var pName = this.project.name.substring(0, this.projectNameItem.firstChild.length - countChar - 3);
			pName += "...";
			this.projectNameItem.innerHTML = pName;
		}
	},
	refreshProjectItem: function(projectItem){
		this.percentage = this.getPercentCompleted();
		dojo.style(projectItem, {
			"left": this.posX + "px",
			"width": this.duration * this.ganttChart.pixelsPerWorkHour + "px"
		});
		var tblProjectItem = projectItem.firstChild;
		var width = this.duration * this.ganttChart.pixelsPerWorkHour;
		tblProjectItem.width = ((width == 0) ? 1 : width) + "px";
		tblProjectItem.style.width = ((width == 0) ? 1 : width) + "px";
		var rowprojectItem = tblProjectItem.rows[0];
		if(this.percentage != -1){
			if(this.percentage != 0){
				var cellprojectItem = rowprojectItem.firstChild;
				cellprojectItem.width = this.percentage + "%";
				var imageProgress = cellprojectItem.firstChild;
				dojo.style(imageProgress, {
					width: (!this.duration ? 1 : (this.percentage * this.duration * this.ganttChart.pixelsPerWorkHour / 100)) + "px",
					height: this.ganttChart.heightTaskItem + "px"
				})
			}
			if(this.percentage != 100){
				var cellprojectItem = rowprojectItem.lastChild;
				cellprojectItem.width = (100 - this.percentage) + "%";
				var imageProgress = cellprojectItem.firstChild;
				dojo.style(imageProgress, {
					width: (!this.duration ? 1 : ((100 - this.percentage) * this.duration * this.ganttChart.pixelsPerWorkHour / 100)) + "px",
					height: this.ganttChart.heightTaskItem + "px"
				})
			}
		}else{
			var cellprojectItem = rowprojectItem.firstChild;
			cellprojectItem.width = "1px";
			var imageProgress = cellprojectItem.firstChild;
			dojo.style(imageProgress, {
				width: "1px",
				height: this.ganttChart.heightTaskItem + "px"
			})
		}
		var divTaskInfo = projectItem.lastChild;
		var tblTaskInfo = divTaskInfo.firstChild;
		dojo.style(tblTaskInfo, {
			height: this.ganttChart.heightTaskItem + "px",
			width: (!this.duration ? 1 : (this.duration * this.ganttChart.pixelsPerWorkHour)) + "px"
		});
		var rowTaskInfo = tblTaskInfo.rows[0];
		var cellTaskInfo = rowTaskInfo.firstChild;
		cellTaskInfo.height = this.ganttChart.heightTaskItem + "px";
		if(this.project.parentTasks.length == 0){
			projectItem.style.display = "none";
		}
		return projectItem;
	},
	refreshDescrProject: function(divDesc){
		var posX = (this.posX + this.duration * this.ganttChart.pixelsPerWorkHour + 10);
		dojo.style(divDesc, {
			"left": posX + "px"
		});
		if(this.project.parentTasks.length == 0){
			this.descrProject.style.visibility = 'hidden';
		}
		return divDesc;
	},
	postLoadData: function(){
		//TODO e.g. project relative info...
	},
	refresh: function(){
		var containerTasks = this.ganttChart.contentData.firstChild;
		this.posX = (this.project.startDate - this.ganttChart.startDate) / (60 * 60 * 1000) * this.ganttChart.pixelsPerHour;
		this.refreshProjectItem(this.projectItem[0]);
		this.refreshDescrProject(this.projectItem[0].nextSibling);
		return this;
	},
	create: function(){
		var containerTasks = this.ganttChart.contentData.firstChild;
		this.posX = (this.project.startDate - this.ganttChart.startDate) / (60 * 60 * 1000) * this.ganttChart.pixelsPerHour;
		if(this.previousProject){
			if(this.previousProject.arrTasks.length > 0){
				var lastChildTask = this.ganttChart.getLastChildTask(this.previousProject.arrTasks[this.previousProject.arrTasks.length - 1]);
				this.posY = parseInt(lastChildTask.cTaskItem[0].style.top) + this.ganttChart.heightTaskItem + this.ganttChart.heightTaskItemExtra;
			}else{
				this.posY = parseInt(this.previousProject.projectItem[0].style.top) + this.ganttChart.heightTaskItem + this.ganttChart.heightTaskItemExtra;
			}
		}else{
			this.posY = 6;
		}
		var containerNames = this.ganttChart.panelNames.firstChild;
		this.projectNameItem = this.createProjectNameItem();
		containerNames.appendChild(this.projectNameItem);
		this.checkWidthProjectNameItem();
		this.projectItem = [this.createProjectItem(), []];
		containerTasks.appendChild(this.projectItem[0]);
		containerTasks.appendChild(this.createDescrProject());
		this.adjustPanelTime();
	},
	getTaskById: function(id){
		for(var i = 0; i < this.arrTasks.length; i++){
			var aTask = this.arrTasks[i];
			var task = this.searchTaskInTree(aTask, id);
			if(task){
				return task;
			}
		}
		return null;
	},
	searchTaskInTree: function(task, id){
		if(task.taskItem.id == id){
			return task;
		}else{
			for(var i = 0; i < task.childTask.length; i++){
				var cTask = task.childTask[i];
				if(cTask.taskItem.id == id){
					return cTask;
				}else{
					if(cTask.childTask.length > 0){
						var cTask = this.searchTaskInTree(cTask, id);
						if(cTask){
							return cTask;
						}
					}
				}
			}
		}
		return null;
	},
	shiftProjectItem: function(){
		var posItemL = null;
		var posItemR = null;
		var posProjectItemL = parseInt(this.projectItem[0].style.left);
		var posProjectItemR = parseInt(this.projectItem[0].firstChild.style.width) + parseInt(this.projectItem[0].style.left);
		var widthProjectItem = parseInt(this.projectItem[0].firstChild.style.width);
		for(var i = 0; i < this.arrTasks.length; i++){
			var aTask = this.arrTasks[i];
			var tmpPosItemL = parseInt(aTask.cTaskItem[0].style.left);
			var tmpPosItemR = parseInt(aTask.cTaskItem[0].style.left) + parseInt(aTask.cTaskItem[0].firstChild.firstChild.width);
			if(!posItemL){
				posItemL = tmpPosItemL;
			}
			if(!posItemR){
				posItemR = tmpPosItemR;
			}
			if(posItemL > tmpPosItemL){
				posItemL = tmpPosItemL;
			}
			if(posItemR < tmpPosItemR){
				posItemR = tmpPosItemR;
			}
		}
		if(posItemL != posProjectItemL){
			this.project.startDate = new Date(this.ganttChart.startDate);
			this.project.startDate.setHours(this.project.startDate.getHours() + (posItemL / this.ganttChart.pixelsPerHour));
		}
		this.projectItem[0].style.left = posItemL + "px";
		this.resizeProjectItem(posItemR - posItemL);
		this.duration = Math.round(parseInt(this.projectItem[0].firstChild.width) / (this.ganttChart.pixelsPerWorkHour));
		this.shiftDescrProject();
		this.adjustPanelTime();
	},
	adjustPanelTime: function(){
		var projectItem = this.projectItem[0];
		var width = parseInt(projectItem.style.left) + parseInt(projectItem.firstChild.style.width) + this.ganttChart.panelTimeExpandDelta;
		width += this.descrProject.offsetWidth;
		this.ganttChart.adjustPanelTime(width);
	},
	resizeProjectItem: function(width){
		var percentage = this.percentage,
			pItem = this.projectItem[0];
		if(percentage > 0 && percentage < 100){
			pItem.firstChild.style.width = width + "px";
			pItem.firstChild.width = width + "px";
			pItem.style.width = width + "px";
			var firstRow = pItem.firstChild.rows[0];
			firstRow.cells[0].firstChild.style.width = Math.round(width * percentage / 100) + "px";
			firstRow.cells[0].firstChild.style.height = this.ganttChart.heightTaskItem + "px";
			firstRow.cells[1].firstChild.style.width = Math.round(width * (100 - percentage) / 100) + "px";
			firstRow.cells[1].firstChild.style.height = this.ganttChart.heightTaskItem + "px";
			pItem.lastChild.firstChild.width = width + "px";
		}else if(percentage == 0 || percentage == 100){
			pItem.firstChild.style.width = width + "px";
			pItem.firstChild.width = width + "px";
			pItem.style.width = width + "px";
			var firstRow = pItem.firstChild.rows[0];
			firstRow.cells[0].firstChild.style.width = width + "px";
			firstRow.cells[0].firstChild.style.height = this.ganttChart.heightTaskItem + "px";
			pItem.lastChild.firstChild.width = width + "px";
		}
	},
	shiftDescrProject: function(){
		var posX = (parseInt(this.projectItem[0].style.left) + this.duration * this.ganttChart.pixelsPerWorkHour + 10);
		this.descrProject.style.left = posX + "px";
		this.descrProject.innerHTML = this.getDescStr();
	},
	showDescrProject: function(){
		var posX = (parseInt(this.projectItem[0].style.left) + this.duration * this.ganttChart.pixelsPerWorkHour + 10);
		this.descrProject.style.left = posX + "px";
		this.descrProject.style.visibility = 'visible';
		this.descrProject.innerHTML = this.getDescStr();
	},
	hideDescrProject: function(){
		this.descrProject.style.visibility = 'hidden';
	},
	getDescStr: function(){
		return this.duration/this.ganttChart.hsPerDay + " days,  " + this.duration + " hours";
	},
	createDescrProject: function(){
		var posX = (this.posX + this.duration * this.ganttChart.pixelsPerWorkHour + 10);
		var divDesc = dojo.create("div", {
			innerHTML: this.getDescStr(),
			className: "ganttDescProject"
		});
		dojo.style(divDesc, {
			left: posX + "px",
			top: this.posY + "px"
		});
		this.descrProject = divDesc;
		if(this.project.parentTasks.length == 0){
			this.descrProject.style.visibility = 'hidden';
		}
		return divDesc;
	},
	createProjectItem: function(){
		this.percentage = this.getPercentCompleted();
		this.duration = this.getDuration();
		var projectItem = dojo.create("div", {
			id: this.project.id,
			className: "ganttProjectItem"
		});
		dojo.style(projectItem, {
			left: this.posX + "px",
			top: this.posY + "px",
			width: this.duration * this.ganttChart.pixelsPerWorkHour + "px"
		});
		var tblProjectItem = dojo.create("table", {
			cellPadding: "0",
			cellSpacing: "0",
			className: "ganttTblProjectItem"
		}, projectItem);
		var width = this.duration * this.ganttChart.pixelsPerWorkHour;
		tblProjectItem.width = ((width == 0) ? 1 : width) + "px";
		tblProjectItem.style.width = ((width == 0) ? 1 : width) + "px";
		
		var rowprojectItem = tblProjectItem.insertRow(tblProjectItem.rows.length);
		if(this.percentage != -1){
			if(this.percentage != 0){
				var cellprojectItem = dojo.create("td", {
					width: this.percentage + "%"
				}, rowprojectItem);
				cellprojectItem.style.lineHeight = "1px";
				var imageProgress = dojo.create("div", {
					className: "ganttImageProgressFilled"
				}, cellprojectItem);
				dojo.style(imageProgress, {
					width: (this.percentage * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
					height: this.ganttChart.heightTaskItem + "px"
				});
			}
			if(this.percentage != 100){
				var cellprojectItem = dojo.create("td", {
					width: (100 - this.percentage) + "%"
				}, rowprojectItem);
				cellprojectItem.style.lineHeight = "1px";
				var imageProgress = dojo.create("div", {
					className: "ganttImageProgressBg"
				}, cellprojectItem);
				dojo.style(imageProgress, {
					width: ((100 - this.percentage) * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
					height: this.ganttChart.heightTaskItem + "px"
				});
			}
		}else{
			var cellprojectItem = dojo.create("td", {
				width: "1px"
			}, rowprojectItem);
			cellprojectItem.style.lineHeight = "1px";
			var imageProgress = dojo.create("div", {
				className: "ganttImageProgressBg"
			}, cellprojectItem);
			dojo.style(imageProgress, {
				width: "1px",
				height: this.ganttChart.heightTaskItem + "px"
			});
		}
		var divTaskInfo = dojo.create("div", {className: "ganttDivTaskInfo"});
		var tblTaskInfo = dojo.create("table", {
			cellPadding: "0",
			cellSpacing: "0",
			height: this.ganttChart.heightTaskItem + "px",
			width: ((this.duration * this.ganttChart.pixelsPerWorkHour == 0) ? 1 : this.duration * this.ganttChart.pixelsPerWorkHour) + "px"
		}, divTaskInfo);
		var rowTaskInfo = tblTaskInfo.insertRow(0);
		var cellTaskInfo = dojo.create("td", {
			align: "center",
			vAlign: "top",
			height: this.ganttChart.heightTaskItem + "px",
			className: "ganttMoveInfo"
		}, rowTaskInfo);
		projectItem.appendChild(divTaskInfo);
		if(this.project.parentTasks.length == 0){
			projectItem.style.display = "none";
		}
		return projectItem;
	},
	createProjectNameItem: function(){
		var divName = dojo.create("div", {
			className: "ganttProjectNameItem",
			innerHTML: this.project.name,
			title: this.project.name
		});
		dojo.style(divName, {
			left: "5px",
			top: this.posY + "px"
		});
		dojo.attr(divName, "tabIndex", 0);
		if(this.ganttChart.isShowConMenu){
			this.ganttChart._events.push(
				dojo.connect(divName, "onmouseover", this, function(event){
					dojo.addClass(divName, "ganttProjectNameItemHover");
					clearTimeout(this.ganttChart.menuTimer);
					this.ganttChart.tabMenu.clear();
					this.ganttChart.tabMenu.show(event.target, this);
				})
			);
			this.ganttChart._events.push(
				dojo.connect(divName, "onkeydown", this, function(event){
					if(event.keyCode == dojo.keys.ENTER){
						this.ganttChart.tabMenu.clear();
						this.ganttChart.tabMenu.show(event.target, this);
					}
					if(this.ganttChart.tabMenu.isShow && (event.keyCode == dojo.keys.LEFT_ARROW || event.keyCode == dojo.keys.RIGHT_ARROW)){
						dijit.focus(this.ganttChart.tabMenu.menuPanel.firstChild.rows[0].cells[0]);
					}
					if(this.ganttChart.tabMenu.isShow && event.keyCode == dojo.keys.ESCAPE){
						this.ganttChart.tabMenu.hide();
					}
				})
			);
			this.ganttChart._events.push(
				dojo.connect(divName, "onmouseout", this, function(){
					dojo.removeClass(divName, "ganttProjectNameItemHover");
					clearTimeout(this.ganttChart.menuTimer);
					this.ganttChart.menuTimer = setTimeout(dojo.hitch(this, function(){
						this.ganttChart.tabMenu.hide();
					}), 200);
				})
			);
			this.ganttChart._events.push(
				dojo.connect(this.ganttChart.tabMenu.menuPanel, "onmouseover", this, function(){
					clearTimeout(this.ganttChart.menuTimer);
				})
			);
			this.ganttChart._events.push(
				dojo.connect(this.ganttChart.tabMenu.menuPanel, "onkeydown", this, function(event){
					if(this.ganttChart.tabMenu.isShow && event.keyCode == dojo.keys.ESCAPE){
						this.ganttChart.tabMenu.hide();
					}
				})
			);
			this.ganttChart._events.push(
				dojo.connect(this.ganttChart.tabMenu.menuPanel, "onmouseout", this, function(){
					clearTimeout(this.ganttChart.menuTimer);
					this.ganttChart.menuTimer = setTimeout(dojo.hitch(this, function(){
						this.ganttChart.tabMenu.hide();
					}), 200);
				})
			);
		}
		return divName;
	},
	getPercentCompleted: function(){
		var sum = 0, percentage = 0;
		dojo.forEach(this.project.parentTasks, function(ppTask){
			sum += parseInt(ppTask.percentage);
		}, this);
		if(this.project.parentTasks.length != 0){
			return percentage = Math.round(sum / this.project.parentTasks.length);
		}else{
			return percentage = -1;
		}
	},
	getDuration: function(){
		var duration = 0, tmpDuration = 0;
		if(this.project.parentTasks.length > 0){
			dojo.forEach(this.project.parentTasks, function(ppTask){
				tmpDuration = ppTask.duration * 24 / this.ganttChart.hsPerDay + (ppTask.startTime - this.ganttChart.startDate) / (60 * 60 * 1000);
				if(tmpDuration > duration){
					duration = tmpDuration;
				}
			}, this);
			return ((duration - this.posX) / 24) * this.ganttChart.hsPerDay;
		}else{
			return 0;
		}
	},
	deleteTask: function(id){
		var task = this.getTaskById(id);
		if(task){
			this.deleteChildTask(task);
			this.ganttChart.checkPosition();
		}
	},
	setName: function(name){
		if(name){
			this.project.name = name;
			this.projectNameItem.innerHTML = name;
			this.projectNameItem.title = name;
			this.checkWidthProjectNameItem();
			
			this.descrProject.innerHTML = this.getDescStr();
			this.adjustPanelTime();
		}
	},
	setPercentCompleted: function(percentage){
		percentage = parseInt(percentage);
		if(isNaN(percentage) || percentage > 100 || percentage < 0){
			return false;
		}
		var prow = this.projectItem[0].firstChild.rows[0],
			rc0 = prow.cells[0], rc1 = prow.cells[1];
		if((percentage > 0) && (percentage < 100) && (this.percentage > 0) && (this.percentage < 100)){
			rc0.width = parseInt(percentage) + "%";
			rc0.firstChild.style.width = (percentage * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px";
			rc1.width = (100 - parseInt(percentage)) + "%";
			rc1.firstChild.style.width = ((100 - percentage) * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px";
		}else if(((percentage == 0) || (percentage == 100)) && (this.percentage > 0) && (this.percentage < 100)){
			if(percentage == 0){
				rc0.parentNode.removeChild(rc0);
				rc1.width = 100 + "%";
				rc1.firstChild.style.width = this.duration * this.ganttChart.pixelsPerWorkHour + "px";
			}else if(percentage == 100){
				rc1.parentNode.removeChild(rc1);
				rc0.width = 100 + "%";
				rc0.firstChild.style.width = this.duration * this.ganttChart.pixelsPerWorkHour + "px";
			}
		}else if(((percentage == 0) || (percentage == 100)) && ((this.percentage == 0) || (this.percentage == 100))){
			if((percentage == 0) && (this.percentage == 100)){
				dojo.removeClass(rc0.firstChild, "ganttImageProgressFilled");
				dojo.addClass(rc0.firstChild, "ganttImageProgressBg");
			}else if((percentage == 100) && (this.percentage == 0)){
				dojo.removeClass(rc0.firstChild, "ganttImageProgressBg");
				dojo.addClass(rc0.firstChild, "ganttImageProgressFilled");
			}
		}else if(((percentage > 0) || (percentage < 100)) && ((this.percentage == 0) || (this.percentage == 100))){
			rc0.parentNode.removeChild(rc0);
			var cellprojectItem = dojo.create("td", {
				width: percentage + "%"
			}, prow);
			cellprojectItem.style.lineHeight = "1px";
			var imageProgress = dojo.create("div", {
				className: "ganttImageProgressFilled"
			}, cellprojectItem);
			dojo.style(imageProgress, {
				width: (percentage * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
				height: this.ganttChart.heightTaskItem + "px"
			});
			cellprojectItem = dojo.create("td", {
				width: (100 - percentage) + "%"
			}, prow);
			cellprojectItem.style.lineHeight = "1px";
			imageProgress = dojo.create("div", {
				className: "ganttImageProgressBg"
			}, cellprojectItem);
			dojo.style(imageProgress, {
				width: ((100 - percentage) * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
				height: this.ganttChart.heightTaskItem + "px"
			});
		}else if(this.percentage == -1){
			if(percentage == 100){
				dojo.removeClass(rc0.firstChild, "ganttImageProgressBg");
				dojo.addClass(rc0.firstChild, "ganttImageProgressFilled");
			}else if(percentage < 100 && percentage > 0){
				rc0.parentNode.removeChild(rc0);
				var cellprojectItem = dojo.create("td", {
					width: percentage + "%"
				}, prow);
				cellprojectItem.style.lineHeight = "1px";
				imageProgress = dojo.create("div", {
					className: "ganttImageProgressFilled"
				}, cellprojectItem);
				dojo.style(imageProgress, {
					width: (percentage * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
					height: this.ganttChart.heightTaskItem + "px"
				});
				cellprojectItem = dojo.create("td", {
					width: (100 - percentage) + "%"
				}, prow);
				cellprojectItem.style.lineHeight = "1px";
				imageProgress = dojo.create("div", {
					className: "ganttImageProgressBg"
				}, cellprojectItem);
				dojo.style(imageProgress, {
					width: ((100 - percentage) * this.duration * this.ganttChart.pixelsPerWorkHour) / 100 + "px",
					height: this.ganttChart.heightTaskItem + "px"
				});
			}
		}
		this.percentage = percentage;
		this.descrProject.innerHTML = this.getDescStr();
		return true;
	},
	deleteChildTask: function(task){
		if(task){
			var tItem0 = task.cTaskItem[0], tNameItem0 = task.cTaskNameItem[0],
				tItem1 = task.cTaskItem[1], tNameItem1 = task.cTaskNameItem[1],
				tItem2 = task.cTaskItem[2], tNameItem2 = task.cTaskNameItem[2];
			if(tItem0.style.display == "none"){
				this.ganttChart.openTree(task.parentTask);
			}
			//delete of connecting lines
			if(task.childPredTask.length > 0){
				for(var i = 0; i < task.childPredTask.length; i++){
					var cpTask = task.childPredTask[i];
					for(var t = 0; t < cpTask.cTaskItem[1].length; t++){
						cpTask.cTaskItem[1][t].parentNode.removeChild(cpTask.cTaskItem[1][t]);
					}
					cpTask.cTaskItem[1] = [];
					cpTask.predTask = null;
				}
			}
			//delete child task
			if(task.childTask.length > 0){
				while(task.childTask.length > 0){
					this.deleteChildTask(task.childTask[0]);
				}
			}
			//shift tasks
			var rowHeight = this.ganttChart.heightTaskItem + this.ganttChart.heightTaskItemExtra;
			if(tItem0.style.display != "none"){
				task.shiftCurrentTasks(task, -rowHeight);
			}
			//delete object task
			this.project.deleteTask(task.taskItem.id);
			//delete div and connecting lines from contentData
			if(tItem0){
				tItem0.parentNode.removeChild(tItem0);
			}
			task.descrTask.parentNode.removeChild(task.descrTask);
			if(tItem1.length > 0){
				for(var j = 0; j < tItem1.length; j++){
					tItem1[j].parentNode.removeChild(tItem1[j]);
				}
			}
			//delete div and connecting lines from panelName
			if(tNameItem0){
				tNameItem0.parentNode.removeChild(tNameItem0);
			}
			if(task.cTaskNameItem[1]){
				for(var j = 0; j < tNameItem1.length; j++){
					tNameItem1[j].parentNode.removeChild(tNameItem1[j]);
				}
			}
			if(tNameItem2 && tNameItem2.parentNode){
				tNameItem2.parentNode.removeChild(tNameItem2);
			}
			if(task.taskIdentifier){
				task.taskIdentifier.parentNode.removeChild(task.taskIdentifier);
				task.taskIdentifier = null;
			}
			//delete object task
			if(task.parentTask){
				if(task.previousChildTask){
					if(task.nextChildTask){
						task.previousChildTask.nextChildTask = task.nextChildTask;
					}else{
						task.previousChildTask.nextChildTask = null;
					}
				}
				var parentTask = task.parentTask;
				for(var i = 0; i < parentTask.childTask.length; i++){
					if(parentTask.childTask[i].taskItem.id == task.taskItem.id){
						parentTask.childTask[i] = null;
						parentTask.childTask.splice(i, 1);
						break;
					}
				}
				if(parentTask.childTask.length == 0){
					if(parentTask.cTaskNameItem[2]){
						parentTask.cTaskNameItem[2].parentNode.removeChild(parentTask.cTaskNameItem[2]);
						parentTask.cTaskNameItem[2] = null;
					}
				}
			}else{
				if(task.previousParentTask){
					if(task.nextParentTask){
						task.previousParentTask.nextParentTask = task.nextParentTask;
					}else{
						task.previousParentTask.nextParentTask = null;
					}
				}
				var project = task.project;
				for(var i = 0; i < project.arrTasks.length; i++){
					if(project.arrTasks[i].taskItem.id == task.taskItem.id){
						project.arrTasks.splice(i, 1);
					}
				}
			}
			if(task.predTask){
				var predTask = task.predTask;
				for(var i = 0; i < predTask.childPredTask.length; i++){
					if(predTask.childPredTask[i].taskItem.id == task.taskItem.id){
						predTask.childPredTask[i] = null;
						predTask.childPredTask.splice(i, 1);
					}
				}
			}
			if(task.project.arrTasks.length != 0){
				task.project.shiftProjectItem();
			}else{
				task.project.projectItem[0].style.display = "none";
				this.hideDescrProject();
			}
			this.ganttChart.contentDataHeight -= this.ganttChart.heightTaskItemExtra + this.ganttChart.heightTaskItem;
		}
	},
	
	insertTask: function(id, name, startTime, duration, percentage, previousTaskId, taskOwner, parentTaskId){
		var task = null;
		var _task = null;
		if(this.project.getTaskById(id)){
			return false;
		}
		if((!duration) || (duration < this.ganttChart.minWorkLength)){
			duration = this.ganttChart.minWorkLength;
		}
		if((!name) || (name == "")){
			name = id;
		}
		if((!percentage) || (percentage == "")){
			percentage = 0;
			
		}else{
			percentage = parseInt(percentage);
			if(percentage < 0 || percentage > 100){
				return false;
			}
		}
		var sortRequired = false;
		if((parentTaskId) && (parentTaskId != "")){
			var parentTask = this.project.getTaskById(parentTaskId);
			if(!parentTask){
				return false;
			}
			startTime = startTime || parentTask.startTime;
			if(startTime < parentTask.startTime){
				return false;
			}
			task = new dojox.gantt.GanttTaskItem({
				id: id,
				name: name,
				startTime: startTime,
				duration: duration,
				percentage: percentage,
				previousTaskId: previousTaskId,
				taskOwner: taskOwner
			});
			if(!this.ganttChart.checkPosParentTask(parentTask, task)){
				return false;
			}
			task.parentTask = parentTask;
			var _parentTask = this.getTaskById(parentTask.id);
			var isHide = false;
			if(_parentTask.cTaskItem[0].style.display == "none"){
				isHide = true;
			}else if(_parentTask.cTaskNameItem[2]){
				if(!_parentTask.isExpanded){
					isHide = true;
				}
			}
			if(isHide){
				if(_parentTask.childTask.length == 0){
					this.ganttChart.openTree(_parentTask.parentTask);
				}else{
					this.ganttChart.openTree(_parentTask);
				}
			}
			if(previousTaskId != ""){
				var predTask = this.project.getTaskById(previousTaskId);
				if(!predTask){
					return false;
				}
				if(predTask.parentTask){
					if(predTask.parentTask.id != task.parentTask.id){
						return false;
					}
				}else{
					return false;
				}
				if(!this.ganttChart.checkPosPreviousTask(predTask, task)){
					this.ganttChart.correctPosPreviousTask(predTask, task);
				}
				task.previousTask = predTask;
			}
			var isAdd = false;
			if(sortRequired) for(var i = 0; i < parentTask.cldTasks.length; i++){
				if(task.startTime < parentTask.cldTasks[i].startTime){
					parentTask.cldTasks.splice(i, 0, task);
					if(i > 0){
						parentTask.cldTasks[i - 1].nextChildTask = parentTask.cldTasks[i];
						parentTask.cldTasks[i].previousChildTask = parentTask.cldTasks[i - 1];
					}
					if(parentTask.cldTasks[i + 1]){
						parentTask.cldTasks[i + 1].previousChildTask = parentTask.cldTasks[i];
						parentTask.cldTasks[i].nextChildTask = parentTask.cldTasks[i + 1];
					}
					isAdd = true;
					break;
				}
			}
			if(!isAdd){
				if(parentTask.cldTasks.length > 0){
					parentTask.cldTasks[parentTask.cldTasks.length - 1].nextChildTask = task;
					task.previousChildTask = parentTask.cldTasks[parentTask.cldTasks.length - 1];
				}
				parentTask.cldTasks.push(task);
			}
			if(parentTask.cldTasks.length == 1){
				var treeImg = _parentTask.createTreeImg();
				_parentTask.cTaskNameItem[2] = treeImg;
			}
			_task = new dojox.gantt.GanttTaskControl(task, this, this.ganttChart);
			_task.create();
			if(task.nextChildTask) _task.nextChildTask = _task.project.getTaskById(task.nextChildTask.id);
			_task.adjustPanelTime();
			var rowHeight = this.ganttChart.heightTaskItem + this.ganttChart.heightTaskItemExtra;
			_task.shiftCurrentTasks(_task, rowHeight);//23
		}else{
			startTime = startTime || this.project.startDate;
			task = new dojox.gantt.GanttTaskItem({
				id: id,
				name: name,
				startTime: startTime,
				duration: duration,
				percentage: percentage,
				previousTaskId: previousTaskId,
				taskOwner: taskOwner
			});
			if(task.startTime <= this.ganttChart.startDate){
				return false;
			}
			if(previousTaskId != ""){
				var predTask = this.project.getTaskById(previousTaskId);
				if(!predTask){
					return false;
				}
				if(!this.ganttChart.checkPosPreviousTask(predTask, task)){
					this.ganttChart.correctPosPreviousTask(predTask, task);
				}
				if(predTask.parentTask){
					return false;
				}
				task.previousTask = predTask;
			}
			var isAdd = false;
			if(sortRequired){
				for(var i = 0; i < this.project.parentTasks.length; i++){
					var ppTask = this.project.parentTasks[i];
					if(startTime < ppTask.startTime){
						this.project.parentTasks.splice(i, 0, task);
						if(i > 0){
							this.project.parentTasks[i - 1].nextParentTask = task;
							task.previousParentTask = this.project.parentTasks[i - 1];
						}
						if(this.project.parentTasks[i + 1]){
							this.project.parentTasks[i + 1].previousParentTask = task;
							task.nextParentTask = this.project.parentTasks[i + 1];
						}
						isAdd = true;
						break;
					}
				}
			}
			if(!isAdd){
				if(this.project.parentTasks.length > 0){
					this.project.parentTasks[this.project.parentTasks.length - 1].nextParentTask = task;
					task.previousParentTask = this.project.parentTasks[this.project.parentTasks.length - 1];
				}
				this.project.parentTasks.push(task);
			}
			_task = new dojox.gantt.GanttTaskControl(task, this, this.ganttChart);
			_task.create();
			if(task.nextParentTask) _task.nextParentTask = _task.project.getTaskById(task.nextParentTask.id);
			_task.adjustPanelTime();
			this.arrTasks.push(_task);
			var rowHeight = this.ganttChart.heightTaskItem + this.ganttChart.heightTaskItemExtra;
			_task.shiftCurrentTasks(_task, rowHeight);
			this.projectItem[0].style.display = "inline";
			this.setPercentCompleted(this.getPercentCompleted());
			this.shiftProjectItem();
			this.showDescrProject();
		}
		this.ganttChart.checkHeighPanelTasks();
		this.ganttChart.checkPosition();
		return _task;
	},
	shiftNextProject: function(project, height){
		if(project.nextProject){
			project.nextProject.shiftProject(height);
			this.shiftNextProject(project.nextProject, height);
		}
	},
	shiftProject: function(height){
		this.posY = this.posY + height;
		this.projectItem[0].style.top = parseInt(this.projectItem[0].style.top) + height + "px";
		this.descrProject.style.top = parseInt(this.descrProject.style.top) + height + "px";
		this.projectNameItem.style.top = parseInt(this.projectNameItem.style.top) + height + "px";
		if(this.arrTasks.length > 0){
			this.shiftNextParentTask(this.arrTasks[0], height);
		}
	},
	shiftTask: function(task, height){
		task.posY = task.posY + height;
		var tNameItem0 = task.cTaskNameItem[0], tNameItem1 = task.cTaskNameItem[1], tNameItem2 = task.cTaskNameItem[2],
			tItem0 = task.cTaskItem[0], tItem1 = task.cTaskItem[1], tItem2 = task.cTaskItem[2];
		tNameItem0.style.top = parseInt(tNameItem0.style.top) + height + "px";
		if(tNameItem2){
			tNameItem2.style.top = parseInt(tNameItem2.style.top) + height + "px";
		}
		if(task.parentTask){
			tNameItem1[0].style.top = parseInt(tNameItem1[0].style.top) + height + "px";
			tNameItem1[1].style.top = parseInt(tNameItem1[1].style.top) + height + "px";
		}
		task.cTaskItem[0].style.top = parseInt(task.cTaskItem[0].style.top) + height + "px";
		task.descrTask.style.top = parseInt(task.descrTask.style.top) + height + "px";
		if(tItem1[0]){
			tItem1[0].style.top = parseInt(tItem1[0].style.top) + height + "px";
			tItem1[1].style.top = parseInt(tItem1[1].style.top) + height + "px";
			tItem1[2].style.top = parseInt(tItem1[2].style.top) + height + "px";
		}
	},
	shiftNextParentTask: function(task, height){
		this.shiftTask(task, height);
		this.shiftChildTasks(task, height);
		if(task.nextParentTask){
			this.shiftNextParentTask(task.nextParentTask, height);
		}
	},
	shiftChildTasks: function(task, height){
		dojo.forEach(task.childTask, function(cTask){
			this.shiftTask(cTask, height);
			if(cTask.childTask.length > 0){
				this.shiftChildTasks(cTask, height);
			}
		}, this);
	}
});


dojo.declare("dojox.gantt.GanttProjectItem", null, {
	constructor: function(configuration){
		//id is required
		this.id = configuration.id;
		this.name = configuration.name || this.id;
		this.startDate = configuration.startDate || new Date();
		this.parentTasks = [];
	},
	getTaskById: function(id){
		for(var i = 0; i < this.parentTasks.length; i++){
			var pTask = this.parentTasks[i];
			var task = this.getTaskByIdInTree(pTask, id);
			if(task){
				return task;
			}
		}
		return null;
	},
	getTaskByIdInTree: function(parentTask, id){
		if(parentTask.id == id){
			return parentTask;
		}else{
			for(var i = 0; i < parentTask.cldTasks.length; i++){
				var pcTask = parentTask.cldTasks[i];
				if(pcTask.id == id){
					return pcTask;
				}
				if(pcTask.cldTasks.length > 0){
					if(pcTask.cldTasks.length > 0){
						var cTask = this.getTaskByIdInTree(pcTask, id);
						if(cTask){
							return cTask;
						}
					}
				}
			}
		}
		return null;
	},
	addTask: function(task){
		this.parentTasks.push(task);
		task.setProject(this);
	},
	deleteTask: function(id){
		var task = this.getTaskById(id);
		if(!task){return;}
		if(!task.parentTask){
			for(var i = 0; i < this.parentTasks.length; i++){
				var pTask = this.parentTasks[i];
				if(pTask.id == id){
					if(pTask.nextParentTask){
						if(pTask.previousParentTask){
							pTask.previousParentTask.nextParentTask = pTask.nextParentTask;
							pTask.nextParentTask.previousParentTask = pTask.previousParentTask;
						}else{
							pTask.nextParentTask.previousParentTask = null;
						}
					}else{
						if(pTask.previousParentTask){
							pTask.previousParentTask.nextParentTask = null;
						}
					}
					pTask = null;
					this.parentTasks.splice(i, 1);
					break;
				}
			}
		}else{
			var parentTask = task.parentTask;
			for(var i = 0; i < parentTask.cldTasks.length; i++){
				var pcTask = parentTask.cldTasks[i];
				if(pcTask.id == id){
					if(pcTask.nextChildTask){
						if(pcTask.previousChildTask){
							pcTask.previousChildTask.nextChildTask = pcTask.nextChildTask;
							pcTask.nextChildTask.previousChildTask = pcTask.previousChildTask;
						}else{
							pcTask.nextChildTask.previousChildTask = null;
						}
					}else{
						if(pcTask.previousChildTask){
							pcTask.previousChildTask.nextChildTask = null;
						}
					}
					pcTask = null;
					parentTask.cldTasks.splice(i, 1);
					break;
				}
			}
		}
	}
});

});
