// The Configuration page's restore: the clock checks a settings file and says what it would change, the page
// shows that and asks for anything the file left empty, then sends the file again to be restored. See
// SettingsBackup.cpp. The build inlines this into configuration.html. Widgets are from ui.js.

var settingsFile = null;	// The file being restored
var settingsReport = null;	// What the clock said about it

// The settings a restore asks about, see carefulSettings in SettingsBackup.cpp
var settingsGroups = [
	{ group: 'hostname', label: 'Hostname' },
	{ group: 'address', label: 'Fixed IP address' },
	{ group: 'login', label: 'Web UI login' },
	{ group: 'wifi', label: 'Wi-Fi network' }
];

// What a backup leaves empty. fileGroup is where one typed in goes in the file, though the clock finds it anywhere.
var settingsSecrets = {
	web_password: { label: 'Web UI password', group: 'login', fileGroup: 'web_login' },
	mqtt_password: { label: 'MQTT password', fileGroup: 'mqtt' },
	weather_token: { label: 'Weather API key', fileGroup: 'weather' },
	wifi: { label: 'Password', group: 'wifi' }
};

function settingsElement(tag, className, text) {
	var element = document.createElement(tag);
	if (className) {
		element.className = className;
	}
	if (text !== undefined) {
		element.textContent = text;
	}
	return element;
}

function settingsShowValue(value) {
	if (value === '') {
		return '(empty)';
	}
	return typeof value == 'string' ? '"' + value + '"' : String(value);
}

function settingsReset() {
	settingsFile = null;
	settingsReport = null;
	var input = document.getElementById('settings_file');
	if (input) {
		input.value = '';
	}
	var report = document.getElementById('settings_report');
	if (report) {
		report.innerHTML = '';
	}
}

function settingsShowMessage(text, className) {
	var report = document.getElementById('settings_report');
	report.innerHTML = '';
	report.appendChild(settingsElement('p', className || 'settings-message', text));
	return report;
}

function settingsSend(file, apply) {
	return fetch('/settings.json' + (apply ? '?apply=1' : ''), {
		method: 'POST',
		headers: { 'Content-Type': 'application/json' },
		body: JSON.stringify(file),
		cache: 'no-store'
	}).then(function (response) {
		if (!response.ok) {
			throw new Error('The clock answered ' + response.status);
		}
		return response.json();
	});
}

function settingsFileChosen(input) {
	var chosen = input.files && input.files[0];
	if (!chosen) {
		return;
	}

	var reader = new FileReader();
	reader.onload = function () {
		var file;
		try {
			file = JSON.parse(reader.result);
		} catch (e) {
			settingsShowMessage("That isn't a settings file. It isn't JSON: " + e.message, 'settings-error');
			return;
		}
		if (file === null || typeof file != 'object' || Array.isArray(file)) {
			settingsShowMessage("That isn't a settings file from the clock.", 'settings-error');
			return;
		}

		settingsShowMessage('Checking it...');
		settingsSend(file, false).then(function (report) {
			settingsFile = file;
			settingsReport = report;
			settingsShowReport(report);
		}).catch(function (e) {
			settingsShowMessage("Couldn't check it: " + e.message, 'settings-error');
		});
	};
	reader.onerror = function () {
		settingsShowMessage("Couldn't read that file.", 'settings-error');
	};
	reader.readAsText(chosen);
}

function settingsChangeRow(change) {
	var row = settingsElement('div', 'settings-change');
	if (change.name == 'wifi') {
		row.appendChild(settingsElement('span', 'settings-name', 'Joins ' + change.to + ', with the password from the file'));
		return row;
	}

	row.appendChild(settingsElement('code', 'settings-name', change.name));
	if (change.secret) {
		row.appendChild(settingsElement('span', 'settings-value', 'set from the file'));
	} else {
		row.appendChild(settingsElement('span', 'settings-value', settingsShowValue(change.from) + ' → ' + settingsShowValue(change.to)));
	}
	return row;
}

// A field for a password or key the file left empty. Typing in one switches its group on.
function settingsSecretField(name, report) {
	var secret = settingsSecrets[name];
	var label = secret.label;
	if (name == 'wifi' && report.wifi_ssid) {
		label += ' for ' + report.wifi_ssid;
	}

	var field = settingsElement('div', 'ui-field-contain settings-secret');
	var id = 'settings_secret_' + name;
	var labelElement = settingsElement('label', null, label);
	labelElement.setAttribute('for', id);
	field.appendChild(labelElement);
	var input = settingsElement('input', 'secret');	// For secret.js's show button
	input.type = 'password';
	input.id = id;
	input.setAttribute('data-secret', name);
	input.setAttribute('autocomplete', 'new-password');	// Not the browser's saved login for the clock
	input.setAttribute('data-mini', 'true');
	input.oninput = function () {
		if (secret.group && input.value.length > 0) {
			var box = document.getElementById('settings_group_' + secret.group);
			if (box && !box.checked) {
				box.checked = true;
				UI.refresh(box);
			}
		}
	};
	field.appendChild(input);
	return field;
}

function settingsList(title, entries, className) {
	var section = settingsElement('div', 'settings-list ' + (className || ''));
	section.appendChild(settingsElement('p', 'settings-subhead', title));
	entries.forEach(function (entry) {
		var row = settingsElement('div', 'settings-change');
		row.appendChild(settingsElement('code', 'settings-name', entry.name));
		if (entry.why) {
			row.appendChild(settingsElement('span', 'settings-value', entry.why));
		}
		section.appendChild(row);
	});
	return section;
}

function settingsCount(n, one, many) {
	return n + ' ' + (n == 1 ? one : many);
}

// What the clock found in the file. A password or key in it is always restored, as the clock doesn't say
// whether it matches the one it has.
function settingsSummary(report) {
	var changes = report.changes || [];
	var secrets = changes.filter(function (change) { return change.secret; }).length;
	var settings = changes.length - secrets;
	var same = report.unchanged || 0;

	if (changes.length == 0) {
		if (same == 0) {
			return "There's nothing in this file the clock can restore.";
		}
		return 'Everything in this file matches the clock: ' + (same == 1 ? 'its 1 setting.' : 'all ' + same + ' settings.');
	}

	var text;
	var keys = settingsCount(secrets, 'password or key', 'passwords or keys');
	if (settings > 0) {
		text = settingsCount(settings, 'setting differs', 'settings differ') + " from the clock's" +
			(secrets > 0 ? ', and the file has ' + keys + ' to restore.' : '.');
	} else {
		text = 'The file has ' + keys + ' to restore.';
	}
	if (same > 0) {
		text += same == 1 ? ' The other setting matches the clock.' : ' The other ' + same + ' settings match the clock.';
	}
	return text;
}

function settingsShowReport(report) {
	var box = document.getElementById('settings_report');
	box.innerHTML = '';

	if (report.error) {
		box.appendChild(settingsElement('p', 'settings-error', report.error));
		return;
	}

	if (report.warnings && report.warnings.length > 0) {
		var warnings = settingsElement('ul', 'settings-warnings');
		report.warnings.forEach(function (warning) {
			warnings.appendChild(settingsElement('li', null, warning));
		});
		box.appendChild(warnings);
	}

	var changes = report.changes || [];
	var blank = report.blank || [];
	var plain = changes.filter(function (change) { return !change.group; });
	box.appendChild(settingsElement('p', 'settings-summary', settingsSummary(report)));

	if (plain.length > 0) {
		var list = settingsElement('div', 'settings-list');
		plain.forEach(function (change) {
			list.appendChild(settingsChangeRow(change));
		});
		box.appendChild(list);
	}

	// The settings to ask about, switched on if the file is from this clock, or for the Wi-Fi network the file has a password for
	var asked = false;
	settingsGroups.forEach(function (entry) {
		var inGroup = changes.filter(function (change) { return change.group == entry.group; });
		var fields = blank.filter(function (name) { return settingsSecrets[name] && settingsSecrets[name].group == entry.group; });
		if (inGroup.length == 0 && fields.length == 0) {
			return;
		}

		if (!asked) {
			box.appendChild(settingsElement('p', 'settings-subhead settings-asked', 'Restore these too?'));
			asked = true;
		}
		var group = settingsElement('div', 'settings-group');
		var head = settingsElement('div', 'settings-group-head');
		var id = 'settings_group_' + entry.group;
		var label = settingsElement('label', null, entry.label);
		label.setAttribute('for', id);
		var toggle = settingsElement('input');
		toggle.type = 'checkbox';
		toggle.id = id;
		toggle.checked = report.same_clock || (entry.group == 'wifi' && inGroup.length > 0);
		toggle.setAttribute('data-role', 'flipswitch');
		toggle.setAttribute('data-on-text', 'Yes');
		toggle.setAttribute('data-off-text', 'No');
		toggle.setAttribute('data-wrapper-class', 'custom-label-flipswitch');
		head.appendChild(label);
		head.appendChild(toggle);
		group.appendChild(head);
		inGroup.forEach(function (change) {
			group.appendChild(settingsChangeRow(change));
		});
		fields.forEach(function (name) {
			group.appendChild(settingsSecretField(name, report));
		});
		box.appendChild(group);
	});

	var otherFields = blank.filter(function (name) { return settingsSecrets[name] && !settingsSecrets[name].group; });
	if (otherFields.length > 0) {
		var secrets = settingsElement('div', 'settings-group');
		secrets.appendChild(settingsElement('p', 'settings-subhead', 'Passwords and keys'));
		otherFields.forEach(function (name) {
			secrets.appendChild(settingsSecretField(name, report));
		});
		box.appendChild(secrets);
	}
	if (blank.length > 0) {
		box.appendChild(settingsElement('p', 'hint', 'The file has no password or key for these. Type one in to restore it too, or leave it empty to keep what the clock has.'));
	}

	if (report.refused && report.refused.length > 0) {
		box.appendChild(settingsList("Can't be restored, so they stay as they are", report.refused, 'settings-refused'));
	}
	if (report.skipped && report.skipped.length > 0) {
		box.appendChild(settingsList('Skipped', report.skipped));
	}
	if (report.kept && report.kept.length > 0) {
		box.appendChild(settingsList('Not in the file, so they stay as they are', report.kept.map(function (name) { return { name: name }; })));
	}

	var buttons = settingsElement('div', 'settings-buttons');
	if (changes.length > 0 || blank.length > 0) {
		var apply = settingsElement('a', 'settings-primary', 'Apply');
		apply.href = '#';
		apply.id = 'settings_apply';
		apply.setAttribute('data-role', 'button');
		apply.setAttribute('data-inline', 'true');
		apply.onclick = function () { settingsApply(); return false; };
		buttons.appendChild(apply);
	}
	var cancel = settingsElement('a', null, changes.length > 0 || blank.length > 0 ? 'Cancel' : 'Close');
	cancel.href = '#';
	cancel.setAttribute('data-role', 'button');
	cancel.setAttribute('data-inline', 'true');
	cancel.onclick = function () { settingsReset(); return false; };
	buttons.appendChild(cancel);
	box.appendChild(buttons);

	UI.enhance(box);
	addSecretControls(box);
}

// Finds a setting in the file, whatever group it's in
function settingsFindGroup(file, name) {
	var groups = Object.keys(file.settings || {});
	for (var i = 0; i < groups.length; i++) {
		var group = file.settings[groups[i]];
		if (group && typeof group == 'object' && Object.prototype.hasOwnProperty.call(group, name)) {
			return group;
		}
	}
	return null;
}

// The file as it's to be restored: with what was typed in, and without the groups switched off
function settingsFileToApply() {
	var file = JSON.parse(JSON.stringify(settingsFile));
	file.settings = file.settings || {};

	var typed = document.querySelectorAll('#settings_report input[data-secret]');
	for (var i = 0; i < typed.length; i++) {
		var name = typed[i].getAttribute('data-secret');
		var value = typed[i].value;
		if (value.length == 0) {
			continue;
		}
		if (name == 'wifi') {
			file.wifi = file.wifi || {};
			file.wifi.password = value;
		} else {
			var group = settingsFindGroup(file, name);
			if (!group) {
				var groupName = settingsSecrets[name].fileGroup;
				group = file.settings[groupName] = file.settings[groupName] || {};
			}
			group[name] = value;
		}
	}

	settingsGroups.forEach(function (entry) {
		var box = document.getElementById('settings_group_' + entry.group);
		if (!box || box.checked) {
			return;
		}
		if (entry.group == 'wifi') {
			delete file.wifi;
			return;
		}
		var names = (settingsReport.changes || []).filter(function (change) { return change.group == entry.group; }).map(function (change) { return change.name; });
		Object.keys(settingsSecrets).forEach(function (name) {
			if (settingsSecrets[name].group == entry.group) {
				names.push(name);
			}
		});
		names.forEach(function (name) {
			var group;
			while ((group = settingsFindGroup(file, name)) !== null) {
				delete group[name];
			}
		});
	});

	return file;
}

function settingsApply() {
	var file = settingsFileToApply();
	var hostnameGroup = document.getElementById('settings_group_hostname');
	var addressGroup = document.getElementById('settings_group_address');
	var moves = (hostnameGroup && hostnameGroup.checked) || (addressGroup && addressGroup.checked);

	settingsShowMessage('Restoring...');
	settingsSend(file, true).then(function (result) {
		if (result.error) {
			settingsShowMessage(result.error, 'settings-error');
			return;
		}

		var changed = (result.changes || []).length;
		if (!result.restart) {
			settingsShowMessage('Nothing was restored: the clock already has everything in the file.');
			settingsFile = null;
			return;
		}

		var joinsWiFi = (result.changes || []).some(function (change) { return change.name == 'wifi'; });
		var text = 'Restored ' + (changed == 1 ? '1 setting' : changed + ' settings') + '. The clock is restarting to use them.';
		if (joinsWiFi) {
			text += ' It joins ' + file.wifi.ssid + ' and shows its address on the displays. If it can\'t join, its access point comes back on, with a new password on the displays.';
		} else if (result.on_ap) {
			text += ' Its access point closes while it restarts. If it comes back on, it has a new password, on the displays.';
		} else if (moves) {
			text += ' If its hostname or address changed, find it at the new one.';
		}
		var report = settingsShowMessage(text);
		settingsFile = null;
		if (!joinsWiFi && !result.on_ap) {
			settingsWaitForClock(report);
		}
	}).catch(function (e) {
		settingsShowMessage("Couldn't restore it: " + e.message, 'settings-error');
	});
}

// Says when the clock answers again after restarting. It may still answer for a moment before it restarts.
function settingsWaitForClock(report) {
	var tries = 0;
	var missed = false;
	var status = settingsElement('p', 'hint', 'Waiting for the clock...');
	report.appendChild(status);

	function check() {
		tries++;
		var controller = window.AbortController ? new AbortController() : null;
		var timer = setTimeout(function () { if (controller) { controller.abort(); } }, 2000);
		fetch('/favicon.svg?' + Date.now(), { cache: 'no-store', signal: controller ? controller.signal : undefined }).then(function (response) {
			clearTimeout(timer);
			if (response.ok && (missed || tries > 7)) {
				status.textContent = 'The clock is back with its restored settings.';
			} else {
				setTimeout(check, 2000);
			}
		}).catch(function () {
			clearTimeout(timer);
			missed = true;
			if (tries < 60) {
				setTimeout(check, 2000);
			} else {
				status.textContent = "The clock hasn't come back yet. Reload the page in a minute.";
			}
		});
	}
	setTimeout(check, 2000);
}
