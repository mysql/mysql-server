define(["doh", "dojox/app/main", "dojox/json/ref", "dojo/text!./error1.json", "dojo/text!./error2.json",
	"dojo/text!./error3.json", "dojo/text!./error4.json", "dojo/text!./error5.json", "dojo/text!./errorLast.json", "dojo/topic"],
	function(doh, Application, json, config1, config2, config3, config4, config5, configLast, topic){
	doh.register("dojox.app.tests.doh.error", [
		{
			timeout: 4000,
			name: "error1",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				Application(json.fromJson(config1));
				// we need to check that before timeout we _never_ entered the START (2) state
				setTimeout(dohDeferred.getTestCallback(function(){
					t.assertEqual([1], events);
				}), 3000);
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				// maybe dojox/app should do that?
				delete testApp;
			}
		},
		{
			timeout: 4000,
			name: "error2",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				Application(json.fromJson(config2));
				// we need to check that before timeout we _never_ entered the START (2) state
				setTimeout(dohDeferred.getTestCallback(function(){
					t.assertEqual([1], events);
				}), 3000);
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				// maybe dojox/app should do that?
				delete testApp;
			}
		},
		{
			timeout: 4000,
			name: "error3",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				Application(json.fromJson(config3));
				// we need to check that before timeout we _never_ entered the START (2) state
				setTimeout(dohDeferred.getTestCallback(function(){
					t.assertEqual([1], events);
				}), 3000);
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				// maybe dojox/app should do that?
				delete testApp;
			}
		},
		{
			timeout: 4000,
			name: "error4",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				Application(json.fromJson(config4));
				// we need to check that before timeout we _never_ entered the START (2) state
				setTimeout(dohDeferred.getTestCallback(function(){
					t.assertEqual([1], events);
				}), 3000);
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				// maybe dojox/app should do that?
				delete testApp;
			}
		},
		{
			timeout: 4000,
			name: "error5",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				var goterror = false;
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				require(["dojox/app/main"], function(Application){
					require.on ? require.on("error", function(){
						goterror = true;
					}) : null;
					Application(json.fromJson(config5));
					// we need to check that before timeout we _never_ entered the START (2) state
					// and we must check error has been thrown
					setTimeout(dohDeferred.getTestCallback(function(){
						t.assertEqual([1], events);
						t.assertTrue(goterror);
					}), 3000);
				});
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				// maybe dojox/app should do that?
				delete testApp;
			}
		},
		{
			timeout: 4000,
			name: "errorLast",
			runTest: function(t){
				var dohDeferred = new doh.Deferred();
				// stack events that are pushed
				var events = [];
				this._topic = topic.subscribe("/app/status", function(evt){
					events.push(evt);
				});
				// to workaround http://trac.dojotoolkit.org/ticket/16032
				for(var p in require.waiting){
					delete require.waiting[p];
				}
				require.execQ.length = 0;
				require(["dojox/app/main"], function(Application){
					Application(json.fromJson(configLast));
					// we need to check that before timeout we _never_ entered the START (2) state
					setTimeout(dohDeferred.getTestCallback(function(){
						t.assertEqual([1], events);
					}), 3000);
				});
				return dohDeferred;
			},
			tearDown: function(){
				this._topic.remove();
				for(var p in require.waiting){
					delete require.waiting[p];
				}
				require.execQ.length = 0;
				// maybe dojox/app should do that?
				delete testApp;
			}
		}
	]);
});
