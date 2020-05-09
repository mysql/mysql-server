define([
	'require',
	'intern!object',
	'intern/chai!assert',
	'intern/dojo/node!leadfoot/helpers/pollUntil'
], function (require, registerSuite, assert, pollUntil) {
	var remote;

	/*jshint -W020 */
	/* global moveEvents, downEvents */
	registerSuite({
		name: 'eventMouse',

		setup: function () {
			remote = this.get('remote');

			return remote
				.get(require.toUrl('./eventMouse.html'))
				.then(pollUntil('return window.ready;', null, 3000))
				.findById('header')
				.moveMouseTo()
				.click()
				.end();
		},

		'mouseenter/mouseleave': {
			'enter middle': function () {
				return remote
					.execute(function () {
						moveEvents = [];
					})

					.findById('outer')
					.moveMouseTo()
					.end()

					.sleep(1000)

					.findById('middleLabel')
					.moveMouseTo()
					.end()

					.execute(function () {
						return moveEvents;
					})
					.then(function (moveEvents) {
						assert.strictEqual(moveEvents.length, 1);
						assert.strictEqual(moveEvents[0].event, 'mouseenter');
						assert.strictEqual(moveEvents[0].target, 'outer');
					});
			},

			'enter inner1': function () {
				return remote
					.execute(function () {
						moveEvents = [];
					})
					.findById('inner1')
					.moveMouseTo()
					.end()

					.execute(function () {
						return moveEvents;
					})
					.then(function (moveEvents) {
						assert.strictEqual(moveEvents.length, 0);
					});
			},

			'after outer': function () {
				return remote
					.execute(function () {
						moveEvents = [];
					})
					.findById('outer')
					.moveMouseTo()
					.end()

					.sleep(1000)

					.findById('afterOuter')
					.moveMouseTo()
					.end()

					.execute(function () {
						return moveEvents;
					})
					.then(function (moveEvents) {
						assert.strictEqual(moveEvents.length, 1);
						assert.strictEqual(moveEvents[0].event, 'mouseleave');
						assert.strictEqual(moveEvents[0].target, 'outer');
					});
			}
		},

		'mousedown, stopEvent': {
			'mousedown inner1 div': function () {
				return remote
					.execute(function () {
						downEvents = [];
					})
					.findById('inner1')
					.moveMouseTo()
					.click()
					.end()

					.execute(function () {
						return downEvents;
					})
					.then(function (downEvents) {
						assert.strictEqual(downEvents.length, 2);
						assert.strictEqual(downEvents[0].event, 'mousedown');
						assert.strictEqual(downEvents[0].target, 'inner1');
						assert.isTrue(downEvents[0].isLeft, 'expected left button');
						assert.isFalse(downEvents[0].isRight, 'did not expect right button');
						assert.strictEqual(downEvents[1].event, 'mousedown');
						assert.strictEqual(downEvents[1].currentTarget, 'middle');
						assert.strictEqual(downEvents[1].target, 'inner1');
						assert.isTrue(downEvents[1].isLeft, 'expected left button');
						assert.isFalse(downEvents[1].isRight, 'did not expect right button');
					});
			},

			'mousedown outer div': function () {
				return remote
					.execute(function () {
						downEvents = [];
					})
					.findById('outerLabel')
					.moveMouseTo()
					// middle mouse button
					.clickMouseButton(1)
					.end()

					.execute(function () {
						return downEvents;
					})
					.then(function (downEvents) {
						assert.strictEqual(downEvents.length, 1);
						assert.strictEqual(downEvents[0].event, 'mousedown');
						assert.strictEqual(downEvents[0].target, 'outerLabel');
						assert.strictEqual(downEvents[0].currentTarget, 'outer');

						// TODO: Selenium isn't getting the button number for middle-click events
						// assert.isFalse(downEvents[0].isLeft, 'did not expect left button');
						// assert.isTrue(downEvents[0].isRight, 'expected right button');
					});
			}
		}
	});
});
