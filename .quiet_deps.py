Import("env")

# Third-party sources that emit warnings we can't fix in this repo.
# Each is compiled with -w; everything else keeps its warnings.
QUIET_SOURCES = [
    # framework-arduinoespressif32 2.0.14: 'return' with no value in uartSetPins()
    "*/cores/esp32/esp32-hal-uart.c",
    # ESPConfig: char* from string literal, DEBUG redefined
    "*/ESPConfig/ConfigItem.cpp",
    "*/ESPConfig/EEPROMConfig.cpp",
    # TimeSync: DEBUG redefined, plus ESPAsyncHTTPClient.h #endif label
    "*/TimeSync/TimeServerTimeSync.cpp",
    # ESPAsyncHttpClient: extra tokens at end of #endif
    "*/ESPAsyncHttpClient/ESPAsyncHTTPClient.cpp",
    # ESP Async WebServer, AsyncMqttClient, espMqttClient: AsyncClient::close(bool) is deprecated in AsyncTCP 3.x
    "*/ESP Async WebServer/src/AsyncEventSource.cpp",
    "*/ESP Async WebServer/src/AsyncWebSocket.cpp",
    "*/ESP Async WebServer/src/WebRequest.cpp",
    "*/ESP Async WebServer/src/WebResponses.cpp",
    "*/ESP Async WebServer/src/WebServer.cpp",
    "*/AsyncMqttClient/src/AsyncMqttClient.cpp",
    "*/espMqttClient/src/Transport/ClientAsync.cpp",
]

def quiet(env, node):
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-w"])

for pattern in QUIET_SOURCES:
    env.AddBuildMiddleware(quiet, pattern)
