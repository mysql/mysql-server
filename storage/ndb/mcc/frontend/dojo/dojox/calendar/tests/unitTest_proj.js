define(["doh", "../ColumnView"],
		
	function(doh, ColumnView){
	doh.register("tests.unitTest_proj", [
	
		function test_Projection_0_24(doh){
			
			var o = new ColumnView({				
				startDate:  new Date(2015, 4, 13)
			});
			
			
			var renderData = {
				dateModule: o.dateModule,
				minHours: 0,
				maxHours: 24
			};
			
			var refDate = new Date(2012, 4, 13);
			
			var d = new Date(2012, 4, 13, 0, 0, 0);			
			var p = o.computeProjectionOnDate(renderData, refDate, d, 1000);			
			doh.is(p, 0);
			
			d = new Date(2012, 4, 14, 0, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 1000);
			
			d = new Date(2012, 4, 13, 12, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 500);
			
		},		
		
		function test_Projection_8_18(doh){
			
			var o = new ColumnView({				
				startDate:  new Date(2015, 4, 13)
			});
			
			
			var renderData = {
				dateModule: o.dateModule,
				minHours: 8,
				maxHours: 18
			};
			
			var refDate = new Date(2012, 4, 13);
			
			var d = new Date(2012, 4, 13, 0, 0, 0);			
			var p = o.computeProjectionOnDate(renderData, refDate, d, 1000);			
			doh.is(p, 0);
			
			d = new Date(2012, 4, 13, 8, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 0);
			
			d = new Date(2012, 4, 13, 13, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 500);
			
			d = new Date(2012, 4, 13, 18, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 1000);
			
			d = new Date(2012, 4, 13, 20, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 1000);
			
		},
		
		function test_Projection_12_36(doh){
			
			var o = new ColumnView({				
				startDate:  new Date(2015, 4, 13)
			});
						
			var renderData = {
				dateModule: o.dateModule,
				minHours: 12,
				maxHours: 36
			};
			
			var refDate = new Date(2012, 4, 13);
			
			var d = new Date(2012, 4, 13, 0, 0, 0);			
			var p = o.computeProjectionOnDate(renderData, refDate, d, 1000);			
			doh.is(p, 0);
			
			d = new Date(2012, 4, 13, 12, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 0);
			
			d = new Date(2012, 4, 14, 0, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 500);
			
			d = new Date(2012, 4, 14, 12, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 1000);
			
			d = new Date(2012, 4, 14, 20, 0, 0);
			p = o.computeProjectionOnDate(renderData, refDate, d, 1000);
			doh.is(p, 1000);
			
		}
		
	]);
});
