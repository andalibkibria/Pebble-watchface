# Classic Analog Watchface

A clean, cross-platform analog watchface for Pebble with date, battery, Bluetooth, and live weather.

## Features

| Feature | Details |
|---|---|
| **Time** | Analog hands — hour, minute, second with counterweights |
| **Date** | Abbreviated day + date (e.g. `Sat 28`) |
| **Battery** | Percentage or `CHG` when charging |
| **Bluetooth** | Coloured dot — green = connected, red = disconnected (+ vibrate on drop) |
| **Weather** | Temperature + short condition via Open-Meteo (no API key needed) |

## Layout

```
         [BT dot]
    ╭──────────────────╮
    │   •  BT dot      │
    │                  │
    │  [batt%]         │  ← right of center
    │       ╱╲         │
    │      ╱  ╲        │
    │──── ─────────────│
    │      ╲  ╱        │
    │       ╲╱  [date] │  ← right of center
    │                  │
    │   72° Clear      │  ← bottom strip
    ╰──────────────────╯
```

## Platform Notes

- **Aplite** (Pebble OG / Pebble Steel): black & white; second hand in light gray
- **Basalt** (Pebble Time / Pebble Time Steel): full color; red second hand
- **Chalk** (Pebble Time Round): full color; round screen auto-handled by SDK

## Weather

Weather is fetched from **[Open-Meteo](https://open-meteo.com/)** — a free, open-source weather API that requires **no API key**.

- Updates automatically every 30 minutes
- Refreshes on app launch
- Displays in °F for `en-US` locale, °C otherwise
- Requires **Location** permission on the phone (granted on first launch)

## Build Instructions

### Prerequisites

```bash
# Install Pebble SDK (macOS / Linux)
brew install pebble/pebble-sdk/pebble-sdk   # macOS
# or follow https://developer.rebble.io/developer.pebble.com/sdk/install/
```

### Build

```bash
cd pebble-watchface
pebble build
```

### Install on watch (over phone)

```bash
pebble install --phone <YOUR_PHONE_IP>
```

Find your phone's IP in the Pebble app → Settings → Developer Mode.

### Emulator

```bash
pebble build && pebble install --emulator basalt   # or aplite / chalk
```

## Customisation Tips

| What to change | Where |
|---|---|
| Hand lengths | `hour_len`, `min_len`, `sec_len` in `main.c` |
| Hand thickness | `HAND_HOUR_THICKNESS` etc. constants at top of `main.c` |
| Colour scheme | `#ifdef PBL_COLOR` block in `prv_canvas_update()` |
| Weather refresh rate | `WEATHER_UPDATE_INTERVAL_MS` in `pebble-js-app.js` |
| Temperature unit | Overridable in `pebble-js-app.js` (force `useFahrenheit = true/false`) |

## File Structure

```
pebble-watchface/
├── appinfo.json          # App metadata + keys
├── wscript               # Build script
├── README.md
└── src/
    ├── main.c            # Watchface logic (C)
    └── pebble-js-app.js  # Companion JS (weather fetch)
```

## License

MIT — do whatever you like with it.
