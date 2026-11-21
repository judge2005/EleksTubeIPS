// The web UI's widgets, in place of the parts of jQuery Mobile 1.4.5 that the pages use. From the same data-
// attributes it builds the same elements and classes, so ui.css, which has jQuery Mobile's rules for them,
// and theme.css style them as before, and they're used the same way. The Display On hours and the color
// pickers take roundSlider's and Spectrum's class names, which theme.css styles too.
// They work as jQuery Mobile 1.4.5's (c) jQuery Foundation, roundSlider 1.3's (c) Soundar and Spectrum 1.8.1's
// (c) Brian Grinstead did, all MIT licensed.
//
// Pages are fetched and shown in place of the one before, without changing the address. Each sends a
// pagecreate event once it's built, and a pagechange event once it's shown, with the page as event.detail.
'use strict';

var UI = (() => {
	const doc = document;
	const all = (selector, root) => Array.from((root || doc).querySelectorAll(selector));
	const data = (element, name) => element.getAttribute('data-' + name);
	const fire = (element, type) => element.dispatchEvent(new Event(type, { bubbles: true }));

	function make(tag, className, text) {
		const element = doc.createElement(tag);
		if (className) {
			element.className = className;
		}
		if (text != null) {
			element.textContent = text;
		}
		return element;
	}

	function wrap(element, wrapper) {
		element.parentNode.insertBefore(wrapper, element);
		wrapper.appendChild(element);
		return wrapper;
	}

	// The classes of a button's data- attributes
	function buttonClasses(element) {
		let classes = '';
		if (data(element, 'theme')) {
			classes += ' ui-btn-' + data(element, 'theme');
		}
		if (data(element, 'inline') == 'true') {
			classes += ' ui-btn-inline';
		}
		if (data(element, 'mini') == 'true') {
			classes += ' ui-mini';
		}
		if (data(element, 'icon')) {
			classes += ' ui-icon-' + data(element, 'icon') + ' ui-btn-icon-' + (data(element, 'iconpos') || 'left');
		}
		return classes;
	}

	// Toolbars, buttons and fields

	function header(element) {
		element.classList.add('ui-header', 'ui-bar-inherit');
		element.setAttribute('role', 'banner');
		if (data(element, 'position') == 'fixed') {
			element.classList.add('ui-header-fixed', 'slidedown');
			const page = element.closest('[data-role=page]');
			if (page) {
				page.classList.add('ui-page-header-fixed');
			}
		}
		const title = element.querySelector('h1');
		if (title) {
			title.classList.add('ui-title');
			title.setAttribute('role', 'heading');
			title.setAttribute('aria-level', '1');
		}
	}

	// A fixed header, like jQuery Mobile's fixed toolbars: a tap on the page where there's nothing to use hides or
	// shows it, and on a small screen it's hidden while a field has the focus, to leave room for the keyboard.
	// Hidden, it scrolls away with the page. It slides when it's scrolled past.

	// What a tap doesn't do that for: jQuery Mobile's list, and what its widgets and Spectrum's kept their clicks from
	const tapIgnore = 'a, button, input, select, textarea, .ui-header-fixed, .ui-flipswitch, .ui-popup, .ui-popup-screen, .ui-panel, ' +
		'.ui-panel-dismiss-open, .ui-radio, .ui-checkbox, .ui-slider, .sp-replacer';

	function headerShow(bar, show) {
		bar.uiHidden = !show;
		const slide = scrollY > bar.clientHeight;
		if (show) {
			bar.classList.remove('out', 'reverse', 'ui-fixed-hidden');
			if (slide) {
				animate(bar, 'in');
			}
		} else if (slide) {
			bar.classList.remove('in');
			animate(bar, 'out reverse', () => bar.uiHidden && bar.classList.add('ui-fixed-hidden'));
		} else {
			bar.classList.add('ui-fixed-hidden');
		}
	}

	const activeHeader = () => {
		const page = doc.querySelector('.ui-page-active');
		return page && page.querySelector('.ui-header-fixed');
	};

	doc.addEventListener('click', event => {
		const bar = activeHeader();
		if (bar && bar.parentNode.contains(event.target) && !event.target.closest(tapIgnore)) {
			headerShow(bar, bar.uiHidden);
		}
	});

	// Moving from one field to the next leaves it hidden
	let fieldFocused = false;
	let headerTimer = 0;
	const fieldFocus = event => {
		const bar = activeHeader();
		const field = event.target;
		// Not radio buttons, which jQuery Mobile's labels didn't focus, nor checkboxes: neither brings up a keyboard
		if (!bar || screen.width >= 1025 || !field.matches || !field.matches('input:not([type=radio]):not([type=checkbox]), textarea, select') ||
				!bar.parentNode.contains(field) || bar.contains(field)) {
			return;
		}
		const focused = event.type == 'focusin';
		if (focused != fieldFocused) {
			fieldFocused = focused;
			clearTimeout(headerTimer);
			headerTimer = setTimeout(() => headerShow(bar, !focused), 0);
		}
	};
	doc.addEventListener('focusin', fieldFocus);
	doc.addEventListener('focusout', fieldFocus);

	function linkButton(a) {
		a.className += ' ui-link ui-btn ui-shadow ui-corner-all' + buttonClasses(a);
		a.setAttribute('role', 'button');
	}

	function inputButton(input) {
		const button = wrap(input, make('div', 'ui-btn ui-input-btn ui-corner-all ui-shadow' + buttonClasses(input)));
		button.insertBefore(doc.createTextNode(input.value), input);
	}

	function textInput(input) {
		// No suggestions from what's been typed elsewhere, as jQuery Mobile had it
		if (!input.hasAttribute('autocomplete')) {
			input.setAttribute('autocomplete', 'off');
		}
		input.setAttribute('autocorrect', 'off');
		const field = wrap(input, make('div', 'ui-input-text ui-body-inherit ui-corner-all' +
			(data(input, 'mini') == 'true' ? ' ui-mini' : '') + ' ui-shadow-inset' +
			(data(input, 'wrapper-class') ? ' ' + data(input, 'wrapper-class') : '')));
		if (data(input, 'clear-btn') == 'true') {
			field.classList.add('ui-input-has-clear');
			const clear = make('a', 'ui-input-clear ui-btn ui-icon-delete ui-btn-icon-notext ui-corner-all ui-input-clear-hidden', 'Clear text');
			clear.href = '#';
			clear.tabIndex = -1;
			clear.title = 'Clear text';
			clear.setAttribute('aria-hidden', 'true');
			const update = () => clear.classList.toggle('ui-input-clear-hidden', !input.value);
			input.addEventListener('input', update);
			input.addEventListener('change', update);
			clear.addEventListener('click', event => {
				event.preventDefault();
				input.value = '';
				update();
				fire(input, 'change');
			});
			field.appendChild(clear);
		}
	}

	// A select shows its choice on a button, under the select itself, which takes the clicks
	function select(element) {
		const button = make('div', 'ui-btn ui-icon-carat-d ui-btn-icon-right ui-corner-all ui-shadow');
		button.id = element.id + '-button';
		wrap(element, make('div', 'ui-select' + (data(element, 'mini') == 'true' ? ' ui-mini' : ''))).appendChild(button);
		button.appendChild(make('span'));
		button.appendChild(element);
		element.addEventListener('focus', () => button.classList.add('ui-focus'));
		element.addEventListener('blur', () => button.classList.remove('ui-focus'));
		element.addEventListener('change', () => refresh(element));
		refresh(element);
	}

	// Switches: a checkbox, hidden in a track the knob moves along

	function flipswitch(input) {
		const track = make('div', 'ui-flipswitch ui-shadow-inset ui-bar-inherit' +
			(data(input, 'wrapper-class') ? ' ' + data(input, 'wrapper-class') : '') + ' ui-corner-all');
		const knob = make('a', 'ui-flipswitch-on ui-btn ui-shadow ui-btn-inherit', data(input, 'on-text') || 'On');
		knob.href = '#';
		input.parentNode.insertBefore(track, input);
		track.appendChild(knob);
		track.appendChild(make('span', 'ui-flipswitch-off', data(input, 'off-text') || 'Off'));
		track.appendChild(input);
		input.classList.add('ui-flipswitch-input');
		input.tabIndex = -1;

		const toggle = event => {
			event.preventDefault();
			input.checked = !input.checked;
			refresh(input);
			fire(input, 'change');
		};
		// Not the click a label for the checkbox sends it, which changes it by itself
		track.addEventListener('click', event => event.target != input && toggle(event));
		knob.addEventListener('keydown', event => {
			if (event.key == ' ') {
				toggle(event);
			}
		});
		input.addEventListener('change', () => refresh(input));
		refresh(input);
	}

	// Segmented buttons: radio buttons whose labels are the buttons

	function controlgroup(group) {
		group.classList.add('ui-controlgroup', data(group, 'type') == 'horizontal' ? 'ui-controlgroup-horizontal' : 'ui-controlgroup-vertical', 'ui-corner-all');
		const mini = data(group, 'mini') == 'true';
		if (mini) {
			group.classList.add('ui-mini');
		}
		const controls = make('div', 'ui-controlgroup-controls');
		while (group.firstChild) {
			controls.appendChild(group.firstChild);
		}
		group.appendChild(controls);

		const labels = [];
		all('input[type=radio], input[type=checkbox]', controls).forEach(input => {
			const label = controls.querySelector('label[for="' + input.id + '"]');
			const box = make('div', 'ui-' + input.type + (mini ? ' ui-mini' : ''));
			controls.insertBefore(box, input);
			if (label) {
				label.classList.add('ui-btn', 'ui-corner-all', 'ui-btn-inherit');
				box.appendChild(label);
				labels.push(label);
			}
			box.appendChild(input);
			refresh(input);
		});
		if (labels.length) {
			labels[0].classList.add('ui-first-child');
			labels[labels.length - 1].classList.add('ui-last-child');
		}
	}

	// Sliders: a number field and a track with a handle, which both set the value

	function slider(input) {
		const min = parseFloat(input.min) || 0;
		const max = input.max === '' ? 100 : parseFloat(input.max);
		const step = parseFloat(input.step) || 1;
		const highlight = data(input, 'highlight') == 'true';

		input.type = 'number';
		input.setAttribute('data-type', 'range');
		input.setAttribute('autocorrect', 'off');
		input.setAttribute('autocomplete', 'off');
		input.className += ' ui-shadow-inset ui-body-inherit ui-corner-all' +
			(data(input, 'wrapper-class') ? ' ' + data(input, 'wrapper-class') : '') + ' ui-slider-input';
		const label = doc.querySelector('label[for="' + input.id + '"]');
		if (label) {
			label.id = input.id + '-label';
		}

		const box = wrap(input, make('div', 'ui-slider'));
		const track = make('div', 'ui-slider-track ui-shadow-inset ui-bar-inherit ui-corner-all');
		track.setAttribute('role', 'application');
		const fill = highlight ? track.appendChild(make('div', 'ui-slider-bg ui-btn-active')) : null;
		const handle = make('a', 'ui-slider-handle ui-btn ui-shadow');
		handle.href = '#';
		handle.setAttribute('role', 'slider');
		handle.setAttribute('aria-valuemin', min);
		handle.setAttribute('aria-valuemax', max);
		if (label) {
			handle.setAttribute('aria-labelledby', label.id);
		}
		track.appendChild(handle);
		box.appendChild(track);
		input.uiSlider = { min, max, step, handle, fill };

		// Each new value is sent as the handle moves, as jQuery Mobile did
		const set = value => {
			value = clamp(Math.round((value - min) / step) * step + min, min, max);
			if (String(value) != input.value) {
				input.value = value;
				refresh(input);
				fire(input, 'change');
			}
		};
		// As jQuery Mobile worked it out: from the whole pixel, as mouse events give it, ignoring the pointer once it's
		// 8px past either end, and in whole percents if the track has fewer pixels than steps
		const fromPointer = event => {
			const left = track.getBoundingClientRect().left;
			const width = track.offsetWidth;
			const x = Math.floor(event.clientX);
			if (x >= left - 8 && x <= left + width + 8) {
				const percent = (x - left) / width * 100;
				set(min + (width / ((max - min) / step) > 1 ? percent : Math.round(percent)) / 100 * (max - min));
			}
		};
		track.addEventListener('pointerdown', event => {
			if (event.button) {
				return;
			}
			event.preventDefault();
			handle.focus({ preventScroll: true });
			track.setPointerCapture(event.pointerId);
			fromPointer(event);
			const move = moveEvent => fromPointer(moveEvent);
			const end = () => {
				track.removeEventListener('pointermove', move);
				track.removeEventListener('pointerup', end);
				track.removeEventListener('pointercancel', end);
			};
			track.addEventListener('pointermove', move);
			track.addEventListener('pointerup', end);
			track.addEventListener('pointercancel', end);
		});
		handle.addEventListener('click', event => event.preventDefault());
		// jQuery Mobile's keys: a step each way, or to either end
		handle.addEventListener('keydown', event => {
			let value = parseFloat(input.value);
			if (isNaN(value)) {
				value = min;
			}
			const keys = {
				ArrowRight: value + step, ArrowUp: value + step, PageUp: value + step,
				ArrowLeft: value - step, ArrowDown: value - step, PageDown: value - step, Home: min, End: max
			};
			if (event.key in keys) {
				event.preventDefault();
				set(keys[event.key]);
			}
		});
		// While typing, the handle follows. The value is kept in range before it's sent.
		input.addEventListener('input', () => refresh(input, true));
		refresh(input);
	}

	const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

	// Before the field's own onchange sends a typed value, it's put in range, as the handle shows it
	doc.addEventListener('change', event => {
		const input = event.target;
		if (input.uiSlider && input.value !== '') {
			const s = input.uiSlider;
			const value = clamp(Math.round((parseFloat(input.value) - s.min) / s.step) * s.step + s.min, s.min, s.max);
			if (!isNaN(value) && String(value) != input.value) {
				input.value = value;
			}
			refresh(input);
		}
	}, true);

	// Sections that open and close

	function collapsible(section) {
		const legend = section.querySelector('legend');
		const heading = make('div', 'ui-collapsible-heading');
		heading.setAttribute('role', 'heading');
		const toggle = make('a', 'ui-collapsible-heading-toggle ui-btn ui-btn-icon-' + (data(section, 'iconpos') || 'left') + ' ui-btn-inherit', legend ? legend.textContent : '');
		toggle.href = '#';
		toggle.appendChild(make('span', 'ui-collapsible-heading-status'));
		heading.appendChild(toggle);

		const content = make('div', 'ui-collapsible-content ui-body-inherit');
		if (legend) {
			// jQuery Mobile left an empty element where the legend was, which theme.css allows for
			const placeholder = make('div');
			placeholder.appendChild(doc.createComment(' placeholder for legend '));
			section.replaceChild(placeholder, legend);
		}
		while (section.firstChild) {
			content.appendChild(section.firstChild);
		}
		section.classList.add('ui-collapsible', 'ui-collapsible-inset', 'ui-corner-all', 'ui-collapsible-themed-content');
		section.appendChild(heading);
		section.appendChild(content);

		const show = collapsed => {
			const icons = [data(section, 'collapsed-icon') || 'plus', data(section, 'expanded-icon') || 'minus'];
			section.classList.toggle('ui-collapsible-collapsed', collapsed);
			toggle.classList.remove('ui-icon-' + icons[collapsed ? 1 : 0]);
			toggle.classList.add('ui-icon-' + icons[collapsed ? 0 : 1]);
			toggle.lastChild.textContent = collapsed ? ' click to expand contents' : ' click to collapse contents';
			content.classList.toggle('ui-collapsible-content-collapsed', collapsed);
			content.setAttribute('aria-hidden', collapsed);
		};
		toggle.addEventListener('click', event => {
			event.preventDefault();
			show(!section.classList.contains('ui-collapsible-collapsed'));
		});
		show(data(section, 'collapsed') != 'false');
	}

	// Lists, of links, or of a link with a second one beside it

	function listview(list) {
		list.classList.add('ui-listview');
		if (data(list, 'inset') == 'true') {
			list.classList.add('ui-listview-inset', 'ui-corner-all', 'ui-shadow');
		}
		refresh(list);
	}

	function refreshList(list) {
		const items = all(':scope > li', list);
		items.forEach((item, i) => {
			item.classList.toggle('ui-first-child', i == 0);
			item.classList.toggle('ui-last-child', i == items.length - 1);
			const links = all(':scope > a', item);
			const theme = data(item, 'theme') ? ' ui-btn-' + data(item, 'theme') : '';
			if (links.length > 1) {
				item.classList.add('ui-li-has-alt');
				if (!links[0].classList.contains('ui-btn')) {
					links[0].className += ' ui-btn' + theme;
					links[1].title = links[1].textContent;
					links[1].textContent = '';
					links[1].className += ' ui-btn ui-btn-icon-notext ui-icon-' + (data(list, 'split-icon') || 'carat-r') + theme;
				}
			} else if (links.length == 1 && !links[0].classList.contains('ui-btn')) {
				links[0].className += ' ui-btn ui-btn-icon-right ui-icon-carat-r' + theme;
			}
		});
	}

	// Pop-ups: in a box in the middle of the window, over a screen that closes it

	let openPopup = null;

	function popup(element) {
		element.classList.add('ui-popup', 'ui-body-inherit', 'ui-overlay-shadow', 'ui-corner-all');
		const screen = make('div', 'ui-screen-hidden ui-popup-screen ui-overlay-inherit');
		screen.id = element.id + '-screen';
		const box = make('div', 'ui-popup-container ui-popup-hidden ui-popup-truncate');
		box.id = element.id + '-popup';
		box.tabIndex = -1;
		// At the end of its page, or of the body for app.html's own
		const host = element.closest('[data-role=page]') || doc.body;
		host.appendChild(screen);
		host.appendChild(box);
		box.appendChild(element);
		screen.addEventListener('click', () => popupClose(element));
	}

	function animate(element, classes, done) {
		const list = classes.split(' ');
		element.classList.add(...list);
		let finished = false;
		const end = () => {
			if (!finished) {
				finished = true;
				element.classList.remove(...list);
				if (done) {
					done();
				}
			}
		};
		element.addEventListener('animationend', end, { once: true });
		setTimeout(end, 500);	// If there's no animation
	}

	// In the middle of the window, at least 15px from its sides and 30px from its top and bottom, as jQuery
	// Mobile put it, relative to the page it's in
	function place(box) {
		const fit = (room, length, start, middle) => room < length ? start + (room - length) / 2 :
			Math.min(Math.max(start, middle - length / 2), start + room - length);
		box.style.maxWidth = (innerWidth - 30) + 'px';
		const x = fit(innerWidth - 30, box.offsetWidth, scrollX + 15, scrollX + innerWidth / 2);
		const y = Math.max(0, fit(innerHeight - 60, box.offsetHeight, scrollY + 30, scrollY + innerHeight / 2));
		const parent = box.offsetParent;
		const origin = parent && parent != doc.body ? parent.getBoundingClientRect() : { left: -scrollX, top: -scrollY };
		box.style.left = (x - origin.left - scrollX) + 'px';
		box.style.top = (y - origin.top - scrollY) + 'px';
	}

	// returnTo is where the keyboard focus goes when it closes, by default where it was
	function popupOpen(element, returnTo) {
		element = typeof element == 'string' ? doc.getElementById(element) : element;
		if (openPopup == element) {
			return;
		}
		if (openPopup) {
			popupClose(openPopup);
		}
		openPopup = element;
		const box = element.parentNode;
		const screen = doc.getElementById(element.id + '-screen');
		box.uiFocusBefore = returnTo || doc.activeElement;
		screen.classList.remove('ui-screen-hidden', 'out');
		screen.classList.add('in');
		box.classList.remove('ui-popup-hidden', 'ui-popup-truncate');
		box.classList.add('ui-popup-active');
		place(box);
		// Keyboard focus goes to the pop-up and stays in it, and goes back when it closes
		box.focus({ preventScroll: true });
		animate(box, 'pop in');
	}

	doc.addEventListener('focusin', event => {
		if (openPopup && !openPopup.parentNode.contains(event.target)) {
			openPopup.parentNode.focus({ preventScroll: true });
		}
	});

	function popupClose(element) {
		element = typeof element == 'string' ? doc.getElementById(element) : element;
		const box = element.parentNode;
		const screen = doc.getElementById(element.id + '-screen');
		if (!box.classList.contains('ui-popup-active')) {
			return;
		}
		if (openPopup == element) {
			openPopup = null;
		}
		box.classList.remove('ui-popup-active');
		screen.classList.remove('in');
		animate(screen, 'out', () => screen.classList.add('ui-screen-hidden'));
		animate(box, 'pop out reverse', () => {
			box.classList.add('ui-popup-hidden', 'ui-popup-truncate');
			const before = box.uiFocusBefore;
			if (before && before.isConnected && (!doc.activeElement || doc.activeElement == doc.body || box.contains(doc.activeElement))) {
				before.focus({ preventScroll: true });
			}
		});
	}

	// The menu: a panel that slides in from the left, over the page

	let menu = null;
	let dismiss = null;

	function panel(element) {
		menu = element;
		element.className += ' ui-panel ui-panel-position-left ui-panel-display-overlay ui-panel-closed ui-body-' + (data(element, 'theme') || 'a') + ' ui-panel-animate';
		const inner = make('div', 'ui-panel-inner');
		while (element.firstChild) {
			inner.appendChild(element.firstChild);
		}
		element.appendChild(inner);
		dismiss = doc.body.appendChild(make('div', 'ui-panel-dismiss'));
		dismiss.addEventListener('click', () => panelOpen(false));
		// A swipe to the left closes it
		let startX = null;
		element.addEventListener('pointerdown', event => startX = event.clientX);
		element.addEventListener('pointerup', event => {
			if (startX !== null && startX - event.clientX > 60) {
				panelOpen(false);
			}
			startX = null;
		});
	}

	function panelOpen(open) {
		if (!menu) {
			return;
		}
		if (open) {
			menu.classList.remove('ui-panel-closed');
			// So the slide starts from the closed position
			menu.offsetWidth;
			menu.classList.add('ui-panel-open');
			dismiss.className = 'ui-panel-dismiss ui-panel-dismiss-position-left ui-panel-dismiss-display-overlay ui-panel-dismiss-open';
			dismiss.style.height = Math.max(doc.documentElement.scrollHeight, innerHeight) + 'px';
		} else if (menu.classList.contains('ui-panel-open')) {
			menu.classList.remove('ui-panel-open');
			dismiss.className = 'ui-panel-dismiss';
			dismiss.style.height = '';
			const closed = () => {
				if (!menu.classList.contains('ui-panel-open')) {
					menu.classList.add('ui-panel-closed');
				}
			};
			menu.addEventListener('transitionend', closed, { once: true });
			setTimeout(closed, 400);
		}
	}

	// The loading box

	const loader = make('div', 'ui-loader ui-corner-all ui-body-a ui-loader-default');
	loader.appendChild(make('span', 'ui-icon-loading'));
	loader.appendChild(make('h1', null, 'loading'));

	function loading(show, text) {
		if (show) {
			loader.className = 'ui-loader ui-corner-all ui-body-a ' + (text ? 'ui-loader-verbose' : 'ui-loader-default');
			loader.lastChild.textContent = text || 'loading';
			if (!loader.parentNode) {
				doc.body.appendChild(loader);
			}
		}
		doc.documentElement.classList.toggle('ui-loading', !!show);
	}

	// The color pickers, like Spectrum's: a swatch that opens a square of saturation and brightness beside a
	// strip of hues. onMove gets { h: 0-360, s: 0-1, v: 0-1 } as either is dragged.

	function hsvToRgb(h, s, v) {
		const f = n => {
			const k = (n + h / 60) % 6;
			return Math.round(255 * (v - v * s * Math.max(0, Math.min(k, 4 - k, 1))));
		};
		return 'rgb(' + f(5) + ', ' + f(3) + ', ' + f(1) + ')';
	}

	// Like Spectrum, the pickers are at the end of the body, and go with their pages
	let pickers = [];

	function colorPicker(input, onMove) {
		const color = { h: 0, s: 1, v: 1 };
		input.style.display = 'none';
		const swatch = make('div', 'sp-replacer sp-light');
		const preview = swatch.appendChild(make('div', 'sp-preview')).appendChild(make('div', 'sp-preview-inner'));
		swatch.appendChild(make('div', 'sp-dd', '▼'));
		input.parentNode.insertBefore(swatch, input.nextSibling);

		const box = make('div', 'sp-container sp-light sp-hidden');
		box.innerHTML = '<div class="sp-picker-container"><div class="sp-top sp-cf"><div class="sp-fill"></div><div class="sp-top-inner">' +
			'<div class="sp-color"><div class="sp-sat"><div class="sp-val"><div class="sp-dragger"></div></div></div></div>' +
			'<div class="sp-hue"><div class="sp-slider"></div></div></div></div>' +
			'<div class="sp-button-container sp-cf"><button type="button" class="sp-choose">Close</button></div></div>';
		box.uiSwatch = swatch;
		const area = box.querySelector('.sp-color');
		const dragger = box.querySelector('.sp-dragger');
		const hues = box.querySelector('.sp-hue');
		const hueHandle = box.querySelector('.sp-slider');
		doc.body.appendChild(box);
		pickers.push(box);

		const show = () => {
			const rgb = hsvToRgb(color.h, color.s, color.v);
			preview.style.backgroundColor = rgb;
			area.style.backgroundColor = hsvToRgb(color.h, 1, 1);
			if (!box.classList.contains('sp-hidden')) {
				const w = area.clientWidth, h = area.clientHeight;
				dragger.style.left = clamp(color.s * w - 5, -5, w - 5) + 'px';
				dragger.style.top = clamp((1 - color.v) * h - 5, -5, h - 5) + 'px';
				hueHandle.style.top = (color.h / 360 * hues.clientHeight - 3) + 'px';
			}
		};
		// Where the pointer is inside the border, from 0 to 1 each way, as Spectrum measured it
		const drag = (target, onPoint) => {
			target.addEventListener('pointerdown', event => {
				event.preventDefault();
				target.setPointerCapture(event.pointerId);
				const point = e => {
					const r = target.getBoundingClientRect();
					const w = target.clientWidth, h = target.clientHeight;
					onPoint(clamp(Math.floor(e.clientX) - r.left, 0, w) / w, clamp(Math.floor(e.clientY) - r.top, 0, h) / h);
					show();
					// Cut short as Spectrum's colors were: hundredths of a percent, or of a degree
					const cut = (n, by) => Math.floor(n * by) / by;
					onMove({ h: cut(color.h, 100), s: cut(color.s, 10000), v: cut(color.v, 10000) });
				};
				point(event);
				target.addEventListener('pointermove', point);
				const end = () => target.removeEventListener('pointermove', point);
				target.addEventListener('pointerup', end, { once: true });
				target.addEventListener('pointercancel', end, { once: true });
			});
		};
		drag(area, (x, y) => {
			color.s = x;
			color.v = 1 - y;
		});
		drag(hues, (x, y) => color.h = y * 360);

		const open = isOpen => {
			box.classList.toggle('sp-hidden', !isOpen);
			swatch.classList.toggle('sp-active', isOpen);
			if (isOpen) {
				// Under the swatch, moved left to fit the window, or above the swatch if there's no room
				// below, as Spectrum put it
				const r = swatch.getBoundingClientRect();
				const width = box.offsetWidth, height = box.offsetHeight;
				const viewWidth = doc.documentElement.clientWidth + scrollX;
				const viewHeight = doc.documentElement.clientHeight + scrollY;
				let left = r.left + scrollX;
				let top = r.top + scrollY + swatch.offsetHeight;
				left -= Math.min(left, left + width > viewWidth && viewWidth > width ? Math.abs(left + width - viewWidth) : 0);
				top -= Math.min(top, top + height > viewHeight && viewHeight > height ? Math.abs(height + swatch.offsetHeight) : 0);
				box.style.left = left + 'px';
				box.style.top = top + 'px';
				show();
			}
		};
		swatch.addEventListener('click', () => open(box.classList.contains('sp-hidden')));
		box.querySelector('.sp-choose').addEventListener('click', () => open(false));
		// A press anywhere else closes it
		const outside = event => {
			if (!box.isConnected) {
				doc.removeEventListener('pointerdown', outside);
			} else if (!box.contains(event.target) && !swatch.contains(event.target)) {
				open(false);
			}
		};
		doc.addEventListener('pointerdown', outside);
		show();
		return {
			set(hsv) {
				Object.assign(color, hsv);
				color.h %= 360;	// 360 is red too, shown at the top as Spectrum did
				show();
			}
		};
	}

	// The Display On hours: a ring of 24 hours, from 0 on the left round clockwise, with a handle at each end
	// of the time the display's on. onChange gets both ends when a handle's dropped or moved with the keys.
	// It's drawn as roundSlider drew it, so theme.css's gradient falls the same way: the arc is a half disc
	// turned to start at the first handle, with another turned to end at the second if the arc's longer than
	// half the ring. Halves of the ring's own color cover the rest. The handles are on bars that turn about
	// the middle, and turn smoothly to new hours from the clock.

	function hourRing(element, onChange) {
		const ends = [8, 22];
		element.classList.add('rs-control', 'rs-animation', 'ui-ring');
		element.innerHTML = '<div class="rs-block rs-outer rs-border rs-split">' +
			'<div class="rs-path rs-transition rs-range-color"></div><div class="rs-path rs-transition rs-range-color"></div>' +
			'<div class="rs-path rs-transition rs-path-color"></div><div class="rs-path rs-transition rs-path-color"></div>' +
			'<span class="rs-block"><div class="rs-inner rs-bg-color rs-border"></div></span></div>' +
			'<div class="rs-bar rs-transition"><div class="rs-handle rs-move"></div></div>' +
			'<div class="rs-bar rs-transition"><div class="rs-handle rs-move"></div></div>' +
			'<span class="rs-tooltip rs-tooltip-text"></span>';
		const halves = element.querySelectorAll('.rs-path');
		const bars = element.querySelectorAll('.rs-bar');
		const handles = Array.from(element.querySelectorAll('.rs-handle'));
		const text = element.querySelector('.rs-tooltip');
		halves[3].style.zIndex = 1;
		handles.forEach((handle, i) => {
			handle.tabIndex = 0;
			handle.setAttribute('index', i + 1);
			handle.setAttribute('role', 'slider');
			handle.setAttribute('aria-valuemin', '0');
			handle.setAttribute('aria-valuemax', '24');
			handle.setAttribute('aria-label', i ? 'Display off hour' : 'Display on hour');
		});

		const rotate = (part, angle) => part.style.transform = 'rotate(' + angle + 'deg)';
		const show = () => {
			const first = ends[0] * 15, second = ends[1] * 15, span = second - first;
			const long = !(span <= 180 && !(span < 0 && span > -180));
			halves[1].style.opacity = long ? 1 : 0;
			halves[2].style.opacity = long ? 0 : 1;
			rotate(halves[0], first);
			rotate(halves[3], first - 180);
			rotate(halves[2], second);
			rotate(halves[1], second - 180);
			rotate(bars[0], first);
			rotate(bars[1], second);
			handles.forEach((handle, i) => handle.setAttribute('aria-valuenow', ends[i]));
			text.textContent = ends[0] + ' - ' + ends[1];
			// Centred by half its size in whole pixels, as roundSlider did, once it's shown
			if (text.offsetWidth) {
				text.style.transform = 'none';
				text.style.margin = (-text.offsetHeight / 2) + 'px 0 0 ' + (-text.offsetWidth / 2) + 'px';
			}
		};
		// The hour at the pointer, as roundSlider worked it out: by the angle from the left, from the whole pixel as mouse
		// events give it, to the nearest hour, so just before the start is 24 and just after it 0
		const pointerAt = event => {
			const r = element.getBoundingClientRect();
			const x = Math.floor(event.clientX) - r.left - element.offsetWidth / 2;
			const y = Math.floor(event.clientY) - r.top - element.offsetHeight / 2;
			let angle = -Math.atan2(y, -x) * 180 / Math.PI;
			if (angle < 0) {
				angle += 360;
			}
			return { hour: Math.round(angle / 15), distance: Math.sqrt(x * x + y * y) };
		};
		// While a handle's moved by hand it follows straight away
		const animated = on => element.classList.toggle('rs-animation', on);
		const changed = () => onChange(ends.slice());

		element.addEventListener('pointerdown', event => {
			if (event.button) {
				return;
			}
			const i = handles.indexOf(event.target);
			const at = pointerAt(event);
			if (i < 0) {
				// A press on the ring moves the handle that has the focus, or else the nearer, there
				if (at.distance >= 39 && at.distance <= 60) {
					event.preventDefault();
					const focused = handles.indexOf(doc.activeElement);
					const j = focused >= 0 ? focused : ends[1] - at.hour < at.hour - ends[0] ? 1 : 0;
					ends[j] = at.hour;
					show();
					changed();
				}
				return;
			}
			event.preventDefault();
			const before = ends[i];
			handles[i].focus({ preventScroll: true });
			element.setPointerCapture(event.pointerId);
			animated(false);
			const move = e => {
				ends[i] = pointerAt(e).hour;
				show();
			};
			const end = () => {
				element.removeEventListener('pointermove', move);
				element.removeEventListener('pointerup', end);
				element.removeEventListener('pointercancel', end);
				animated(true);
				if (ends[i] != before) {
					changed();
				}
			};
			element.addEventListener('pointermove', move);
			element.addEventListener('pointerup', end);
			element.addEventListener('pointercancel', end);
		});
		// roundSlider's keys: up and left add an hour, right and down take one away, Home and End go as far as they can
		handles.forEach((handle, i) => {
			handle.addEventListener('keydown', event => {
				const keys = {
					ArrowUp: ends[i] + 1, ArrowLeft: ends[i] + 1, ArrowRight: ends[i] - 1, ArrowDown: ends[i] - 1,
					Home: i ? ends[0] : 0, End: i ? 24 : ends[1]
				};
				if (event.key == 'Escape') {
					handle.blur();
				} else if (event.key in keys) {
					event.preventDefault();
					if (event.key.startsWith('Arrow')) {
						animated(false);
					}
					ends[i] = clamp(keys[event.key], 0, 24);
					show();
					changed();
				}
			});
			handle.addEventListener('keyup', () => animated(true));
		});
		show();
		return {
			set(i, hour) {
				ends[i] = Number(hour);
				show();
			}
		};
	}

	// Building and refreshing

	// Builds the widgets in root from their markup, like jQuery Mobile's enhanceWithin()
	function enhance(root) {
		const done = element => {
			if (element.uiDone) {
				return false;
			}
			element.uiDone = true;
			return true;
		};
		const each = (selector, build) => all(selector, root).filter(done).forEach(build);
		each('[data-role=header]', header);
		each('[data-role=content]', element => {
			element.classList.add('ui-content');
			element.setAttribute('role', 'main');
		});
		each('[data-role=fieldcontain]', element => element.classList.add('ui-field-contain'));
		each('[data-role=collapsible]', collapsible);
		each('[data-role=controlgroup]', controlgroup);
		each('input[type=checkbox][data-role=flipswitch]', flipswitch);
		each('input[type=range]', slider);
		each('select', select);
		each('input[type=button], input[type=submit]', inputButton);
		each('a[data-role=button]', linkButton);
		each('input[type=text], input[type=password], input[type=number]:not(.ui-slider-input), input[type=file], input:not([type])', element => {
			if (data(element, 'role') != 'none') {
				textInput(element);
			}
		});
		each('[data-role=listview]', listview);
		each('table[data-role=table]', element => element.classList.add('ui-table'));
		each('[data-role=popup]', popup);
		each('[data-role=panel]', panel);
		each('.ui-content a:not([data-role]):not(.ui-btn)', element => element.classList.add('ui-link'));
	}

	// Shows what's in a widget's input or list after it's changed from code, like jQuery Mobile's refresh
	function refresh(element) {
		if (!element) {
			return;
		}
		if (element.tagName == 'UL') {
			refreshList(element);
		} else if (element.tagName == 'SELECT') {
			const option = element.options[element.selectedIndex];
			element.parentNode.firstChild.textContent = option ? option.text : ' ';
		} else if (element.uiSlider) {
			const s = element.uiSlider;
			let value = parseFloat(element.value);
			if (isNaN(value)) {
				value = s.min;
			}
			if (!arguments[1]) {
				// Not while it's being typed
				value = clamp(value, s.min, s.max);
				if (element.value !== '' && String(value) != element.value) {
					element.value = value;
				}
			}
			const percent = (clamp(value, s.min, s.max) - s.min) / (s.max - s.min) * 100;
			s.handle.style.left = percent + '%';
			if (s.fill) {
				s.fill.style.width = percent + '%';
			}
			s.handle.setAttribute('aria-valuenow', value);
			s.handle.setAttribute('aria-valuetext', value);
			s.handle.title = value;
		} else if (element.classList.contains('ui-flipswitch-input')) {
			element.parentNode.classList.toggle('ui-flipswitch-active', element.checked);
		} else if (element.type == 'radio' || element.type == 'checkbox') {
			const group = element.type == 'radio' && element.name ? all('input[type=radio][name="' + element.name + '"]') : [element];
			group.forEach(input => {
				const label = input.previousElementSibling;
				if (label && label.tagName == 'LABEL') {
					label.classList.toggle('ui-btn-active', input.checked);
					label.classList.toggle('ui-' + input.type + '-on', input.checked);
					label.classList.toggle('ui-' + input.type + '-off', !input.checked);
				}
			});
		}
	}

	// Radio buttons and checkboxes show their state when they're clicked
	doc.addEventListener('change', event => {
		const input = event.target;
		if ((input.type == 'radio' || input.type == 'checkbox') && !input.classList.contains('ui-flipswitch-input')) {
			refresh(input);
		}
	});

	// Fields and their labels show the focus
	doc.addEventListener('focusin', event => {
		const field = event.target.parentNode;
		if (field && field.classList && field.classList.contains('ui-input-text')) {
			field.classList.add('ui-focus');
		}
	});
	doc.addEventListener('focusout', event => {
		const field = event.target.parentNode;
		if (field && field.classList && field.classList.contains('ui-input-text')) {
			field.classList.remove('ui-focus');
		}
	});

	// Pages

	let changing = null;

	// Fetches a page, builds it and shows it in place of the one before. As with jQuery Mobile, the loading box
	// shows if it takes a while, and says so if it fails.
	function changePage(url) {
		const slow = setTimeout(() => loading(true), 50);
		const request = changing = fetch(url).then(response => {
			if (!response.ok) {
				throw new Error(url + ': ' + response.status);
			}
			return response.text();
		}).then(html => {
			if (request != changing) {
				return;	// Another page was asked for since
			}
			const template = make('template');
			template.innerHTML = html;
			const page = template.content.querySelector('[data-role=page]');
			const old = doc.querySelector('.ui-page-active');
			page.classList.add('ui-page', 'ui-page-theme-a');
			page.tabIndex = 0;
			// Scripts in a page run when it's added, in order
			const scripts = all('script', page);
			doc.body.appendChild(page);
			return scripts.reduce((ready, script) => ready.then(() => new Promise(resolve => {
				const run = make('script');
				if (script.src) {
					run.src = script.src;
					run.onload = run.onerror = resolve;
				} else {
					run.textContent = script.textContent;
				}
				script.parentNode.replaceChild(run, script);
				if (!script.src) {
					resolve();
				}
			})), Promise.resolve()).then(() => {
				enhance(page);
				doc.dispatchEvent(new CustomEvent('pagecreate', { detail: page }));
				if (old) {
					old.classList.remove('ui-page-active');
					if (old.uiLoaded) {
						old.remove();
					}
				}
				pickers = pickers.filter(box => page.contains(box.uiSwatch) || box.remove());
				fieldFocused = false;
				page.uiLoaded = true;
				page.classList.add('ui-page-active');
				fitHeader(page);
				scrollTo(0, 0);
				page.focus({ preventScroll: true });
				animate(page, 'fade in');
				doc.dispatchEvent(new CustomEvent('pagechange', { detail: page }));
			});
		}).catch(error => {
			console.log(error);
			loading(true, 'Error Loading Page');
			setTimeout(() => loading(false), 1500);
		}).finally(() => {
			clearTimeout(slow);
			if (!doc.querySelector('.ui-loader-verbose')) {
				loading(false);
			}
		});
		return request;
	}

	// A fixed header is over the page, which starts below it. The header is up by its top, -1px. The page is at
	// least as tall as the window.
	function fitHeader(page) {
		if (!page) {
			return;
		}
		const bar = page.querySelector('.ui-header-fixed');
		let top = 0;
		if (bar) {
			top = bar.offsetHeight + (parseFloat(getComputedStyle(bar).top) || 0);
			page.style.paddingTop = top + 'px';
		}
		page.style.minHeight = (innerHeight - top) + 'px';
	}
	addEventListener('resize', () => fitHeader(doc.querySelector('.ui-page-active')));

	// Links that open the menu or a pop-up, close them, or go to a page from the menu
	doc.addEventListener('click', event => {
		const link = event.target.closest('a');
		if (!link || event.defaultPrevented) {
			return;
		}
		const href = link.getAttribute('href') || '';
		const rel = data(link, 'rel');
		if (href == '#mainMenu') {
			event.preventDefault();
			panelOpen(true);
		} else if (rel == 'popup' && href.length > 1 && href[0] == '#') {
			event.preventDefault();
			popupOpen(href.slice(1));
		} else if (rel == 'back') {
			event.preventDefault();
			const box = link.closest('.ui-popup');
			if (box) {
				popupClose(box);
			}
		} else if (rel == 'close' || (menu && menu.contains(link) && href && href[0] != '#')) {
			event.preventDefault();
			panelOpen(false);
			if (href && href[0] != '#') {
				changePage(href);
			}
		}
	});
	// A button in a pop-up whose data-rel is back closes it after its own onclick
	doc.addEventListener('click', event => {
		const input = event.target.closest('input[data-rel=back]');
		const box = input && input.closest('.ui-popup');
		if (box) {
			popupClose(box);
		}
	});

	doc.addEventListener('keydown', event => {
		if (event.key == 'Escape') {
			if (openPopup) {
				popupClose(openPopup);
			} else {
				panelOpen(false);
			}
		}
	});

	// Enter in a field doesn't send its form anywhere
	doc.addEventListener('submit', event => event.preventDefault());

	doc.documentElement.classList.add('ui-mobile');
	const start = () => {
		doc.body.classList.add('ui-mobile-viewport', 'ui-overlay-a');
		all('[data-role=page]').forEach(page => {
			page.classList.add('ui-page', 'ui-page-theme-a');
			page.tabIndex = 0;
		});
		enhance(doc.body);
		const first = doc.querySelector('[data-role=page]');
		if (first && !doc.querySelector('.ui-page-active')) {
			first.classList.add('ui-page-active');
			fitHeader(first);
		}
	};
	if (doc.readyState == 'loading') {
		doc.addEventListener('DOMContentLoaded', start);
	} else {
		start();
	}

	return { enhance, refresh, changePage, popupOpen, popupClose, panelOpen, loading, colorPicker, hourRing };
})();
