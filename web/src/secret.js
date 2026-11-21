// Passwords and keys, the fields with class secret, on the Network, MQTT, Weather and Configuration pages. It uses
// app.js's elementBlur(), getPageId() and safeSend().
//
// The clock never sends one back, only dots to say one is saved, which the field shows. They go when the
// field gets the focus, staying as a grey placeholder until something's typed, and come back if it's left
// empty. What's typed is sent when the field loses focus, unless that's to its show button, so it can be
// checked first. An empty field changes nothing, so there's a link to remove one that's saved.

var savedDots = '********';	// What the clock sends for one that's saved
var placeholderDots = '••••••••';	// Like the password field's

// From the clock, through app.js's updateElements(). Whatever's being typed stays.
function secretSaved(input, saved) {
	input.secretSaved = saved;
	input.placeholder = saved ? placeholderDots : '';
	var remove = input.parentNode.nextElementSibling;
	if (remove && remove.classList.contains('secret-saved')) {
		remove.style.display = saved ? '' : 'none';
	}
	if (input == document.activeElement) {
		return;	// secretLeft() sorts out the dots
	}
	if (input.value == '' || input.secretDots) {
		input.value = saved ? savedDots : '';
		showDots(input, saved);
	}
}

// The field holds the saved dots, which there's nothing to show of
function showDots(input, dots) {
	input.secretDots = dots;
	if (dots) {
		showSecret(input, false);
	}
	if (input.secretButton) {
		input.secretButton.disabled = dots;
	}
}

function showSecret(input, show) {
	input.type = show ? 'text' : 'password';
	var button = input.secretButton;
	if (button) {
		var label = show ? 'Hide' : 'Show';
		button.classList.toggle('shown', show);
		button.title = label;
		button.setAttribute('aria-label', label);
		button.setAttribute('aria-pressed', show);
	}
}

function secretFocus(input) {
	if (input.secretDots) {
		input.value = '';
		showDots(input, false);
	}
}

function secretSend(input, invalidMsg) {
	if (input.value != '' && input.value != input.secretSent) {
		if (elementBlur(input, invalidMsg) !== false) {
			input.secretSent = input.value;
			showSecret(input, false);	// Done with, so not left on the screen
		}
	}
}

// Left empty, the saved dots come back. Otherwise what's typed is sent.
function secretLeft(input, invalidMsg) {
	if (input.value == '' && input.secretSaved) {
		input.value = savedDots;
		showDots(input, true);
	} else {
		secretSend(input, invalidMsg);
	}
}

// The field's onblur
function secretBlur(input, invalidMsg) {
	// Where a press on the show button takes the focus from the field (phones), it puts it back.
	// If the press doesn't become a click, send what's typed anyway.
	if (Date.now() - (input.showPressed || 0) < 1000) {
		setTimeout(function () {
			if (input != document.activeElement) {
				secretLeft(input, invalidMsg);
			}
		}, 1000);
		return;
	}
	secretLeft(input, invalidMsg);
}

function removeSecret(input) {
	input.value = '';
	input.secretSent = undefined;
	showDots(input, false);
	safeSend('9:' + getPageId(activePage()) + ':' + input.id + ':');
}

// Gives each secret field in root a show button and a link to remove what's saved
function addSecretControls(root) {
	root.querySelectorAll('input.secret').forEach(function (input) {
		var wrapper = input.parentNode;
		if (input.secretButton) {
			return;
		}
		wrapper.classList.add('has-secret-show');

		var button = document.createElement('button');
		button.type = 'button';
		button.setAttribute('data-role', 'none');
		button.className = 'secret-show';
		input.secretButton = button;
		var focused = false;
		var press = function (event) {
			if (button.disabled) {
				return;
			}
			if (event.type == 'pointerdown') {
				focused = input == document.activeElement;
				input.showPressed = Date.now();
			} else {
				event.preventDefault();	// Keeps the focus in the field, where browsers allow it
			}
		};
		button.addEventListener('pointerdown', press);
		button.addEventListener('mousedown', press);
		button.addEventListener('click', function () {
			showSecret(input, input.type == 'password');
			input.showPressed = 0;
			if (focused) {
				input.focus();
			}
		});
		input.after(button);
		showSecret(input, false);
		input.addEventListener('focus', function () {
			secretFocus(input);
		});

		var remove = document.createElement('p');
		remove.className = 'hint secret-saved';
		remove.innerHTML = 'Type a new one to replace it, or <a href="#">remove it</a>.';
		remove.querySelector('a').addEventListener('click', function (event) {
			event.preventDefault();
			removeSecret(input);
		});
		remove.style.display = 'none';
		wrapper.after(remove);
	});
}

document.addEventListener('pagecreate', function (event) {
	addSecretControls(event.detail);
});
