# A Custom Firmware for the EleksTube IPS V1 clock

## Features of This Firmware

This firmware uses the WiFi support that your clock already has to sync the time with the internet and to provide an easy way to configure the clock and change the display:

* It can display the time, the date or a weather forecast.
* It synchronizes the time with NTP so it is very accurate.
* It automatically handles timezone and DST conversions.
* All clock configurarion is done using a web interface.
  * Set the timezone information
  * Set the hours the clock is on
  * Switch between a six digit clock showing seconds and a four digit AM/PM display
  * Enable/disable leading zeros
  * Switch the display between time, date and the weather
  * Set the date format
  * Set the backlight effects and colors
  * Upload, delete and select the images used for the clock display
  * Configure the weather forecast
  * Upload, delete and select the icons used for the weather display
  * Upload, delete and select images for slide show mode
* Clock faces and weather icons can be selected using the buttons.
* You can switch between time, date, weather and slide show using the buttons.
* You can automatically cycle through display modes.
* It has a screen saver.
* There is also a tool to convert images in to a format that can be displayed on the clock
* It can connect to a MQTT broker, primarily to allow use of movement and luminance sensors, setting a custom display value and controlling all aspects of the clock - including uploading image packs. See the [User Guide](https://github.com/judge2005/EleksTubeIPS/wiki/User-Guide#mqtthome-assistant) for details.
* you can also set custom display values using curl
* There are specific variants for these clocks:
  * EleksTube v1
  * Si Hai
  * NovelLife SE
  * PCBWay 'Punkcyber'
  * IPSTube

This image shows the clock running in 4 digit mode with weather forecast

![Captive Portal](docs/original.jpg)

This shows a weather forecast using colored icons:

![Captive Portal](docs/weather_display.jpg)

This shows the digital rain screen saver:

![Captive Portal](docs/matrix.jpg)

## Documentation

See [the wiki](https://github.com/judge2005/EleksTubeIPS/wiki "wiki") for detailed information on how to build, install and use this firmware.

* [Installation](https://github.com/judge2005/EleksTubeIPS/wiki/Installation) takes you through backing up the original firmware, building the custom firmware and uploading it to the clock
* [Connecting to WiFi](https://github.com/judge2005/EleksTubeIPS/wiki/Connecting-to-WiFi) takes you through getting your clock connected to your router so it can sync the time with NTP and you can configure it using a web browser.
  * On an iPhone, the captive portal often doesn't open by itself when you join the clock's network. iOS only opens it when the Wi-Fi list in Settings has just appeared, so tap ⓘ next to the clock's network and go back, and it opens. Or open Safari and go to 192.168.4.1, the setup page address the clock shows along with the network name and password.
* [User Guide](https://github.com/judge2005/EleksTubeIPS/wiki/User-Guide) shows you how to configure and control your clock.
* [Image Collections](https://github.com/judge2005/EleksTubeIPS/wiki/Image-Collections) tells you how to prepare new clock faces and weather icon sets so that they can be uploaded to the clock and used.
* [Hardware Modification](https://github.com/judge2005/EleksTubeIPS/wiki/Hardware-Modification) details a mod that you might want to make to your clock, but only if you are comfortable wielding a soldering iron.

## Credits

[Original documentation and software from EleksMaker.](https://wiki.eleksmaker.com/doku.php?id=ips)

This code wouldn't exist without the work done by the people on [this reddit discussion](https://www.reddit.com/r/arduino/comments/mq5td9/hacking_the_elekstube_ips_clock_anyone_tried_it/).
