  define ("dojox/charting/bidi/_bidiutils", {
		reverseMatrix: function(plot, dim, offsets, rtl){
			//summary:
			//	reverse the underlying matrix of the plots to perform the mirroring behavior.
			//plot:
			//  the plot which has the matrix to be reversed.
			//dim:
			//  the dimension (width,height) of the chart.
			//offsets:
			//  the offsets of the chart
			var shift = offsets.l - offsets.r;
			var xx = rtl? -1 : 1;
			var xy = 0;
			var yx = 0;
			var yy = 1;
			var dx = rtl? dim.width + shift : 0;
			var dy = 0;
			if(plot.matrix){
				xx = xx * Math.abs(plot.matrix.xx);
				yy = plot.matrix.yy;
				xy = plot.matrix.xy;
				yx = plot.matrix.yx;
				dy = plot.matrix.xy;
			}
			plot.setTransform({xx: xx, xy: xy, yx: yx, yy: yy, dx: dx, dy: dy});
 	}
 });
