# xdialtester

Native application to test DIAL functionality in RDK environment without an app manager or system UI. The implementation is based on XCast plugin and RDKShell for supporting app state management and reporting. By default YouTube, Netflix are supported based on `Cobalt` and `Netflix` thunder plugin `callsign` availability.

## Features

- **Real-time App Monitoring**: Tracks application lifecycle events (launch, suspend, resume, terminate) with the help of `RDKShell` plugin events.
- **Debug Logging**: Comprehensive logging with color-coded output along with option to enable extended debug logs.
- **JSON Configuration**: Override option for app settings using optional `/opt/appConfig.json` - specifying app specific deeplink method and baseurl.

## Build

### Dependencies
- boost
- websocketpp
- jsoncpp
- cmake (for building)

### Yocto Build
If you are building using Yocto, use this command to checkout:

```bash
devtool add --autorev xdialtester https://github.com/joseinweb/xdialtester.git --srcbranch develop
```

Add the following line in the recipe:
```bash
DEPENDS += "jsoncpp websocketpp systemd boost"
```

## Usage

### Command Line Options

| Option | Description | Example |
|--------|-------------|---------|
| `--enable-apps=<apps>` | Comma-separated list of apps | `--enable-apps=YouTube,Netflix,Amazon` |
| `--enable-debug` | Enable detailed debug logging | `--enable-debug` |

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `SMDEBUG` | Enable debug mode (alternative to --enable-debug) | Not set |

### Basic Usage

Preconditions: Activate required plugins before running this test app.
```bash
curl -X POST http://127.0.0.1:9998/jsonrpc -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.Xcast"}}'
curl -X POST http://127.0.0.1:9998/jsonrpc -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.RDKShell"}}'
```

```bash
# Run with default apps (YouTube, Netflix, Amazon)
./xdialtester

# Specify only required app
./xdialtester --enable-apps=YouTube

# Enable debug logging
./xdialtester --enable-debug

# Combination
./xdialtester --enable-apps=Netflix --enable-debug
```

## Configuration

### App Configuration File (`/opt/appConfig.json`)

This can intake an optional app configuration `/opt/appConfig.json` on startup. This JSON file provides an option to adjust the app's base url and deeplink method. Default settings are as below
```
"YouTube" - "https://www.youtube.com/tv" and "Cobalt.1.deeplink"
"Netflix" - "https://www.netflix.com" and "Netflix.1.systemcommand"
"Amazon" - "https://www.amazon.com/gp/video" and "PrimeVideo.1.deeplink"
```

#### File Location and Permissions
- **Path**: `/opt/appConfig.json`
- **Owner**: Should be readable by the user running xdialtester
- **Format**: Valid JSON with UTF-8 encoding

#### Sample appConfig.json Structure
```json
{
  "appConfig": [
    {
      "name": "YouTube",
      "baseurl": "https://www.youtube.com/tv",
      "deeplinkmethod": "Cobalt.1.deeplink"
    },
    {
      "name": "Netflix",
      "baseurl": "https://www.netflix.com",
      "deeplinkmethod": "Netflix.1.systemcommand"
    },
    {
      "name": "Amazon",
      "baseurl": "https://www.amazon.com/gp/video",
      "deeplinkmethod": "PrimeVideo.1.deeplink"
    }
  ]
}
```

#### Details of Configuration Fields of `/opt/appConfig.json`

| Field | Type | Required | Description | Example |
|-------|------|----------|-------------|---------|
| `name` | String | Yes | Application identifier used in DIAL requests and command line | `"YouTube"`, `"Netflix"` |
| `baseurl` | String | Yes | Base URL for deep linking. Can include query parameters | `"https://www.youtube.com/tv"` |
| `deeplinkmethod` | String | Optional | Thunder method name for sending deep links | `"Cobalt.1.deeplink"` |

### Log Levels
The application uses color-coded logging:
- 🔴 **ERROR** (RED): Critical errors
- 🟠 **WARN** (ORANGE): Warnings
- 🟢 **TRACE** (GREEN): Debug trace information
- ⚪ **INFO** (DEFAULT): General information

## Testing DIAL Functionality

The xdialtester works as a DIAL client that communicates with the RDK Thunder framework through the `org.rdk.Xcast` plugin.

### Prerequisites
1. Ensure the mobile app and DUT are on the same network
2. Ensure Thunder framework is running on your device
3. Set device IP environment variable:
```bash
export DEVICEIP=192.168.1.100  # Replace with your device's IP address
```

3. Activate required plugins:
```bash
# Activate XCast plugin (handles DIAL protocol)
curl -X POST http://$DEVICEIP:9998/jsonrpc -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.Xcast"}}'

# Activate RDKShell plugin (handles app lifecycle)
curl -X POST http://$DEVICEIP:9998/jsonrpc -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.RDKShell"}}'

# Verify plugins are active
curl -X POST http://$DEVICEIP:9998/jsonrpc -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.status"}'

# Find the correct DIAL port (common ports: 56889 for REST, 56890 for SSDP)
netstat -tlnp | grep -E ":(56889|56890|56789)"
```

3. Start xdialtester application:
```bash
./xdialtester --enable-apps=YouTube,Netflix --enable-debug
```

4. Launch the Second screen mobile application to test the casting functionality on First screen.
