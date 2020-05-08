/*jshint node:true */
define([
	"../../buildControl",
	"../../fileUtils",
	"dojo/has"
], function(bc, fileUtils, has){
	// start up a few processes to compensate for the miserably slow closure compiler

	var processes = [],
		killAllRunners = function(){
			processes.forEach(function(proc){
				try{
					proc.runner && proc.runner.kill();
					proc.runner = 0;
				}catch(e){
					//squelch
				}
			});
		};

	// don't leave orphan child procs
	global.process.on("exit", killAllRunners);
	global.process.on("uncaughtException", function(err){
		killAllRunners();
		// TODO: log these via bc.log
		console.log(err);
		console.log(err.stack);
		global.process.exit(-1);
	});

	if(bc.maxOptimizationProcesses < 0){
		bc.maxOptimizationProcesses = require.nodeRequire('os').cpus().length;
	}
	var
		processesStarted = 0,
		totalOptimizerOutput = "",
		nextProcId = 0,
		i, //used in for loop
		sendJob = function(src, dest, optimizeSwitch, copyright){
			processes[nextProcId++].write(src, dest, optimizeSwitch, copyright);
			nextProcId= nextProcId % bc.maxOptimizationProcesses;
		},
		doneRe = new RegExp("^Done\\s\\(compile\\stime.+$", "m"),
		optimizerRunner = require.toUrl("build/optimizeRunner.js"),
		buildRoot = optimizerRunner.match(/(.+)\/build\/optimizeRunner\.js$/)[1],
		runJava, //function, defined later
		oldSendJob = sendJob, //preserves reference if sendJob is replaced
		child_process = require.nodeRequire("child_process"),
		isCygwin = global.process.platform === 'cygwin',
		separator = has("is-windows") ? ";" : ":",
		javaClasses = fileUtils.catPath(buildRoot, "closureCompiler/compiler.jar") + separator + fileUtils.catPath(buildRoot, "shrinksafe/js.jar") + separator + fileUtils.catPath(buildRoot, "shrinksafe/shrinksafe.jar");
	if(isCygwin){
		//assume we're working with Windows Java, and need to translate paths
		runJava = function(cb){
			child_process.exec("cygpath -wp '" + javaClasses + "'", function(err, stdout){
				javaClasses = stdout.trim();
				child_process.exec("cygpath -w '" + optimizerRunner + "'", function(err, stdout){
					optimizerRunner = stdout.trim();
					cb();
				});
			});
		};
		//wrap sendJob calls to convert to windows paths first
		sendJob = function(src, dest, optimizeSwitch, copyright){
			child_process.exec("cygpath -wp '" + src + "'", function(err, srcstdout){
				child_process.exec("cygpath -wp '" + dest + "'", function(err, deststdout){
					oldSendJob(srcstdout.trim(), deststdout.trim(),
						optimizeSwitch, copyright);
				});
			});
		};
	}else if(has("is-windows")){
		runJava = function(cb){
			javaClasses = fileUtils.normalize(javaClasses);
			optimizerRunner = fileUtils.normalize(optimizerRunner);
			cb();
		};
		sendJob = function(src, dest, optimizeSwitch, copyright){
			var wsrc = fileUtils.normalize(src);
			var wdest = fileUtils.normalize(dest);
			oldSendJob(wsrc, wdest, optimizeSwitch, copyright);
		};
	}else{
		//no waiting necessary, pass through
		runJava = function(cb) { cb(); };
	}
	runJava(function() {
		for(i = 0; i < bc.maxOptimizationProcesses; i++) {(function(){
			var
				runner = child_process.spawn("java", ["-cp", javaClasses, "org.mozilla.javascript.tools.shell.Main", optimizerRunner]),
				proc = {
					runner:runner,
					results:"",
					tempResults:"",
					sent:[],
					write:function(src, dest, optimizeSwitch, copyright){
						proc.sent.push(dest);
						runner.stdin.write(src + "\n" + dest + "\n" + optimizeSwitch + "\n" + JSON.stringify({ copyright: copyright, options: bc.optimizeOptions, useSourceMaps: bc.useSourceMaps }) + "\n");
					},
					sink:function(output){
						proc.tempResults += output;
						var match, message, chunkLength;
						while((match = proc.tempResults.match(doneRe))){
							message = match[0];
							if(/OPTIMIZER\sFAILED/.test(message)){
								bc.log("optimizeFailed", ["module identifier", proc.sent.shift(), "exception", message.substring(5)]);
							}else{
								bc.log("optimizeDone", [proc.sent.shift() + " " + message.substring(5)]);
							}
							chunkLength = match.index + message.length;
							proc.results += proc.tempResults.substring(0, chunkLength);
							proc.tempResults = proc.tempResults.substring(chunkLength);
						}
					}
				};
			processesStarted++; // matches *3*
			runner.stdout.on("data", function(data){
				// the +"" converts to Javascript string
				proc.sink(data + "");
			}),
			runner.stderr.on("data", function(data){
				// the +"" converts to Javascript string
				proc.sink(data + "");
			}),
			runner.on("exit", function(){
				proc.results += proc.tempResults;
				totalOptimizerOutput += proc.results;
				bc.logOptimizerOutput(totalOptimizerOutput);
				processesStarted--; // matches *3*
				if(!processesStarted){
					// all the processes have completed and shut down at this point
					if(bc.showOptimizerOutput){
						bc.log("optimizeMessages", [totalOptimizerOutput]);
					}
					bc.passGate(); // matched with *1*
				}
			});
			processes.push(proc);
		})();}
	}); //end runJava(function...)

	bc.gateListeners.push(function(gate){
		if(gate==="cleanup"){
			// going through the cleanup gate signals that all optimizations have been started;
			// we now signal the runner there are no more files and wait for the runner to stop
			bc.log("pacify", "waiting for the optimizer runner to finish...");
			bc.waiting++;  // matched with *1*
			processes.forEach(function(proc){
				proc.write(".\n");
			});
			processes = [];
		}
	});

	return sendJob;
});
