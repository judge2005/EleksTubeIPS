#!/usr/bin/env node

/*
 * A test server
 */
'use strict';

var expressStaticGzip = require("express-static-gzip");
var express = require('express');
var http = require('http');
var ws = require('ws');
var multiparty = require('multiparty');

var app = new express();

var server = http.createServer(app);

var wss = new ws.Server({ server });

var wssConn;

app.use(function(req, res, next) {
    console.log(req.originalUrl);
    next();
});

app.use(expressStaticGzip("src"));

var uploadHandler = function (req, res) {
	var form = new multiparty.Form();
	
    form.parse(req, function(err, fields, files) {
		var status = 500;
		var msg = 'Upload failed';

		if (files.file[0] != 'undefined') {
			var fileName = files.file[0].originalFilename;
			var key = fileName.substring(0, fileName.indexOf('.'));
			console.log(fileName);
			state['3']['face_files'][key] = fileName;
			setTimeout(function sendUpdate() {
				sendFacesValues(wssConn);
			}, 200);			

			status = 200;
			msg = 'File uploaded';
		}

		res.status(status).send(msg);
    });
}

app.post('/upload_face', uploadHandler);

app.use('/delete_face/', function (req, res) {
	console.log(req.path);
	if (req.method == 'DELETE') {
		if ('/blue_ribbon.tar.gz' == req.path) {
			res.send('File deleted');
			delete state['3']['face_files']['blue_ribbon'];
			setTimeout(function sendUpdate() {
				sendFacesValues(wssConn);
			}, 200);			
		} else {
			res.status(500).send("Delete failed");
		}
	} else {
		res.status(500).send("Unknown request");
	}
});

var pages = {
		"type":"sv.init.menu",
		"value": [
			{"1": { "url" : "clock.html", "title" : "Clock" }},
			{"2": { "url" : "leds.html", "title" : "LEDs" }},
			{"3": { "url" : "faces.html", "title" : "Files" }},
			{"7": { "url" : "weather.html", "title" : "Weather" }},
			{"5": { "url" : "info.html", "title" : "Info" }}
		]
	}


var sendValues = function(conn, screen) {
}

var sendPages = function(conn) {
	var json = JSON.stringify(pages);
	conn.send(json);
	console.log(json);
}

var sendClockValues = function(conn) {
	var json = '{"type":"sv.init.clock","value":';
	json += JSON.stringify(state[1]);
	json += '}';
	console.log(json);
	conn.send(json);

	sendFacesValues(wssConn);
}

var sendLEDValues = function(conn) {
	var json = '{"type":"sv.init.leds","value":';
	json += JSON.stringify(state[2]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendFacesValues = function(conn) {
	var json = '{"type":"sv.init.faces","value":';
	json += JSON.stringify(state[3]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendWeatherValues = function(conn) {
	var json = '{"type":"sv.init.weather","value":';
	json += JSON.stringify(state[7]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendPresetValues = function(conn) {
	var json = '{"type":"sv.init.presets","value":';
	json += JSON.stringify(state[4]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendInfoValues = function(conn) {
	var json = '{"type":"sv.init.info","value":';
	json += JSON.stringify(state[5]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendPresetNames = function(conn) {
	var json = '{"type":"sv.init.preset_names","value":';
	json += JSON.stringify(state[6]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendSync = function(conn) {
	var json = '{"type":"sv.init.sync","value":';
	json += JSON.stringify(state[8]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var state = {
	"1": {
		'time_or_date':  1,
		'date_format':  1,
		'time_format':  true,
		'leading_zero': false,
		'display_on':  10,
		'display_off':  20,
		'time_server':  'http://niobo.us/blah'
	},
	"2": {
		'led_pattern': 3,
		'breath_per_min': 7,
		'led_intensity': 5
	},
	"3": {
		'set_icon_faces': 'Bletch',
		'clock_face': 'divergence',
		'weather_icons': 'yahoo',
		'face_files' : {
			'blue_ribbon': 'blue_ribbon.tar.gz',
			'divergence': 'divergence.tar.gz',
			'dots': 'dots.tar.gz'
		},
		'file_set':'faces'	// Deliberately out of order
	},
	"4": {
		'preset' : 'set3'
	},
	"5": {
		'esp_boot_version' : "1234",
		'esp_free_heap' : "5678",
		'esp_sketch_size' : "90123",
		'esp_sketch_space' : "4567",
		'esp_flash_size' : "8901",
		'esp_chip_id' : "chip id",
		'wifi_ip_address' : "192.168.1.1",
		'wifi_mac_address' : "0E:12:34:56:78",
		'wifi_ssid' : "STC-Wonderful"
	},
	"6": {
		'set1_name' : 'Clock 1',
		'set2_name' : 'Clock 2',
		'set3_name' : 'Clock 3',
		'set4_name' : 'Conditioner',
		'set5_name' : 'Manual'
	},
	"7": {
		"weather_token":"462cf98d57c30f4cc3698a70a63bd3bb",
		"weather_latitude":"21.2",
		"weather_longitude":"-37.1",
		"units":"imperial"
	},
	"8": {
		'sync_port' : '12345',
		'sync_role' : '0',
		'set_icon_sync' : 'burble',
		'wifi_ap' : true
	}
}

var broadcastUpdate = function(conn, field, value) {
	var json = '{"type":"sv.update","value":{' + '"' + field + '":' + JSON.stringify(value) + '}}';
	console.log(json);
	try {
		conn.send(json);
	} catch (e) {
		console.log(e);
	}
}

var updateValue = function(conn, screen, pair) {
	console.log(screen);
	console.log(pair);
	var index = pair.indexOf(':');

	var key = pair.substring(0, index);
	var value = pair.substring(index+1);
	try {
		value = JSON.parse(value);
	} catch (e) {

	}

	if (screen == 4 || screen == 6) {
		state[screen][key] = '"' + value + '"';
	} else {
		state[screen][key] = value;
	}
	broadcastUpdate(conn, key, state[screen][key]);
}

var updateHue = function(conn) {
	// var hue = state['2']['hue'];
	// hue = (hue + 1) % 256;
	// updateValue(conn, 2, "hue:" + hue);
	var val = state['1']['time_or_date'];
	val = (val + 1) % 3;
	updateValue(conn, 1, "time_or_date:" + val)
}

wss.on('connection', function(conn) {
	wssConn = conn;

    console.log('connected');
	var hueTimer = setInterval(updateHue, 5000, conn);

    //connection is up, let's add a simple simple event
	conn.on('message', function(data, isBinary) {

        //log the received message and send it back to the client
        console.log('received: %s', data);
        var message = isBinary ? data : data.toString();
    	var code = parseInt(message.substring(0, message.indexOf(':')));

    	switch (code) {
    	case 0:
    		sendPages(conn);
    		break;
    	case 1:
    		sendClockValues(conn);
    		break;
    	case 2:
    		sendLEDValues(conn);
    		break;
    	case 3:
    		sendFacesValues(conn);
    		break;
    	case 4:
    		sendPresetValues(conn);
    		break;
    	case 5:
    		sendInfoValues(conn);
    		break;
    	case 6:
    		sendPresetNames(conn);
    		break;
    	case 7:
    		sendWeatherValues(conn);
    		break;
    	case 8:
    		sendSync(conn);
    		break;
    	case 9:
    		message = message.substring(message.indexOf(':')+1);
    		var screen = message.substring(0, message.indexOf(':'));
    		var pair = message.substring(message.indexOf(':')+1);
    		updateValue(conn, screen, pair);
    		break;
    	}
    });

	conn.on('close', function() {
		clearInterval(hueTimer);
	});
});

//start our server
server.listen(process.env.PORT || 8080, function() {
    console.log('Server started on port' + server.address().port + ':)');
});

