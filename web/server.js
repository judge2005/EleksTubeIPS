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

// The Configuration page's backup and restore, roughly as SettingsBackup.cpp does them. Groups here are the
// pages, and values aren't checked. MOCK_AP=1 answers as if over the clock's access point.
var settingsPages = { clock: '1', leds: '2', faces: '3', mqtt: '4', network: '6', weather: '7', matrix: '8' };
var settingsSecrets = ['web_password', 'mqtt_password', 'weather_token'];
var settingsCareful = { hostname: 'hostname', web_user: 'login', web_password: 'login' };

var isSetting = function(key) {
	return !key.startsWith('set_icon_') && key != 'face_files';
}

var findSetting = function(name) {
	for (var group in settingsPages) {
		var page = settingsPages[group];
		if (isSetting(name) && Object.prototype.hasOwnProperty.call(state[page], name)) {
			return page;
		}
	}
	return null;
}

app.get('/settings.json', function (req, res) {
	var file = { file: 'EleksTubeIPS settings', format: 1, firmware: 'mock', clock: state['5'].description, clock_id: 'MOCK01', settings: {} };
	for (var group in settingsPages) {
		var values = file.settings[group] = {};
		var page = state[settingsPages[group]];
		Object.keys(page).filter(isSetting).forEach(function (key) {
			values[key] = settingsSecrets.indexOf(key) >= 0 ? '' : page[key];
		});
	}
	file.wifi = { ssid: state['5'].wifi_ssid, password: '' };
	// Named like the clock names it, with the date and time
	var now = new Date();
	var two = function (n) { return String(n).padStart(2, '0'); };
	var when = now.getFullYear() + '-' + two(now.getMonth() + 1) + '-' + two(now.getDate()) + '-' + two(now.getHours()) + two(now.getMinutes());
	res.set('Content-Disposition', 'attachment; filename="' + state['6'].hostname + '-settings-' + when + '.json"');
	res.type('application/json').send(JSON.stringify(file, null, 2));
});

app.post('/settings.json', express.json({ limit: '8kb' }), function (req, res) {
	var file = req.body;
	var onAP = process.env.MOCK_AP == '1';
	if (!file || !file.settings) {
		return res.json({ error: "That isn't a settings file from the clock. It has no settings in it." });
	}

	var report = { same_clock: file.clock_id == 'MOCK01', on_ap: onAP, warnings: [], changes: [], refused: [], skipped: [], kept: [], blank: [], unchanged: 0 };
	if (file.firmware != 'mock') {
		report.warnings.push('It was saved by firmware ' + file.firmware + ', and this clock has mock.');
	}
	if (!report.same_clock) {
		report.warnings.push("It's from another clock, so it only restores the settings that should differ between clocks if you switch them on below.");
	}

	var seen = {};
	var toChange = [];
	Object.keys(file.settings).forEach(function (group) {
		Object.keys(file.settings[group] || {}).forEach(function (name) {
			var page = findSetting(name);
			var value = file.settings[group][name];
			if (page === null) {
				report.skipped.push({ name: name, why: 'Not a setting on this clock' });
				return;
			}
			seen[name] = true;
			var secret = settingsSecrets.indexOf(name) >= 0;
			if (secret && (value === '' || value === '********')) {
				return;
			}
			if (value === state[page][name]) {
				report.unchanged++;
				return;
			}
			var change = { name: name };
			if (secret) {
				change.secret = true;
			} else {
				change.from = state[page][name];
				change.to = value;
			}
			if (settingsCareful[name]) {
				change.group = settingsCareful[name];
			}
			report.changes.push(change);
			toChange.push({ page: page, name: name, value: value });
		});
	});

	for (var group in settingsPages) {
		Object.keys(state[settingsPages[group]]).filter(isSetting).forEach(function (name) {
			if (!seen[name] && settingsSecrets.indexOf(name) < 0) {
				report.kept.push(name);
			}
		});
	}
	// Not a password for a login or broker that won't be used
	var resulting = function (name) {
		var change = toChange.find(function (change) { return change.name == name; });
		return change ? change.value : state[findSetting(name)][name];
	};
	settingsSecrets.forEach(function (name) {
		if (toChange.some(function (change) { return change.name == name; })) {
			return;
		}
		if ((name == 'web_password' && !resulting('web_user')) || (name == 'mqtt_password' && !resulting('mqtt_host'))) {
			return;
		}
		report.blank.push(name);
	});

	if (file.wifi) {
		report.wifi_ssid = file.wifi.ssid;
		if (!file.wifi.password) {
			if (onAP && file.wifi.ssid) {
				report.blank.push('wifi');
			}
		} else if (!onAP) {
			var current = state['5'].wifi_ssid;
			report.skipped.push({ name: 'wifi', why: file.wifi.ssid == current ?
				'The clock is already on ' + current + ". Wi-Fi is only changed over the clock's access point." :
				"Only changed over the clock's access point, so the clock stays on " + current + '.' });
		} else {
			report.changes.push({ name: 'wifi', to: file.wifi.ssid, secret: true, group: 'wifi' });
		}
	}

	if (req.query.apply && report.changes.length > 0) {
		toChange.forEach(function (change) {
			// A password or key is kept as the placeholder, as the web socket keeps them
			state[change.page][change.name] = settingsSecrets.indexOf(change.name) >= 0 ? '********' : change.value;
		});
		report.restart = true;
	}

	res.json(report);
});

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
			{"8": { "url" : "matrix.html", "title" : "Screen Saver" }},
			{"4": { "url" : "mqtt.html", "title" : "MQTT" }},
			{"6": { "url" : "network.html", "title" : "Network"}},
			{"10": { "url" : "configuration.html", "title" : "Configuration"}},
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

var sendInfoValues = function(conn) {
	var json = '{"type":"sv.init.info","value":';
	json += JSON.stringify(state[5]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendMQTTValues = function(conn) {
	var json = '{"type":"sv.init.mqtt","value":';
	json += JSON.stringify(state[4]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendNetwork = function(conn) {
	var json = '{"type":"sv.init.network","value":';
	json += JSON.stringify(state[6]);
	json += '}';
	console.log(json);
	conn.send(json);
}

var sendMatrix = function(conn) {
	var json = '{"type":"sv.init.matrix","value":';
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
		'dimming': 1,
		'four_digit_display': 2,
		'brightness_config': 200,
		'display_cycle': 30,
		'set_icon_clock': 'Foo'
	},
	"2": {
		'led_pattern': 3,
		'breath_per_min': 7,
		'led_hue': 255,
		'led_saturation': 200,
		'led_value': 210,
		'set_icon_leds': 'Bar'
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
		'file_set':'faces',	// Deliberately out of order
		'set_icon_weather': 'Bletch'
	},
	"4": {
		'mqtt_host' : "",
		'mqtt_port' : 1883,
		'mqtt_user' : "",
		'mqtt_password' : ""
	},
	"5": {
		'description' : "IPSTube Clock With Hardware Dimming, Replacement Firmware",
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
		'hostname' : 'localhost',
		'time_server' : 'pool.ntp.org',
		'web_user' : '',
		'web_password' : ''
	},
	"7": {
		"weather_token":"********",
		"weather_latitude":"21.2",
		"weather_longitude":"-37.1",
		"units":"imperial",
		'weather_hue': 255,
		'weather_saturation': 200,
		'weather_value': 210,
		'set_icon_weather': 'To'
	},
	"8": {
		'screen_saver' : '1',
		'screen_saver_delay' : '53',
		'matrix_hue': 255,
		'matrix_saturation': 200,
		'matrix_value': 210,
		'set_icon_matrix': 'You'
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

var secrets = ['web_password', 'mqtt_password', 'weather_token'];

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

	// Like the clock, only say whether a password or key is set, and ignore the placeholder sent back
	if (secrets.includes(key)) {
		if (value === '********') {
			value = state[screen][key];
		} else {
			value = value !== '' ? '********' : '';
		}
	}

	state[screen][key] = value;

	broadcastUpdate(conn, key, state[screen][key]);
}

var updateHue = function(conn) {
	// var hue = state['2']['led_hue'];
	// hue = (hue + 5) % 256;
	// updateValue(conn, 2, "led_hue:" + hue);

	// var intensity = state['2']['led_value'];
	// intensity = (intensity + 2) % 256;
	// updateValue(conn, 2, "led_value:" + intensity);
	// var val = state['1']['time_or_date'];
	// val = (val + 1) % 3;
	// updateValue(conn, 1, "time_or_date:" + val)
}

wss.on('connection', function(conn) {
	wssConn = conn;

    console.log('connected');
	var hueTimer = setInterval(updateHue, 500, conn);

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
    		sendMQTTValues(conn);
    		break;
    	case 5:
    		sendInfoValues(conn);
    		break;
    	case 6:
    		sendNetwork(conn);
    		break;
    	case 7:
    		sendWeatherValues(conn);
    		break;
    	case 8:
    		sendMatrix(conn);
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

