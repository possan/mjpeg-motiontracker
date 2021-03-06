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
			var url = '/preview.jpg';//?x='+Math.random();
			// var i = new Image();
			// i.src = url;
			// i.onload = function() {
			$('div.preview img#preview').attr('src', url);
			// };
		}
		tick ++;
	}

	revert();

	setInterval(reloadpreview, 500);

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
		var idx = 0;
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
				// ar.text(args[7]);
				ar.attr('id', 'area'+idx);
				ar.html('<i>'+ idx + '</i><b>0</b><span>...</span>');
				parent.append(ar);
				idx ++;
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
			config += '\narea '+startx+' '+starty+' '+w+' '+h+' 100 5000 /motion/1\n';
			$('textarea#config').val(config);
			reloadareas();
		}
	});

	$('textarea#config').on('blur', reloadareas);
	$('textarea#config').on('keyup', reloadareas);

	function reloadstat() {
		$.ajax({
			method: 'GET',
			url: '/stat.json',
			dataType: 'json',
			success: function(r) {
				console.log('Stat.json response:', r);
				$('pre#stat').text(JSON.stringify(r, null, 2));
				r.forEach(function(item, idx) {
					$('div#area'+idx+' b').text(item.percent);
					if (item.percent > 0) {
						$('div#area'+idx).addClass('highlight');
					} else {
						$('div#area'+idx).removeClass('highlight');
					}
				})
			}
		});
	}

	setInterval(reloadstat, 500);

});
