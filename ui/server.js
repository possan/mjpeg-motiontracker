var zmq = require('zmq'),
	sock = zmq.socket('req'),
	express = require('express'),
	fs = require('fs');

var configfile = '../konsert.config';
var snapshotfile = '../snapshot.jpg';

sock.connect('tcp://127.0.0.1:9999');

sock.on('message', function(data) {
    console.log('answer: ' + data);
});

setInterval(function() {
	console.log('sending request.');
	sock.send('ping');
}, 10000);

var app = express();

app.use(express.static(__dirname + '/public'));
app.use(express.bodyParser({}));

app.post('/config.txt', function(req, res) {
	console.log(req.body);
	if (req.body.content) {
		fs.writeFileSync(configfile, req.body.content, 'UTF-8');
	}
	res.send('ok');
});

app.get('/config.txt', function(req, res) {
	var content = fs.readFileSync(configfile, 'UTF-8');
	res.send(content);
});

app.post('/restart', function(req, res) {
	sock.send('restart');
	res.send('ok');
});

app.post('/snapshot', function(req, res) {
	sock.send('snapshot');
	res.send('ok');
});

app.get('/preview.jpg', function(req, res) {
	res.sendfile(snapshotfile, {'root': '.'});
});

app.listen(8000);
