if (process.argv.length < 3) {
	console.log('Syntax: node server.js [configfile] [snapshotfile] [statsfile]');
	process.exit(1);
}

var express = require('express'),
	fs = require('fs'),
	util = require('util'),
	exec = require('child_process').exec;

var configfile = process.argv[2]; // '../demo.config';
var snapshotfile = process.argv[3]; // '/var/tmp/snapshot.jpg';
var statusfile = process.argv[4]; // '/var/tmp/stats.json';

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
	var content = fs.readFileSync(configfile);
	res.setHeader('Cache-Control', 'public, max-age=0');
	res.setHeader('Content-Type', 'text/plain');
	res.setHeader('Content-Length', content.length);
	res.setHeader("Connection", "close");
	res.send(content);
	res.end();
});

app.post('/restart', function(req, res) {
	console.log('Sending restart request..');
	res.send('ok');
	exec('killall mmnew',function(err,stdout,stderr) {
		console.log(err, stdout, stderr);
	});
});

app.get('/preview.jpg', function(req, res) {
	var content = fs.readFileSync(snapshotfile);
	res.setHeader('Cache-Control', 'public, max-age=0');
	res.setHeader('Content-Type', 'image/jpeg');
	res.setHeader('Content-Length', content.length);
	res.setHeader("Connection", "close");
	res.send(content);
	res.end();
});

app.get('/stat.json', function(req, res) {
	var content = fs.readFileSync(statusfile);
	res.setHeader('Cache-Control', 'public, max-age=0');
	res.setHeader('Content-Type', 'application/json');
	res.setHeader("Connection", "close");
	res.send(content);
	res.end();
});

app.listen(8000);
