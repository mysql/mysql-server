This directory in dojox/mvc/tests/controllers is used to show the differences when using different types of MVC Controllers.

step1_test_mvc.html 
	Just uses getStateful(), it does not use a controller.
	The getStateful call will take json data and make it Stateful.
	In this example a transform (transformAddress2Class) is used to hide the row for AddressLine2 if it is blank.
	'dojox/mvc/parserExtension' is required for this since the row being hidden is not a widget.
	The class attribute is bound to AddressLine2 with a direction and a transform.


step2_test_mvc.html 
	Step 2 of tests for Controllers, uses a ModelRefController.
	To move from getStateful to ModelRefController, the only change other than the call to require and call new ModelRefController, 
	is to change the binding on the groups to use ctrl instead of model.
	
	This example also uses the transform (transformAddress2Class) to hide the row for AddressLine2 if it is blank.

	The ModelRefController constructor sets model to the results of the getStateful call which will take json data and make it Stateful


step3_test_mvc.html 
	Step 3 of test for Controllers, uses an EditModelRefController with holdModelUntilCommit: true.
	To move from ModelRefController to use EditModelRefController, need to require EditModelRefController. 
	and on the new call use sourceModel instead of model.
 
	EditModelRefController adds support for reset() and commit(), so buttons were added to show Reset and Save (commit).

	With holdModelUntilCommit set to true, you will want to bind the output address for the group to 
	"target: at(ctrl,'sourceModel')", if holdModelUntilCommit is false you can stay with model, sourceModel will also work.
	You can change holdModelUntilCommit to false to see how that works, updates will be reflected into the Verify section 
	immediately upon focus changes, and Reset will revert back to the last saved model.
	The EditModelRefController constructor sets sourceModel to the results of the getStateful call which will take json data and create make it Stateful


step4_test_mvc.html 
	Step 4 of test for Controllers, uses a StoreRefController.
	To move from ModelRefController to use StoreRefController, need to require StoreRefController, Memory (store), and when 
	Also needed to change the data to be valid for a store query. 
	Setup StoreRefController and use ctrl.queryStore() to setup the models.

	Because of the queryStore call had to move the call to parser.parse inside the when to wait for it. 
	The change in the data for queryStore required changes to the bindings, shad to add a group with target: at('rel:','0')
	for the bindings of fields. 
	
	An exprchar:'%' was used for an mvc/Output just to show how it is used. 


step5_test_mvc.html
	Step 5 of test for Controllers, uses an EditStoreRefController with holdModelUntilCommit: true.
	To move from an EditModelRefController to use an EditStoreRefController, need to require EditStoreRefController, 
	Memory, and when.  Also needed to change the data to be valid for a store query. 

	Because of the queryStore call had to move the call to parser.parse inside the when to wait for it. 
	The change in the data for queryStore required changes to the bindings. had to add a group with target: at('rel:','0') 
	for the bindings of fields.  

	With holdModelUntilCommit set to true, you will want to bind the output address for the group to 
	"target: at(ctrl,'sourceModel')", if holdModelUntilCommit is false you can stay with model, sourceModel will also work.
	You can change holdModelUntilCommit to false to see how that works, updates will be reflected into the Verify section
	immediately upon focus changes, and Reset will revert back to the last saved model.
	The EditStoreRefController constructor sets store to the memory store and uses queryStore() to setup the models.


step6_test_mvc.html
	Step 6 of test for Controllers, uses a ListController.
	To move from ModelRefController to also use ListController, 
	Need to require declare, and ListController, I also needed WidgetList and _InlineTemplateMixin.  
	mvc/parserExtension is still needed to bind to class with data-mvc-bindings for 'AddressLine2'  
	Decided to setup a ctrlclz with ListController to have the functions as part of the controller. 
	The data had to be changed to be array data. 
	The html was updated to use a WidgetList to show a radio button group with the choices, and to use a 
	"cursor" binding for the selected address.  
	Added a transform (transformRadioChecked) to change the cursorIndex when a different AddressName radio button is 
	selected.  Also added an addEmpty function to add a New Address to the list.


step7_test_mvc.html
	Step 7 of test for Controllers, uses an EditModelRefController and ListController.
	To move from ModelRefController to also use EditModelRefController and a ListController, 
	Need to require declare, and EditModelRefController, ListController, 
	I also needed WidgetList and _InlineTemplateMixin.  

	Needed to setup ctrlclz with EditModelRefController and ListController, and the data needs to be array data and the 
	call to new ctrlclz uses sourceModel instead of model, and sets holdModelUntilCommit
	The html was updated to use a WidgetList to show a radio button group with the choices, and to use a 
	"cursor" binding for the selected address.  
	Added a transform (transformRadioChecked) to change the cursorIndex when a different AddressName radio button is 
	selected.  Also added an addEmpty function to add a New Address to the list.


step8_test_mvc.html
	Step 8 of test for Controllers, uses an EditStoreRefListController with holdModelUntilCommit: false
	To move from ModelRefController with EditModelRefController and a ListController to a EditStoreRefListController
	Need to require the EditStoreRefListController, when, and Memory 
	I also needed to change the data to be valid for a store query.   
	Needed to setup ctrlclz with EditStoreRefListController, and use when and ctrl.queryStore() to setup the models. 

	This test uses holdModelUntilCommit: false on the WidgetList, so updates to the address will immediately show-up in 
	the verify section and in the select an address section, but changes can be "Reset" back to their last saved state.


step9_test_mvc.html
	Step 9 of test for Controllers, uses an EditStoreRefListController with holdModelUntilCommit: true
	To move from EditStoreRefListController with holdModelUntilCommit: false to use holdModelUntilCommit: true 
	I had to add the require for dojox/mvc/at and ListController 

	This test uses holdModelUntilCommit: true on the WidgetList, so updates to the address will not show-up in 
	the "Verify" section and in the "Select an address" section until "Save" is pressed.
	But using holdModelUntilCommit: true requires use of a second controller (ctrlSource) which is a ListController 
	and is bound to the sourceModel and cursorIndex of the first controller (ctrl).

	The "Verify" section and the "Select an address" section are bound to at(ctrlSource,'cursor') and at(ctrlSource, 'model') respectively.
	To set the Radio buttons correctly, "Checked" has to be updated in ctrl.model inside the transformRadioChecked. 
