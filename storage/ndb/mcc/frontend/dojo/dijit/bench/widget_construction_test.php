<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
        "http://www.w3.org/TR/html4/strict.dtd">

<html>
	<head>
		<title>test of various synchronous page searching methods</title>
		<style type="text/css">
			@import "../themes/claro/document.css";
			@import "../themes/tundra/tundra.css";
		</style>
		<script type="text/javascript" src="../../dojo/dojo.js"
			data-dojo-config="parseOnLoad: true, isDebug: true"></script>
		<script type="text/javascript">
			dojo.require("dojo.parser");	// scan page for widgets and instantiate them
			dojo.require("dijit._WidgetBase");
			dojo.require("dijit._TemplatedMixin");

			/* dummy widget for benchmarking purposes */
			dojo.declare(
				"SimpleButton",
				[ dijit._WidgetBase, dijit._TemplatedMixin ],
				{
					label: "",

					templateString: "<button data-dojo-attach-event='onclick:onClick'>${label}</button>",

					onClick: function(){
						this.domNode.style.backgroundColor="green";
					},
					postCreate: function(){
					}
				}
			);
		</script>
	</head>
	<body>
		<h1 style="font-size: 40px; line-height: 50px;">This page contains a huge number of nodes, most of which are "chaff".</h1>
		<h3>Here's the relative timings for this page</h3>
		<div id="profileOutputTable"></div>
		<!--
		<h3>And some comparison data</h3>
		<table border=1>
		<thead>
			<tr>
				<th>IE
				<th>Safari
				<th>Gecko (on PC)
				<th>Gecko (on intel mac)
			</tr>
		</thead>
		<tbody>
			<tr>
				<td>4890
				<td>3242
				<td>3094
				<td>3782
			</tr>
		</tbody>
		</table>
		-->


<?
	$containerDepth = 30;
	$leadingChaff = 100;
	$trailingChaff = 100;
	$items = 100;
?>
<?
	function generateChaff($iters){
		for($i=0;$i<$iters;$i++){ ?>
			<pre class="highlighted"><code><span class="hl-reserved">var </span><span class="hl-identifier">dlg</span><span class="hl-default"> = </span><span class="hl-reserved">new </span><span class="hl-identifier">blah</span><span class="hl-default">.</span><span class="hl-identifier">ext</span><span class="hl-default">.</span><span class="hl-identifier">LayoutDialog</span><span class="hl-brackets">(</span><span class="hl-identifier">config</span><span class="hl-code">.</span><span class="hl-identifier">id</span><span class="hl-code"> || </span><span class="hl-identifier">blah</span><span class="hl-code">.</span><span class="hl-identifier">util</span><span class="hl-code">.</span><span class="hl-identifier">Dom</span><span class="hl-code">.</span><span class="hl-identifier">generateId</span><span class="hl-brackets">()</span><span class="hl-code">, </span><span class="hl-brackets">{
				</span><span title="autoCreate" class="hl-identifier">autoCreate</span><span class="hl-code"> : </span><span class="hl-reserved">true</span><span class="hl-code">,
				</span><span title="minWidth" class="hl-identifier">minWidth</span><span class="hl-code">:</span><span class="hl-number">400</span><span class="hl-code">,
				</span><span title="minHeight" class="hl-identifier">minHeight</span><span class="hl-code">:</span><span class="hl-number">300</span><span class="hl-code">,
				</span>
				<span title="syncHeightBeforeShow" class="hl-identifier">syncHeightBeforeShow</span><span class="hl-code">: </span><span class="hl-reserved">true</span><span class="hl-code">,
				</span><span title="shadow" class="hl-identifier">shadow</span><span class="hl-code">:</span><span class="hl-reserved">true</span><span class="hl-code">,
				</span><span title="fixedcenter" class="hl-identifier">fixedcenter</span><span class="hl-code">: </span><span class="hl-reserved">true</span><span class="hl-code">,
				</span><span title="center" class="hl-identifier">center</span><span class="hl-code">:</span><span class="hl-brackets">{</span><span class="hl-identifier">autoScroll</span><span class="hl-code">:</span><span class="hl-reserved">false</span><span class="hl-brackets">}</span><span class="hl-code">,
				</span><span title="east"  class="hl-identifier">east</span><span class="hl-code">:</span><span class="hl-brackets">{</span><span class="hl-identifier">split</span><span class="hl-code">:</span><span class="hl-reserved">true</span><span class="hl-code">,</span><span class="hl-identifier">initialSize</span><span class="hl-code">:</span><span class="hl-number">150</span><span class="hl-code">,</span><span class="hl-identifier">minSize</span><span class="hl-code">:</span><span class="hl-number">150</span><span class="hl-code">,</span><span class="hl-identifier">maxSize</span><span class="hl-code">:</span><span class="hl-number">250</span><span class="hl-brackets">}
			})</span><span class="hl-default">;
			</span><span class="hl-identifier">dlg</span><span class="hl-default">.</span><span class="hl-identifier">setTitle</span><span class="hl-brackets">(</span><span class="hl-quotes">'</span><span class="hl-string">Choose an Image</span><span class="hl-quotes">'</span><span class="hl-brackets">)</span><span class="hl-default">;
			</span><span class="hl-identifier">dlg</span><span class="hl-default">.</span><span class="hl-identifier">getEl</span><span class="hl-brackets">()</span><span class="hl-default">.</span><span class="hl-identifier">addClass</span><span class="hl-brackets">(</span><span class="hl-quotes">'</span><span class="hl-string">ychooser-dlg</span><span class="hl-quotes">'</span><span class="hl-brackets">)</span><span class="hl-default">;</span></code></pre><br />
			<pre class="highlighted"><code><span class="hl-reserved">var </span><span class="hl-identifier">animated</span><span class="hl-default"> = </span><span class="hl-reserved">new </span><span class="hl-identifier">blah</span><span class="hl-default">.</span><span class="hl-identifier">ext</span><span class="hl-default">.</span><span class="hl-identifier">Resizable</span><span class="hl-brackets">(</span><span class="hl-quotes">'</span><span class="hl-string">animated</span><span class="hl-quotes">'</span><span class="hl-code">, </span><span class="hl-brackets">{
			    </span><span title="east" class="hl-identifier">width</span><span class="hl-code">: </span><span class="hl-number">200</span><span class="hl-code">,
			    </span><span title="east" class="hl-identifier">height</span><span class="hl-code">: </span><span class="hl-number">100</span><span class="hl-code">,
			    </span><span title="east" class="hl-identifier">minWidth</span><span class="hl-code">:</span><span class="hl-number">100</span><span class="hl-code">,
			    </span><span class="hl-identifier">minHeight</span><span class="hl-code">:</span><span class="hl-number">50</span><span class="hl-code">,
			    </span><span class="hl-identifier">animate</span><span class="hl-code">:</span><span class="hl-reserved">true</span><span class="hl-code">,
			    </span><span class="hl-identifier">easing</span><span class="hl-code">: </span><span class="hl-identifier">YAHOO</span><span class="hl-code">.</span><span class="hl-identifier">util</span><span class="hl-code">.</span><span class="hl-identifier">Easing</span><span class="hl-code">.</span><span class="hl-identifier">backIn</span><span class="hl-code">,
			    </span><span class="hl-identifier">duration</span><span class="hl-code">:</span><span class="hl-number">.6
			</span><span class="hl-brackets">})</span><span class="hl-default">;</span></code></pre>
			<h4>The standard Lorem Ipsum passage, used since the 1500s</h4>
			<p>
			"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
			eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
			ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
			aliquip ex ea commodo consequat. Duis aute irure dolor in
			reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
			pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
			culpa qui officia deserunt mollit anim id est laborum."
			</p>

			<h4>Section 1.10.32 of "de Finibus Bonorum et Malorum", written by Cicero in 45 BC</h4>

			<p>
			"Sed ut perspiciatis unde omnis iste natus error sit voluptatem
			accusantium doloremque laudantium, totam rem aperiam, eaque ipsa
			quae ab illo inventore veritatis et quasi architecto beatae vitae
			dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit
			aspernatur aut odit aut fugit, sed quia consequuntur magni dolores
			eos qui ratione voluptatem sequi nesciunt. Neque porro quisquam
			est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci
			velit, sed quia non numquam eius modi tempora incidunt ut labore et
			dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam,
			quis nostrum exercitationem ullam corporis suscipit laboriosam,
			nisi ut aliquid ex ea commodi consequatur? Quis autem vel eum iure
			reprehenderit qui in ea voluptate velit esse quam nihil molestiae
			consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla
			pariatur?"
			</p>

			<h4>1914 translation by H. Rackham</h4>

			<p>
			"But I must explain to you how all this mistaken idea of denouncing
			pleasure and praising pain was born and I will give you a complete
			account of the system, and expound the actual teachings of the
			great explorer of the truth, the master-builder of human happiness.
			No one rejects, dislikes, or avoids pleasure itself, because it is
			pleasure, but because those who do not know how to pursue pleasure
			rationally encounter consequences that are extremely painful. Nor
			again is there anyone who loves or pursues or desires to obtain
			pain of itself, because it is pain, but because occasionally
			circumstances occur in which toil and pain can procure him some
			great pleasure. To take a trivial example, which of us ever
			undertakes laborious physical exercise, except to obtain some
			advantage from it? But who has any right to find fault with a man
			who chooses to enjoy a pleasure that has no annoying consequences,
			or one who avoids a pain that produces no resultant pleasure?"
			</p>
		<? }
	} // end generateChaff
	$widgetName = "SimpleButton";
?>
<? generateChaff($leadingChaff); ?>
<hr>
<? for($i=0;$i<$containerDepth;$i++){ ?>
	<table border="1" cellpadding="0" cellspacing="0" width="100%">
	<!--
	<table>
	-->
		<tr>
			<td>
			<br>
			chaff!
			<br>
<? } ?>
<? for($i=0;$i<$items;$i++){ ?>
			<div data-dojo-type="<?= $widgetName ?>" label="item2 <?= $i ?>">item2 <?= $i ?></div>
<? } ?>
<? for($i=0;$i<$containerDepth;$i++){ ?>
			</td>
		</tr>
	</table>
<? } ?>
<? generateChaff($trailingChaff);  ?>
<? for($i=0;$i<$items;$i++){ ?>
	<div data-dojo-type="<?= $widgetName ?>" label="item2 <?= $i ?>"><span>item <?= $i ?></span></div>
<? } ?>

<script type="text/javascript">

		oldTime = new Date();
		dojo.ready(function(){
			var time = new Date().getTime() - oldTime;
			var p = document.createElement("p");
			alert("Widgets loaded in " + time + "ms");
		});

</script>

	</body>
</html>
