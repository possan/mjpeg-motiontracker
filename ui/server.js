var zmq = require('zmq'),
	sock = zmq.socket('req'),
	express = require('express'),
	fs = require('fs'),
	util=require('util'),
	exec=require('child_process').exec;

var configfile = '../konsert.config';
var snapshotfile = '../snapshot.jpg';
var statusfile = '../stats.json';
var last_stat = '[]';

sock.connect('tcp://127.0.0.1:9999');

sock.on('message', function(data) {
	data = data.toString();
    console.log('response from tracker:', data);
    if (data.substring(0, 7) == 'status:') {
    	console.log('got status', data.substring(7));
    	last_stat = data.substring(7);
    }
});

/* setInterval(function() {
	console.log('Sending ping request..');
	sock.send('ping');
}, 10000);

setInterval(function() {
	console.log('Sending stat request..');
	sock.send('stat');
}, 1000); */

var app = express();

app.use(express.static(__dirname + '/public'));
app.use(express.bodyParser({}));

app.post('/config.txt', function(req, res) {
	console.log(req.body);
	if (req.body.content) {
		console.log('Saving configuration..');
		fs.writeFileSync(configfile, req.body.content, 'UTF-8');
	}
	res.send('ok');
});

app.get('/config.txt', function(req, res) {
	var content = fs.readFileSync(configfile, 'UTF-8');
	res.send(content);
});

app.post('/restart', function(req, res) {
	console.log('Sending restart request..');
	res.send('ok');
	// sock.send('restart');
	exec('killall mmnew',function(err,stdout,stderr) {
		console.log(err, stdout, stderr);

		process.exit(1);
	});
});

app.post('/trigger/:index', function(req, res) {
	console.log('Sending trigger ' + req.params.index + ' request..');
	sock.send('trigger,'+req.params.index);
	res.send('ok');
});

app.post('/snapshot', function(req, res) {
	console.log('Sending snapshot request..');
	sock.send('snapshot');
	res.send('ok');
});

app.get('/preview.jpg', function(req, res) {
	res.sendfile(snapshotfile, {'root': '.'});
});

app.get('/stat.json', function(req, res) {
	res.sendfile(statusfile, {'root': '.'});
});

app.listen(8000);
