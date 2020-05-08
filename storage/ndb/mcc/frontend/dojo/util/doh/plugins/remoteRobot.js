define("doh/plugins/remoteRobot", ["doh/runner", "dojo/_base/lang"], function(runner, lang){

/*=====
return {
	// summary:
	//		Plugin that bridges the doh.robot and WebDriver APIs.
};
=====*/
	
	// read in the test and port parameters from the URL
	var remoteRobotURL = "";
	var paths = "";
	var qstr = window.location.search.substr(1);
	if(qstr.length){	
	var qparts = qstr.split("&");
		for(var x=0; x<qparts.length; x++){
			var tp = qparts[x].split("="), name = tp[0], value = tp[1].replace(/[<>"'\(\)]/g, "");	// replace() to avoid XSS attack
			//Avoid URLs that use the same protocol but on other domains, for security reasons.
			if (value.indexOf("//") === 0 || value.indexOf("\\\\") === 0) {
				throw "Insupported URL";
			}
			switch(name){
				case "remoteRobotURL":
					remoteRobotURL = value;
					break;
				case "paths":
					paths = value;
					break;
			}
		}
	}
	// override doh runner so that it appends the remote robot url to each test
	runner._registerUrl = (function(oi){
		return lang.hitch(runner, function(group, url, timeout, type, dohArgs){
			// append parameter, or specify new query string if appropriate
			if(remoteRobotURL){
				url += (/\?/.test(url)?"&":"?") + "remoteRobotURL=" + remoteRobotURL
			}
			if(paths){
				url += (/\?/.test(url)?"&":"?") + "paths=" + paths;
			}
			oi.apply(runner, [group, url, timeout, type, dohArgs]);
		});
	})(runner._registerUrl);
	return remoteRobotURL;
});