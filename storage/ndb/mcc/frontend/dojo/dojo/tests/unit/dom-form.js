define([
	'intern!object',
	'intern/chai!assert',
	'../../dom-form',
	'dojo/dom',
	'dojo/dom-construct',
	'dojo/json'
], function (registerSuite, assert, domForm, dom, domConstruct, JSON) {
	var formContainer;

	var f1 = {
		blah: 'blah'
	};
	var f1_query = 'blah=blah';
	var f2 = {
		blah: 'blah',
		multi: [ 'thud', 'thonk' ],
		textarea: 'textarea_value'
	};
	var f2_query = 'blah=blah&multi=thud&multi=thonk&textarea=textarea_value';
	var f3 = {
		spaces: 'string with spaces'
	};
	var f3_query = 'spaces=string%20with%20spaces';
	var f4 = {
		action: 'Form with input named action'
	};
	var f4_query = 'action=Form%20with%20input%20named%20action';
	var f5 = {
		'blåh': 'bláh'
	};
	var f5_query = 'bl%C3%A5h=bl%C3%A1h';
	var f6 = {
		cb_group: 'foo',
		radio_group: 'bam'
	};
	var f6_query = 'cb_group=foo&radio_group=bam';
	var f6_1 = {
		cb_group: 'boo',
		radio_group: 'baz'
	};
	var f6_2 = {
		cb_group: ['foo', 'boo'],
		radio_group: 'baz'
	};
	registerSuite({
		name: 'dojo/dom-form',

		setup: function () {
			formContainer = domConstruct.place('<div>' +
				'<form id="f1" style="border: 1px solid black;">' +
					'<input id="f1_blah" type="text" name="blah" value="blah">' +
					'<input id="f1_no_value" type="text" name="no_value" value="blah" disabled>' +
					'<input  id="f1_no_value2" type="button" name="no_value2" value="blah">' +
				'</form>' +
				'<form id="f2" style="border: 1px solid black;">' +
					'<input id="f2_blah" type="text" name="blah" value="blah">' +
					'<input id="f2_no_value" type="text" name="no_value" value="blah" disabled>' +
					'<input  id="f2_no_value2" type="button" name="no_value2" value="blah">' +
					'<select  id="f2_multi" type="select" multiple name="multi" size="5">' +
						'<option value="blah">blah</option>' +
						'<option value="thud" selected>thud</option>' +
						'<option value="thonk" selected>thonk</option>' +
					'</select>' +
					'<textarea id="f2_textarea" name="textarea">textarea_value</textarea>' +
					'<button id="f2_button1" name="button1" value="buttonValue1">This is a button that should not be in formToObject.</button>' +
					'<input id="f2_fileParam1" type="file" name="fileParam1" value="fileValue1"> File input should not show up in formToObject.' +
				'</form>' +
				'<form id="f3" style="border: 1px solid black;">' +
					'<input id="f3_spaces" type="hidden" name="spaces" value="string with spaces">' +
				'</form>' +
				'<form id="f4" style="border: 1px solid black;" action="xhrDummyMethod.php">' +
					'<input id="f4_action" type="hidden" name="action" value="Form with input named action">' +
				'</form>' +
				'<form id="f5" style="border: 1px solid black;">' +
					'<input id="f5_blah" type="text" name="blåh" value="bláh">' +
					'<input id="f5_no_value" type="text" name="no_value" value="blah" disabled>' +
					'<input id="f5_no_value2" type="button" name="no_value2" value="blah">' +
				'</form>' +
				'<form id="f6" style="border: 1px solid black;">' +
					'<input id="f6_checkbox1" type="checkbox" name="cb_group" value="foo" checked>' +
					'<input id="f6_checkbox2" type="checkbox" name="cb_group" value="boo">' +
					'<input id="f6_radio1" type="radio" name="radio_group" value="baz">' +
					'<input id="f6_radio2" type="radio" name="radio_group" value="bam" checked>' +
				'</form>' +
			'</div>', document.body);
		},

		teardown: function () {
			domConstruct.destroy(formContainer);
			formContainer = null;
		},

		'.fieldToObject': {
			'ids': function () {
				assert.strictEqual(domForm.fieldToObject('f1_no_value'), null);
				assert.strictEqual(domForm.fieldToObject('f1_no_value2'), 'blah');
				assert.deepEqual(domForm.fieldToObject('f2_multi'), f2.multi);
				assert.strictEqual(domForm.fieldToObject('f2_textarea'), f2.textarea);
				assert.strictEqual(domForm.fieldToObject('f2_fileParam1'), '');
				assert.strictEqual(domForm.fieldToObject('f4_action'), f4.action);
				assert.strictEqual(domForm.fieldToObject('f6_checkbox1'), 'foo');
				assert.strictEqual(domForm.fieldToObject('f6_checkbox2'), null);
				assert.strictEqual(domForm.fieldToObject('f6_radio1'), null);
				assert.strictEqual(domForm.fieldToObject('f6_radio2'), 'bam');
			},

			'nodes': function () {
				assert.strictEqual(domForm.fieldToObject(dom.byId('f1_no_value')), null);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f1_no_value2')), 'blah');
				assert.deepEqual(domForm.fieldToObject(dom.byId('f2_multi')), f2.multi);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f2_textarea')), f2.textarea);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f2_fileParam1')), '');
				assert.strictEqual(domForm.fieldToObject(dom.byId('f4_action')), f4.action);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f6_checkbox1')), 'foo');
				assert.strictEqual(domForm.fieldToObject(dom.byId('f6_checkbox2')), null);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f6_radio1')), null);
				assert.strictEqual(domForm.fieldToObject(dom.byId('f6_radio2')), 'bam');
			}
		},

		'.toObject': function () {
			assert.deepEqual(domForm.toObject('f1'), f1);
			assert.deepEqual(domForm.toObject('f2'), f2);
			assert.deepEqual(domForm.toObject('f3'), f3);
			assert.deepEqual(domForm.toObject('f4'), f4);
			assert.deepEqual(domForm.toObject('f5'), f5);
			assert.deepEqual(domForm.toObject('f6'), f6);

			dom.byId('f6_checkbox1').checked = false;
			dom.byId('f6_checkbox2').checked = true;
			dom.byId('f6_radio1').checked = true;
			assert.deepEqual(domForm.toObject('f6'), f6_1);

			dom.byId('f6_checkbox1').checked = true;
			assert.deepEqual(domForm.toObject('f6'), f6_2);

			// reset to defaults
			dom.byId('f6_checkbox1').checked = true;
			dom.byId('f6_checkbox2').checked = false;
			dom.byId('f6_radio2').checked = true;
		},

		'.toQuery': function () {
			assert.strictEqual(domForm.toQuery('f1'), f1_query);
			assert.strictEqual(domForm.toQuery('f2'), f2_query);
			assert.strictEqual(domForm.toQuery('f3'), f3_query);
			assert.strictEqual(domForm.toQuery('f4'), f4_query);
			assert.strictEqual(domForm.toQuery('f5'), f5_query);
			assert.strictEqual(domForm.toQuery('f6'), f6_query);
		},

		'.toJson': function () {
			assert.deepEqual(JSON.parse(domForm.toJson('f1')), f1);
			assert.deepEqual(JSON.parse(domForm.toJson('f2')), f2);
			assert.deepEqual(JSON.parse(domForm.toJson('f3')), f3);
			assert.deepEqual(JSON.parse(domForm.toJson('f4')), f4);
			assert.deepEqual(JSON.parse(domForm.toJson('f5')), f5);
			assert.deepEqual(JSON.parse(domForm.toJson('f6')), f6);
		}
	});
});
