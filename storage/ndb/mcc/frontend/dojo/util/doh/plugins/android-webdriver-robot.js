define(["dojo/has", "dojo/_base/xhr", "doh/runner", "dojo/sniff", "dojo/dom-geometry", "doh/plugins/remoteRobot"], function(has, xhr, doh, sniff, geom, remoteRobotURL){
	// summary:
	//		Wraps WebDriver APIs around doh.robot API.
	//		Should be loaded as a doh plugin.
	//		In theory, with a change to the base URL, this same file could also be used for iOS WebDriver.
	// 		WebDriver must be modified to accept cross-domain requests (currently it sends incomplete headers).
	//
	
	var top=window.parent?window.parent:window;
	
	// short-term FIFO command queue, similar to the one in the applet
	var commands=[];
	var _inFlight=false;
	// send requests to the WebDriver and get its JSON text back
	function robotXHR(args){
		var commandString=args.commandString;
		var deferred=args.deferred;
		var immediate=args.immediate;
		//console.debug("remote url: "+remoteRobotURL+"/"+commandString);
		if(immediate||!_inFlight){
			_inFlight=true;
			xhr(args.method,{
				url:remoteRobotURL+"/"+commandString,
				headers: { "Content-Type": "application/json"},
				postData:args.postData,
				load:function(response){
					//console.debug("success sending webdriver command: ", response);
					_inFlight=false;
					if(deferred){
						deferred.callback(response);
					}
					if(commands.length){
						robotXHR(commands.shift());
					}
				},
				error:function(response){
					console.error("failure sending webdriver command: ", response);
				}
			});
		}else{
			commands.push(args);
		}
	}
	
	// record stats about last mouse position
	var lastX=0;
	var lastY=0;
	var mouse=null; // debug mouse cursor so you can see what is being touched; created on demand
	// map dojo keys to WebDriver key codes
	// enable/disable as we discover which ones are legit
	
	var keyMap={};
	if(window["dojo"]){
		// keyMap[dojo.keys.BACKSPACE]=0xE003;
		//keyMap[dojo.keys.TAB]=0xE004;
		// keyMap[dojo.keys.CLEAR]=0xE005;
		// keyMap[dojo.keys.ENTER]=0xE007;
		// keyMap[dojo.keys.SHIFT]=0xE008;
		// keyMap[dojo.keys.CTRL]=0xE009;
		// keyMap[dojo.keys.ALT]=0xE00A;
		// keyMap[dojo.keys.META]=0xE03D;		// the apple key on macs
		// keyMap[dojo.keys.PAUSE]=0xE00B;
		// // dojo.keys.CAPS_LOCK: 20,
		// keyMap[dojo.keys.ESCAPE]=0xE00C;
		// keyMap[dojo.keys.SPACE]=0xE00D;
		// keyMap[dojo.keys.PAGE_UP]=0xE00E;
		// keyMap[dojo.keys.PAGE_DOWN]=0xE00F;
		keyMap[dojo.keys.END]=0xE010;
		keyMap[dojo.keys.HOME]=0xE011;
		keyMap[dojo.keys.LEFT_ARROW]=0xE012;
		keyMap[dojo.keys.UP_ARROW]=0xE013;
		keyMap[dojo.keys.RIGHT_ARROW]=0xE014;
		keyMap[dojo.keys.DOWN_ARROW]=0xE015;
		// keyMap[dojo.keys.INSERT]=0xE016;
		// keyMap[dojo.keys.DELETE]=0xE017;
		// // dojo.keys.HELP: 47,
		// // dojo.keys.LEFT_WINDOW: 91,
		// // dojo.keys.RIGHT_WINDOW: 92,
		// // dojo.keys.SELECT: 93,
		// keyMap[dojo.keys.NUMPAD_0]=0xE01A;
		// keyMap[dojo.keys.NUMPAD_1]=0xE01B;
		// keyMap[dojo.keys.NUMPAD_2]=0xE01C;
		// keyMap[dojo.keys.NUMPAD_3]=0xE01D;
		// keyMap[dojo.keys.NUMPAD_4]=0xE01E;
		// keyMap[dojo.keys.NUMPAD_5]=0xE01F;
		// keyMap[dojo.keys.NUMPAD_6]=0xE020;
		// keyMap[dojo.keys.NUMPAD_7]=0xE021;
		// keyMap[dojo.keys.NUMPAD_8]=0xE022;
		// keyMap[dojo.keys.NUMPAD_9]=0xE023;
		// keyMap[dojo.keys.NUMPAD_MULTIPLY]=0xE024;
		// keyMap[dojo.keys.NUMPAD_PLUS]=0xE025;
		// keyMap[dojo.keys.NUMPAD_ENTER]=0xE026;
		// keyMap[dojo.keys.NUMPAD_MINUS]=0xE027;
		// keyMap[dojo.keys.NUMPAD_PERIOD]=0xE028;
		// keyMap[dojo.keys.NUMPAD_DIVIDE]=0xE029;
		keyMap[dojo.keys.F1]=0xE031;
		keyMap[dojo.keys.F2]=0xE032;
		keyMap[dojo.keys.F3]=0xE033;
		keyMap[dojo.keys.F4]=0xE034;
		keyMap[dojo.keys.F5]=0xE035;
		keyMap[dojo.keys.F6]=0xE036;
		keyMap[dojo.keys.F7]=0xE037;
		keyMap[dojo.keys.F8]=0xE038;
		keyMap[dojo.keys.F9]=0xE039;
		keyMap[dojo.keys.F10]=0xE03A;
		keyMap[dojo.keys.F11]=0xE03B;
		keyMap[dojo.keys.F12]=0xE03C;
	}
	var lastElement={node:null,reference:""};
	// replace applet with methods to call WebDriver
	_robot={
		_setKey:function(sec){
			// initialization
			// switch WebDriver to test frame so it can resolve elements!
			/*robotXHR({
				method:"POST",
				commandString:"frame",
				postData:'{"id":"testBody"}',
				deferred:null,
				immediate:false
			});*/
			// skip keyboard initialization
			doh.robot._onKeyboard();
		},
		
		_callLoaded:function(sec){
			// shouldn't be called
		},
		
		_initKeyboard:function(sec){
			// shouldn't be called
		},
		
		_initWheel:function(sec){
			// shouldn't be called
		},
		
		setDocumentBounds:function(sec, docScreenX, docScreenY, width, height){
			// shouldn't be called
		},
		
		_spaceReceived:function(){
			// shouldn't be called
		},
		
		_notified:function(sec, keystring){
			// shouldn't be called
		},
		
		_nextKeyGroupACK: function(sec){
			// shouldn't be called
		},
		
		typeKey:function(sec, charCode, keyCode, alt, ctrl, shift, meta, delay, async){
			// send keys to active element
			var deferred=new dojo.Deferred();
			// after active element received...
			deferred.then(function(response){
				// remove garbage characters
				response=response.replace(/\{/g,"({").replace(/\}/g,"})");
				response=response.replace(/[^ -~]/g, "");
				//response=response.substring(0,response.lastIndexOf(")")+1);
				var json=dojo.fromJson(response);
				var activeElement=json.value.ELEMENT;
				// send keys to active element
				var keys=(ctrl?'"'+String.fromCharCode(dojo.keys.CTRL)+'",':'')
					+(shift?'"'+String.fromCharCode(dojo.keys.SHIFT)+'",':'');
				if(keyCode in keyMap){
					keys+='"'+String.fromCharCode(keyMap[keyCode])+'"';
				}else if(keyCode){
					keys+='"'+String.fromCharCode(keyCode)+'"';
				}else{
					keys+='"'+String.fromCharCode(charCode)+'"';
				}
				robotXHR({
					method:"POST",
					commandString:"element/"+activeElement+"/value",
					postData:'{"value":['+keys+']}',
					deferred:null,
					immediate:false
				});
			});
			// get active element to send keys to
			// strangely, this request is a POST in the WebDriver API??
			robotXHR({
				method:"POST",
				commandString:"execute",
				postData:'{"script":"return document.activeElement;","args":[]}',
				deferred:deferred,
				immediate:false
			});
		},
		
		/* ---- do we need to add the keyUp and keyDown here? */
		upKey:function(sec,charCode,keyCode,delay){
			//robotXHR("upKey?sec="+sec+"&charCode="+charCode+"&keyCode="+keyCode+"&delay="+delay);
		},
		
		downKey:function(sec,charCode,keyCode,delay){
			//robotXHR("downKey?sec="+sec+"&charCode="+charCode+"&keyCode="+keyCode+"&delay="+delay);
		},
		
		moveMouse:function(sec, x, y, delay, duration){
			// x,y are not being computed relative to the mobile screen for some reason (probably iframe)
			x=Math.round(x);
			y=Math.round(y);
			if(!mouse){
				// create fake mouse
				mouse=dojo.doc.createElement("div");
				dojo.style(mouse, {
					// x, y relative to screen (same coordinates as WebDriver)
					position:"fixed",
					left:"0px",
					top:"0px",
					width:"5px",
					height:"5px",
					"background-color":"red"
				});
				dojo.body().appendChild(mouse);
			}
			// fix x and y
			lastX=x-top.scrollX;
			lastY=y-top.scrollY;
			// cursor needs to be away from center of event or else the cursor itself will interfere with the event!
			mouse.style.left=(x+5)+"px";
			mouse.style.top=(y+5)+"px";
			//mouse.style.left=((lastX+top.scrollX+dojo.global.scrollX)+5)+"px";
			//mouse.style.top=((lastY+top.scrollY+dojo.global.scrollY)+5)+"px";
			robotXHR({
				method:"POST",
				commandString:"touch/move",
				postData:'{"x":'+lastX+',"y":'+lastX+'}',
				//content:{x:x,y:y},
				deferred:null,
				immediate:false
			});
		},
		
		pressMouse:function(sec, left, middle, right, delay){
			//robotXHR("pressMouse?sec="+sec+"&left="+left+"&middle="+middle+"&right="+right+"&delay="+delay,null,true);
			var deferred=new dojo.Deferred();
			deferred.then(function(){
				mouse.style.backgroundColor="yellow";
			});
			robotXHR({
				method:"POST",
				commandString:"touch/down",
				postData:'{"x":'+(lastX)+',"y":'+(lastY)+'}',
				//content:{x:lastX,y:lastY},
				deferred:deferred,
				immediate:false
			});
		},
		
		releaseMouse:function(sec, left, middle, right, delay){
			//robotXHR("releaseMouse?sec="+sec+"&left="+left+"&middle="+middle+"&right="+right+"&delay="+delay,null,true);
			var deferred=new dojo.Deferred();
			deferred.then(function(){
				mouse.style.backgroundColor="red";
			});
			robotXHR({
				method:"POST",
				commandString:"touch/up",
				postData:'{"x":'+(lastX+1)+',"y":'+(lastY+1)+'}',
				//content:{x:lastX,y:lastY},
				deferred:deferred,
				immediate:false
			});
		},
		
		// doh.robot will call these functions in place of its own
		typeKeys:function(/*String||Number*/ chars, /*Integer, optional*/ delay, /*Integer, optional*/ duration){
			// send keys to active element
			var deferred=new dojo.Deferred();
			// after active element received...
			deferred.then(function(response){
				response=response.replace(/\{/g,"({").replace(/\}/g,"})");
				response=response.replace(/[^ -~]/g, "");
				//response=response.substring(0,response.lastIndexOf(")")+1);
				var json=dojo.fromJson(response);
				var activeElement=json.value.ELEMENT;
				// send keys to active element
				var deferred2= new dojo.Deferred();
				deferred2.then(function(){
					lastElement={node:dojo.doc.activeElement,reference:activeElement};
				});
				robotXHR({
					method:"POST",
					commandString:"element/"+activeElement+"/value",
					// TODO: add codes to sent array for press and release modifiers
					postData:'{"value":["'+chars+'"]}',
					deferred:deferred2,
					immediate:false
				});
			});
			// get active element to send keys to
			// strangely, this request is a POST in the WebDriver API??
			if(dojo.doc.activeElement!=lastElement.node){
				lastElement={node:dojo.doc.activeElement,reference:null};
				robotXHR({
					method:"POST",
					commandString:"execute",
					postData:"{script:'return document.activeElement;',args:[]}",
					deferred:deferred,
					immediate:false
				});
			}else{
				deferred.callback("{value:{ELEMENT:'"+lastElement.reference+"'}}");
			}
		},
		
		_scrollIntoView:function(node){
			// for whatever reason, scrollIntoView does not work...
			//node.scrollIntoView(true);
			p = geom.position(node);
			// scrolling the iframe doesn't seem to do anything
			//dojo.global.scrollTo(p.x,p.y);
			// this seems to work
			top.scrollTo(p.x,p.y);
			// this is also reasonable
			/*var scrollBy={dx:p.x-top.scrollX,dy:p.y-top.scrollY};
			robotXHR({
				method:"POST",
				commandString:"touch/scroll",
				// TODO: add codes to sent array for press and release modifiers
				postData:'{"xoffset":'+scrollBy.dx+', "yoffset":'+scrollBy.dy+'}',
				deferred:null,
				immediate:false
			});*/
		}
	};
	
	// robot.js will use this robot instead of the applet
	has.add("doh-custom-robot", function(){
		return has("android") && _robot;
	});
	return _robot;
});
