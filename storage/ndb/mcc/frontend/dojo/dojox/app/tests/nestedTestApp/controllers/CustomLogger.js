define(["dojo/_base/declare", "dojo/dom", "dojo/dom-style",
	"dojo/dom-class", "dojo/dom-attr", "dojo/dom-construct", "dojo/_base/config", "dojo/sniff",
	"dojox/app/Controller"],
	function (declare, dom, domStyle, domClass, domAttr, domConstruct, dconfig, has, Controller){
		// module:
		//		dapp/tests/nestedTestApp/controllers/CustomLogger
		// summary:
		//		A custom logger to handle a special case to only log messages for transitions
		//		It will replace the app.log function with the one in this controller
		//
		return declare(Controller, {

			constructor: function (app){
				// summary:
				//		bind "app-initLayout", "app-layoutView" and "app-resize" events on application instance.
				//
				// app:
				//		dojox/app application instance.
				// dojo mobile events instead
				app.appLogging=app.appLogging || {};
				app.appLogging.loggingList=app.appLogging.loggingList || [];

				has.add("app-log-api", ((dconfig["app"] || {}).debugApp) || app.appLogging["logAll"]);
				has.add("app-log-partial", app.appLogging.loggingList.length > 0);

				if(has("app-log-api") || has("app-log-partial")){
					app.log=function (){
						// summary:
						// If config is set to turn on app logging, then log msg to the console
						//
						// arguments:
						// the message to be logged,
						// all but the last argument will be treated as Strings and be concatenated together,
						// the last argument can be an object it will be added as an argument to the console.log
						var msg="";
						if(app.appLogging.logTimeStamp){
							msg=msg + new Date().getTime() + " ";
						}
						if(has("app-log-api") || app.appLogging["logAll"]){ // log all messages
							try{
								for(var i=0; i < arguments.length - 1; i++){
									msg=msg + arguments[i] + " ";
								}
								console.log(msg, arguments[arguments.length - 1]);
							} catch (e){
							}
						} else if(has("app-log-partial")){ // only log specific things
							try{
								if(app.appLogging.loggingList.indexOf(arguments[0]) > -1){ // if the 1st arg is in the loggingList log it
									for(var i=2; i < arguments.length - 1; i++){
										msg=msg + arguments[i];
									}
									console.log(msg, arguments[arguments.length - 1]);
								}
							} catch (e){
							}

						}
					};
				}
			}
		});
	});
