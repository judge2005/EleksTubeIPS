// The web UI's link with the clock, over a web socket: the clock sends the menu and each page's settings, and
// the pages send each setting as it's changed, as "9:<page>:<name>:<value>". Widgets are from ui.js.
'use strict';

var serverSetting = false;	// While the clock's values are being shown, so they aren't sent back
var clockColors = {};	// Each color fieldset's hue, saturation and brightness on the clock, 0-255
var pickerColors = {};	// The same for its color picker, h 0-360, s and v 0-1
var colorPickers = {};	// Each color fieldset's picker
var hourRing = null;	// The Clock page's Display On hours
var pageMap = {};	// Page title to the clock's page number

function byId(id) {
	return document.getElementById(id);
}

function activePage() {
	var page = document.querySelector('.ui-page-active');
	return page ? page.id : undefined;
}

function getPageId(pageName) {
	return pageMap[pageName];
}

// Shows or hides an element, as jQuery's show() and hide() did
function showElement(element, show) {
	if (element) {
		element.style.display = show ? '' : 'none';
	}
}

function checkedValue(name) {
	var checked = document.querySelector('input[name="' + name + '"]:checked');
	return checked ? checked.value : undefined;
}

// Checks the radio button in a group with the value, if there is one
function checkRadio(name, value) {
	var radio = document.querySelector('input[type=radio][name="' + name + '"][value="' + value + '"]');
	if (radio) {
		radio.checked = true;
		UI.refresh(radio);
	}
	return radio;
}

// Shows what the clock sent, without sending it back
function updateElements(values) {
	serverSetting = true;
	try {
		if (values.display_on !== undefined) {
			if (hourRing) {
				hourRing.set(0, values.display_on);
			}
			delete values.display_on;
		}
		if (values.display_off !== undefined) {
			if (hourRing) {
				hourRing.set(1, values.display_off);
			}
			delete values.display_off;
		}

		if (values.face_files !== undefined) {
			var list = byId('face_files');
			var files = values.face_files;
			if (list) {
				list.innerHTML = '';
				Object.keys(files).forEach(function (key) {
					var item = document.createElement('li');
					item.setAttribute('data-theme', 'c');
					item.id = 'face_item_' + key;
					var choose = document.createElement('a');
					choose.href = '#';
					choose.textContent = key;
					choose.onclick = function () { setFace(key); return false; };
					var remove = document.createElement('a');
					remove.href = '#';
					remove.textContent = 'Delete Face';
					remove.onclick = function () { showDeletePopup(files[key]); return false; };
					item.appendChild(choose);
					item.appendChild(remove);
					list.appendChild(item);
				});
				UI.refresh(list);
			}
			delete values.face_files;
		}

		if (values.file_set !== undefined) {
			if (checkRadio('file_set', values.file_set)) {
				delete values.file_set;
			}
		}

		var fileSet = checkedValue('file_set');
		[['clock_face', 'faces'], ['weather_icons', 'weather'], ['slide_show', 'slides']].forEach(function (setting) {
			if (values[setting[0]] !== undefined) {
				if (fileSet == setting[1]) {
					setFace(values[setting[0]]);
				}
				delete values[setting[0]];
			}
		});

		Object.keys(values).forEach(function (key) {
			var value = values[key];
			showElement(byId(key + '_container'), true);

			var element = byId(key);
			var type = element ? element.getAttribute('type') : null;
			if (type == 'checkbox') {
				element.checked = !!value;
				UI.refresh(element);
			} else if (type == 'radio') {
				element.checked = true;
				UI.refresh(element);
			} else if (!type) {
				// A group of radio buttons named after the setting, or else an element showing the value
				var radio = checkRadio(key, value);
				if (radio) {
					radio.dispatchEvent(new Event('change', { bubbles: true }));
				} else if (element) {
					element.innerHTML = value;
				}
			} else if (element.classList.contains('secret')) {
				secretSaved(element, value !== '');
			} else {
				element.value = value;
				if (element.uiSlider) {
					UI.refresh(element);
					colorChange(element);
				} else if (element.tagName == 'SELECT') {
					UI.refresh(element);
					element.dispatchEvent(new Event('change', { bubbles: true }));
				}
			}
		});
	} finally {
		serverSetting = false;
	}
}

function updateStatus(msg) {
	var status = byId('status');
	if (status) {
		status.innerHTML = msg;
	}
}

function url(s) {
	var l = window.location;
	return ((l.protocol === 'https:') ? 'wss://' : 'ws://') + l.host + s;
}

function getElementValue(element) {
	return element.type == 'checkbox' ? element.checked : element.value;
}

function setVisibility(targetName, element, showVals) {
	showElement(byId(targetName), showVals.includes(getElementValue(element)));
}

// Sends a field's value, unless it doesn't match its pattern, which a pop-up says
function elementBlur(element, invalidMsg) {
	if (serverSetting) {
		return;
	}
	if (arguments.length === 2) {
		if (element.validity.patternMismatch) {
			// Back to the field once the pop-up's closed
			byId('invalid_field_text').textContent = invalidMsg;
			UI.popupOpen('invalid_field_popup', element);
			return false;
		}
	}
	var name = element.type == 'radio' ? element.name : element.id;
	safeSend('9:' + getPageId(activePage()) + ':' + name + ':' + getElementValue(element));
}

function elementChange(element) {
	elementBlur(element);
}

function hwb(h, w, b) {
	return 'hwb(' + h + ' ' + w + '% ' + b + '%)';
}

// A color fieldset's sliders show the color on their tracks and handles, and its picker shows it too
function colorChange(element) {
	var fieldset = element.closest('fieldset');
	if (!fieldset) {
		return;
	}
	var id = fieldset.id;
	var clockColor = clockColors[id];
	var pickerColor = pickerColors[id];
	var picker = colorPickers[id];
	var val = Number(getElementValue(element));
	var part = function (name) {
		var input = fieldset.querySelector('.' + name);
		return input ? { track: input.nextElementSibling, handle: input.nextElementSibling.querySelector('.ui-slider-handle') } : null;
	};
	if (!clockColor) {
		return;
	}

	if (element.classList.contains('hue')) {
		var hue = Math.round(val * 360 / 255);
		var hues = part('hue'), saturation = part('saturation'), value = part('value');
		if (saturation) {
			saturation.track.style.backgroundImage = 'linear-gradient(to right, white, ' + hwb(hue, 0, 0) + ')';
			saturation.handle.style.backgroundColor = hwb(hue, Math.round((255 - clockColor.s) * 100 / 255), 0);
		}
		if (value) {
			value.track.style.backgroundImage = 'linear-gradient(to right, black, ' + hwb(hue, 0, 0) + ')';
			value.handle.style.backgroundColor = hwb(hue, 0, Math.round((255 - clockColor.v) * 100 / 255));
		}
		hues.handle.style.backgroundColor = hwb(hue, 0, 0);
		clockColor.h = val;
		pickerColor.h = val * 360 / 255;
	} else if (element.classList.contains('saturation')) {
		clockColor.s = val;
		pickerColor.s = val / 255;
		part('saturation').handle.style.backgroundColor = hwb(Math.round(clockColor.h * 360 / 255), Math.round((255 - val) * 100 / 255), 0);
	} else if (element.classList.contains('value')) {
		clockColor.v = val;
		pickerColor.v = val / 255;
		part('value').handle.style.backgroundColor = hwb(Math.round(clockColor.h * 360 / 255), 0, Math.round((255 - val) * 100 / 255));
	} else {
		return;
	}
	if (picker) {
		picker.set(pickerColor);
	}
}

// The color picker sends the parts of its color that changed
function pickerMove(fieldset, hsv) {
	if (serverSetting) {
		return;
	}
	var clockColor = clockColors[fieldset.id];
	var pickerColor = pickerColors[fieldset.id];
	Object.assign(pickerColor, hsv);
	var pageId = getPageId(activePage());
	[['h', 'hue', Math.round(hsv.h * 255 / 360)], ['s', 'saturation', Math.round(hsv.s * 255)], ['v', 'value', Math.round(hsv.v * 255)]].forEach(function (part) {
		if (part[2] != clockColor[part[0]]) {
			clockColor[part[0]] = part[2];
			var slider = fieldset.querySelector('.' + part[1]);
			if (slider) {
				safeSend('9:' + pageId + ':' + slider.id + ':' + part[2]);
			}
		}
	});
}

function showDeletePopup(filename) {
	byId('delete_face_file').value = filename;
	byId('delete_face_text').textContent = filename;
	UI.popupOpen('delete_face_popup');
}

// Marks the chosen file set, and sends it unless it came from the clock
function setFace(key) {
	document.querySelectorAll('#face_files a').forEach(function (a) {
		a.classList.add('ui-btn-c');
		a.classList.remove('ui-btn-b', 'ui-btn-active', 'item-disabled');
	});
	document.querySelectorAll('#face_item_' + CSS.escape(key) + ' a').forEach(function (a) {
		a.classList.add('item-disabled', 'ui-btn-b', 'ui-btn-active');
		a.classList.remove('ui-btn-c');
	});

	if (serverSetting) {
		return;
	}

	var fileSet = checkedValue('file_set');
	var configName = ':clock_face:';
	if (fileSet == 'weather') {
		configName = ':weather_icons:';
	} else if (fileSet == 'slides') {
		configName = ':slide_show:';
	}
	safeSend('9:' + getPageId(activePage()) + configName + key);
}

function deleteFace() {
	var fileName = byId('delete_face_file').value;
	fetch('/delete_face/' + fileName, { method: 'DELETE', cache: 'no-store' }).then(function (response) {
		if (!response.ok) {
			throw new Error(response.status);
		}
	}).catch(function () {
		UI.loading(false);
		alert('Failed to delete file');
	});
	UI.popupClose('delete_face_popup');
}

function uploadFace() {
	var fileInfo = byId('face_file').files[0];
	if (fileInfo && fileInfo.size > 0) {
		UI.loading(true, 'Uploading face...');
		var formData = new FormData();
		formData.append('file', fileInfo);
		var abort = new AbortController();
		var timer = setTimeout(function () { abort.abort(); }, 60000);
		fetch('/upload_face', { method: 'POST', body: formData, cache: 'no-store', signal: abort.signal }).then(function (response) {
			if (!response.ok) {
				throw new Error(response.status);
			}
			return response.text();
		}).then(function (text) {
			UI.loading(false);
			alert(text);
		}).catch(function () {
			UI.loading(false);
			alert('Failed to upload file');
		}).finally(function () {
			clearTimeout(timer);
		});
	}
	UI.popupClose('upload_face_popup');
}

// The page shown last, kept in the browser. Before, a cookie kept it.
function rememberedPage() {
	try {
		var title = localStorage.getItem('activePageTitle');
		if (title) {
			return title;
		}
	} catch (e) {
	}
	var cookie = document.cookie.match(/(?:^|; )activePageTitle=([^;]*)/);
	return cookie ? decodeURIComponent(cookie[1]) : null;
}

function initializeMenu(values) {
	var list = document.createElement('ul');
	list.id = 'menuList';
	list.setAttribute('data-role', 'listview');
	list.setAttribute('data-inset', 'true');
	pageMap = {};
	var activePageTitle = rememberedPage();
	var activeURL;

	values.forEach(function (obj, i) {
		var key = Object.keys(obj)[0];
		var page = obj[key];
		if (i == 0) {
			activeURL = page.url;
		}
		if (!page.noNav) {
			var item = document.createElement('li');
			var link = document.createElement('a');
			link.setAttribute('data-rel', 'close');
			link.href = page.url;
			link.setAttribute('pageId', page.title);
			link.textContent = page.title;
			item.appendChild(link);
			list.appendChild(item);
		}
		pageMap[page.title] = key;
		if (page.title == activePageTitle) {
			activeURL = page.url;
		}
	});

	byId('menuList').replaceWith(list);
	UI.enhance(byId('mainMenu'));
	if (values.length > 0) {
		UI.changePage(activeURL);
	}
}

function pageRefresh(activePage) {
	if (activePage !== undefined) {
		try {
			localStorage.setItem('activePageTitle', activePage);
		} catch (e) {
		}
		safeSend(getPageId(activePage) + ':');
	}
}

function getMenu() {
	safeSend('0:');
}

var ws;
var timeout = 500;	// ms

function startWebsocket(initialMsg) {
	console.log('start websocket');
	UI.loading(true, 'Trying to connect');

	ws = null;
	ws = new WebSocket(url('/ws'));

	ws.onopen = function (evt) {
		UI.loading(false);
		console.log('web socket opened: ', evt);
		timeout = 500;
		try {
			safeSend(initialMsg);
		} catch (e) {
			(console.error || console.log).call(console, e.stack || e);
		}
	};

	ws.onmessage = function (event) {
		var msg = JSON.parse(event.data);

		switch (msg.type) {
			case 'sv.update':
			case 'sv.init.clock':
			case 'sv.init.leds':
			case 'sv.init.faces':
			case 'sv.init.weather':
			case 'sv.init.matrix':
			case 'sv.init.mqtt':
			case 'sv.init.network':
			case 'sv.init.info':
				console.log(msg.type);
				updateElements(msg.value);
				break;

			case 'sv.status':
				updateStatus(msg.value);
				break;

			case 'sv.init.menu':
				initializeMenu(msg.value);
				break;
		}
	};

	ws.onclose = function (evt) {
		// Connection closed: discard the old websocket and make a new one, waiting longer each time up to 4 s
		console.log('websocket closed, ', evt);

		ws = null;
		setTimeout(startWebsocket, timeout, getPageId(activePage()) + ':');
		if (timeout < 4000) {
			timeout = timeout * 2;
		}
	};

	ws.onerror = function (err) {
		console.log('websocket error: ', err);
	};
}

function checkWebsocket() {
	try {
		if (!(ws.readyState == WebSocket.OPEN || ws.readyState == WebSocket.CONNECTING)) {
			console.log('websocketstate=', ws.readyState);
			ws = null;
			startWebsocket(getPageId(activePage()) + ':');
		}
	} catch (e) {
		console.log('Exception thrown while checking websocket state, ', e.stack || e);
	}
}

function safeSend(msg) {
	console.log(msg);
	try {
		ws.send(msg);
	} catch (e) {
		console.log(e.stack || e);
	}
}

// A new page's color pickers and Display On hours
document.addEventListener('pagecreate', function (event) {
	var page = event.detail;
	page.querySelectorAll('.color_picker').forEach(function (input) {
		var fieldset = input.closest('fieldset');
		clockColors[fieldset.id] = { h: 0, s: 255, v: 255 };
		pickerColors[fieldset.id] = { h: 0, s: 1, v: 1 };
		colorPickers[fieldset.id] = UI.colorPicker(input, function (hsv) {
			pickerMove(fieldset, hsv);
		});
	});

	var ring = page.querySelector('#on_range');
	if (ring) {
		hourRing = UI.hourRing(ring, function (ends) {
			if (serverSetting) {
				return;
			}
			var pageId = getPageId(activePage());
			safeSend('9:' + pageId + ':display_on:' + ends[0]);
			safeSend('9:' + pageId + ':display_off:' + ends[1]);
		});
	}
});

// Once a page is shown, the clock sends its settings, and the menu marks it
document.addEventListener('pagechange', function (event) {
	var current = event.detail.id;
	pageRefresh(current);
	document.querySelectorAll('#menuList a').forEach(function (a) {
		a.classList.toggle('ui-btn-active', a.getAttribute('pageId') == current);
	});
});

document.addEventListener('visibilitychange', function () {
	// When the user comes back to the tab
	if (document.visibilityState == 'visible') {
		checkWebsocket();
	}
});

startWebsocket('0:');
