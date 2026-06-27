// pebble-js-app.js  — runs in the Pebble phone app (PebbleKit JS)
// Fetches weather from Open-Meteo (free, no API key required) and
// sends temperature + conditions text to the watch.

// ── Config ───────────────────────────────────────────────────────────────────

var WEATHER_UPDATE_INTERVAL_MS = 30 * 60 * 1000;  // 30 minutes

// WMO weather-code → short description mapping (subset)
var WMO_CODES = {
  0:  'Clear',
  1:  'Mostly Clear', 2: 'Partly Cloudy', 3: 'Overcast',
  45: 'Foggy',        48: 'Icy Fog',
  51: 'Lt Drizzle',   53: 'Drizzle',    55: 'Hvy Drizzle',
  61: 'Lt Rain',      63: 'Rain',       65: 'Hvy Rain',
  71: 'Lt Snow',      73: 'Snow',       75: 'Hvy Snow',
  77: 'Snow Grains',
  80: 'Showers',      81: 'Showers',    82: 'Hvy Showers',
  85: 'Snow Shower',  86: 'Hvy Snow Shower',
  95: 'Thunderstorm', 96: 'T-Storm Hail', 99: 'T-Storm Hail'
};

// ── State ─────────────────────────────────────────────────────────────────────

var lastWeatherUpdate = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

function describeWMO(code) {
  return WMO_CODES[code] || 'Unknown';
}

function celsiusToFahrenheit(c) {
  return Math.round(c * 9 / 5 + 32);
}

function fetchWeather() {
  // Use Geolocation API if available, otherwise skip
  if (!navigator.geolocation) {
    console.warn('Geolocation not available');
    return;
  }

  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);

      // Open-Meteo: free, no API key
      var url = 'https://api.open-meteo.com/v1/forecast'
        + '?latitude='  + lat
        + '&longitude=' + lon
        + '&current_weather=true'
        + '&temperature_unit=celsius';

      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.onload = function() {
        if (req.status === 200) {
          try {
            var data    = JSON.parse(req.responseText);
            var current = data.current_weather;
            if (!current || current.temperature === undefined) {
              console.error('Unexpected API response shape');
              return;
            }
            var tempC   = Math.round(current.temperature);
            var code    = current.weathercode;
            var desc    = describeWMO(code);

            // Determine unit preference from phone locale
            var useFahrenheit = (Intl.DateTimeFormat().resolvedOptions()
                                    .locale.indexOf('en-US') === 0);
            var displayTemp = useFahrenheit ? celsiusToFahrenheit(tempC) : tempC;

            console.log('Weather: ' + displayTemp + '° ' + desc);

            Pebble.sendAppMessage(
              { '0': displayTemp, '1': desc, '2': useFahrenheit ? 1 : 0 },
              function() { console.log('Weather sent to watch'); },
              function(e) { console.error('Send failed: ' + JSON.stringify(e)); }
            );

            lastWeatherUpdate = Date.now();
          } catch (ex) {
            console.error('Parse error: ' + ex);
          }
        } else {
          console.error('HTTP error: ' + req.status);
        }
      };
      req.onerror = function() { console.error('Network error'); };
      req.send();
    },
    function(err) {
      console.error('Geolocation error: ' + err.message);
    },
    { timeout: 15000 }
  );
}

// ── PebbleKit JS lifecycle ────────────────────────────────────────────────────

Pebble.addEventListener('ready', function() {
  console.log('PebbleKit JS ready');
  fetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  // Watch requested a weather refresh
  console.log('Watch requested weather update');
  var now = Date.now();
  if (now - lastWeatherUpdate > 60000) {   // debounce: at most once/minute
    fetchWeather();
  }
});
