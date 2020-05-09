require([
	'dojo/_base/array',
	'dojo/Deferred',
	'dojo/promise/all',
	'dojo/string',
	'dojo/window',
	'dojo/dom-geometry',
	'dojo/_base/window',
	'dojo/on',
	'dojo/query',
	'./iframe_content/scrollDocuments.js',
	'dojo/text!./iframe_content/scrollItem.html',
	'dojo/domReady!'
], function (
	arrayUtil,
	Deferred,
	all,
	string,
	winUtils,
	domGeo,
	baseWin,
	on,
	query,
	docs,
	scrollItemTemplate
) {
	// This script runs in the functional testing page. It renders all of the
	// document layout	scenarios and hooks up events for the test runner.

	var promises = [];

	function isInViewport(win, element) {
		var rect = element.getBoundingClientRect();
		var documentElement = win.document.documentElement || {};
		var viewPortHeight = win.innerHeight || documentElement.clientHeight || win.document.body.clientHeight;
		var viewPortWidth = win.innerWidth || documentElement.clientWidth || win.document.body.clientWidth;
		return (rect.bottom <= viewPortHeight || rect.top < viewPortHeight) && rect.right <= viewPortWidth;
	}

	// update the input elements so the test runner and inspect the output
	function updateCalculations(win, rootElement, targetElement, setName, hasScrolled) {
		var inView = isInViewport(win, targetElement);
		query('.' + setName, rootElement)[0].value = inView ? 1 : 0;
		query('.hasScrolled', rootElement)[0].value = hasScrolled ? 1 : 0;
	}

	function createIframe(html, iframeRoot, root) {
		var dfd = new Deferred();
		var iframe = document.createElement('iframe');
		var iframeDoc;
		var iframeWin;

		iframe.frameBorder = '0';
		iframe.allowTransparency = 'true';

		iframeRoot.appendChild(iframe);

		iframeWin = iframe.contentWindow.window;
		iframeDoc = iframeWin.document;

		try {
			iframeDoc.open();
		}
		catch (e) {
			iframe.src = 'javascript:var d=document.open();d.domain="' + document.domain + '";void(0);';
			iframeDoc.open();
		}

		// The iframe will call this function to let us know its ready for
		// testing.
		iframeWin.ready = function () {
			var target = iframeDoc.getElementById('it');
			var winScrollBefore = baseWin.withGlobal(iframeWin, 'docScroll', domGeo, []);

			updateCalculations(iframeWin, root, target, 'before', false);

			on(query('.scrollBtn', root)[0], 'click', function () {
				var hasScrolled;
				var winScrollAfter;
				winUtils.scrollIntoView(target);
				winScrollAfter = baseWin.withGlobal(iframeWin, 'docScroll', domGeo, []);
				hasScrolled = (winScrollBefore.y !== winScrollAfter.y || winScrollBefore.x !== winScrollAfter.x);
				updateCalculations(iframeWin, root, target, 'after', hasScrolled);
			});

			dfd.resolve();
		};

		iframeDoc.write(html);
		iframeDoc.close();

		promises.push(dfd.promise);
	}

	arrayUtil.forEach(docs, function (doc) {
		var html = string.substitute(scrollItemTemplate, {
			label: doc.label,
			testId: doc.id
		});
		var div = document.createElement('div');
		div.innerHTML = html;
		document.body.appendChild(div);
		createIframe(doc.html, query('.iframeRoot', div)[0], div);
	});

	all(promises).then(function () {
		// signal to intern we are ready to test
		window.ready = true;
	});
});
