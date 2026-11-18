// Passwords and keys, the fields with class secret, on the Network, MQTT and Weather pages. Inlined into
// those pages, which are built into the firmware, so it works with any app.html, including the one in an
// older filesystem image. It only uses app.html's elementBlur(), getPageId() and safeSend().
//
// The clock never sends one back, only dots to say one is saved, which the field shows. They go when the
// field gets the focus, staying as a grey placeholder until something's typed, and come back if it's left
// empty. What's typed is sent when the field loses focus, unless that's to its show button, so it can be
// checked first. An empty field changes nothing, so there's a link to remove one that's saved.

var savedDots = '********';	// What the clock sends for one that's saved
var placeholderDots = '••••••••';	// Like the password field's

// From the clock, through app.html's updateElements(). Whatever's being typed stays.
function secretSaved(element, saved) {
	element.data('saved', saved);
	element.attr('placeholder', saved ? placeholderDots : '');
	element.closest('.ui-input-text').next('.secret-saved').toggle(saved);
	if (element.is(':focus')) {
		return;	// secretLeft() sorts out the dots
	}
	var input = element.get(0);
	if (input.value == '' || element.data('dots')) {
		input.value = saved ? savedDots : '';
		showDots(element, saved);
	}
}

// The field holds the saved dots, which there's nothing to show of
function showDots(element, dots) {
	element.data('dots', dots);
	if (dots) {
		showSecret(element, false);
	}
	element.siblings('.secret-show').prop('disabled', dots);
}

function showSecret(element, show) {
	element.get(0).type = show ? 'text' : 'password';
	var label = show ? 'Hide' : 'Show';
	element.siblings('.secret-show').toggleClass('shown', show)
		.attr({ 'title': label, 'aria-label': label, 'aria-pressed': show });
}

function secretFocus(element) {
	if (element.data('dots')) {
		element.get(0).value = '';
		showDots(element, false);
	}
}

function secretSend(input, invalidMsg) {
	if (input.value != '' && input.value != $(input).data('sent')) {
		if (elementBlur(input, invalidMsg) !== false) {
			$(input).data('sent', input.value);
			showSecret($(input), false);	// Done with, so not left on the screen
		}
	}
}

// Left empty, the saved dots come back. Otherwise what's typed is sent.
function secretLeft(input, invalidMsg) {
	if (input.value == '' && $(input).data('saved')) {
		input.value = savedDots;
		showDots($(input), true);
	} else {
		secretSend(input, invalidMsg);
	}
}

// The field's onblur
function secretBlur(input, invalidMsg) {
	// Where a press on the show button takes the focus from the field (phones), it puts it back.
	// If the press doesn't become a click, send what's typed anyway.
	if (Date.now() - ($(input).data('showPressed') || 0) < 1000) {
		setTimeout(function () {
			if (!$(input).is(':focus')) {
				secretLeft(input, invalidMsg);
			}
		}, 1000);
		return;
	}
	secretLeft(input, invalidMsg);
}

function removeSecret(element) {
	element.get(0).value = '';
	element.removeData('sent');
	showDots(element, false);
	safeSend('9:' + getPageId($(".ui-page-active").attr("id")) + ':' + element.attr('id') + ':');
}

// Gives each secret field a show button and a link to remove what's saved
function addSecretControls(page) {
	page.find('input.secret').each(function (i, input) {
		input = $(input);
		var wrapper = input.closest('.ui-input-text');
		if (wrapper.hasClass('has-secret-show')) {
			return;
		}
		wrapper.addClass('has-secret-show');

		var button = $('<button type="button" data-role="none" class="secret-show"></button>');
		var focused = false;
		button.on('pointerdown mousedown', function (event) {
			if (button.prop('disabled')) {
				return;
			}
			if (event.type == 'pointerdown') {
				focused = input.is(':focus');
				input.data('showPressed', Date.now());
			} else {
				event.preventDefault();	// Keeps the focus in the field, where browsers allow it
			}
		});
		button.on('click', function () {
			showSecret(input, input.get(0).type == 'password');
			input.removeData('showPressed');
			if (focused) {
				input.focus();
			}
		});
		input.after(button);
		showSecret(input, false);
		input.on('focus', function () {
			secretFocus(input);
		});

		var remove = $('<p class="hint secret-saved">Type a new one to replace it, or <a href="#">remove it</a>.</p>');
		remove.find('a').on('click', function () {
			removeSecret(input);
			return false;
		});
		remove.hide();
		wrapper.after(remove);
	});
}

// Each page that has this runs it as it loads, so only the first sets these up. The page is created after.
if (!window.secretFieldsReady) {
	window.secretFieldsReady = true;

	$(document).on('pagecreate', function (event) {
		addSecretControls($(event.target));
	});

	// app.html's updateElements() sets each field with val(). For these, that's whether one is saved.
	// A shown one's type is text, so both types are hooked.
	$.each(['password', 'text'], function (i, type) {
		var hook = $.valHooks[type] || {};
		var set = hook.set;
		hook.set = function (input, value, name) {
			if ($(input).hasClass('secret')) {
				secretSaved($(input), value != '');
				return true;
			}
			return set ? set.apply(this, arguments) : undefined;
		};
		$.valHooks[type] = hook;
	});
}
