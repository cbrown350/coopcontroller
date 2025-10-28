[](https://github.com/cbrown350/)


Firmware Installation
Flash the firmware and filesystem, this can be done through the web tool
Once it's flashed, it will create a WiFi network called CoopController, connect to it with the password coopycontroller
Go to http://192.168.4.1 in your browser to load the user interface
Enter your wifi ssid, password and hit "save settings", the device will restart and connect to your network.
Access the web UI at anytime by going to http://coopcontroller.local


Web UI
Web UI code is a SolidJS app with vite in the /web folder, it comes with a mock server. Just run `npm i && npm run dev` in the web folder. Use `npm i --include=dev`, `npm run build` in the /web folder to copy code into the /data folder, followed by Upload file sytem image command from PlatformIO



34 - temp/meter
35 - temp/meter
39 - out pump
36 - out light