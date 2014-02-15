$(function() {

	var save = function(e) {
		$.ajax({
			method: 'POST',
			url: '/config.txt',
			data: {
				content: $('textarea#config').val()
			},
			dataType: 'plaintext',
			success: function(r) {
				console.log('Save response:', r);
				reloadareas();
			}
		});
		if (e) {
			e.preventDefault();
		}
	};

	$('button#save').click(save);

	var revert = function(e) {
		$.ajax({
			method: 'GET',
			url: '/config.txt',
			type: 'plaintext',
			success: function(r) {
				console.log('Get config response:', r);
				$('textarea#config').val(r);
				reloadareas();
			}
		});
		if (e) {
			e.preventDefault();
		}
	};

	$('button#revert').click(revert);

	$('button#restart').click(function(e) {
		$.ajax({
			method: 'POST',
			url: '/restart',
			dataType: 'plaintext',
			success: function(r) {
				console.log('Restart response:', r);
			}
		});
		if (e) {
			e.preventDefault();
		}
	});

	var snapshot = function() {
		$.ajax({
			method: 'POST',
			url: '/snapshot',
			dataType: 'plaintext',
			success: function(r) {
				console.log('Snapshot response:', r);
			}
		});
	};

	var tick = 0;

	function reloadpreview() {
		if ($('input#refresh').is(':checked')) {
			switch(tick % 5) {
				case 0:
					snapshot();
					break;
				default:
					var url = '/preview.jpg?r='+Math.random();
					var i = new Image();
					i.src = url;
					i.onload = function() {
						$('div.preview img#preview').attr('src', url);
					};
					break;
			}
		}
		tick ++;
		setTimeout(reloadpreview, 500);
	}

	revert();

	reloadpreview();

	var mousestate = 0;
	var dragel = undefined;
	var grabx = 0;
	var graby = 0;
	var startx = 0;
	var starty = 0;
	var lastid = '';

	function reloadareas() {
		var parent = $('div.areas');
		parent.html('');
		var config = $('textarea#config').val();
		var lines = config.split('\n');
		console.log(lines);
		lines.forEach(function(line) {
			var args = line.split(' ');
			if (args.length > 6 && args[0] == 'area') {
				console.log('parse area', args);
				var ar = $('<div class=\"area\" />');
				ar.css({
					left: args[1] + 'px',
					top: args[2] + 'px',
					width: args[3] + 'px',
					height: args[4] + 'px'
				});
				ar.text(args[7]);
				parent.append(ar);
			}
		});
	}

	$('div.preview').on('mousedown', function(e) {
		console.log('mousedown', e.offsetX, e.offsetY, e);
		e.stopPropagation();
		e.preventDefault();
		mousestate = 1;
		dragel = -1;
		startx = e.offsetX;
		starty = e.offsetY;
		lastid = ''
	});

	$('div.preview').on('mousemove', function(e) {
		// console.log('mousemove', e.offsetX, e.offsetY, e);
		e.stopPropagation();
		e.preventDefault();
	});

	$('div.preview').on('mouseup', function(e) {
		console.log('mouseup', e.offsetX, e.offsetY, e);
		e.stopPropagation();
		e.preventDefault();
		mousestate = 0;
		dragel = undefined;
		// rebuild config and save
		var w = e.offsetX - startx;
		var h = e.offsetY - starty;
		if (w > 3 && h > 3) {
			var config = $('textarea#config').val();
			config += 'area '+startx+' '+starty+' '+w+' '+h+' 5 100 /motion/1\n';
			$('textarea#config').val(config);
			reloadareas();
		}
	});

	$('textarea#config').on('blur', reloadareas);
	$('textarea#config').on('keyup', reloadareas);

});
