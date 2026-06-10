#pragma once

inline const char *getConfigPageHtml() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Blind Assistant Settings</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f4f6f8;
      --panel: #ffffff;
      --text: #1f2933;
      --muted: #667085;
      --line: #d0d5dd;
      --accent: #2563eb;
      --accent-hover: #1d4ed8;
      --success-bg: #ecfdf3;
      --success-text: #027a48;
    }

    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: Arial, Helvetica, sans-serif;
      background: linear-gradient(180deg, #eef2ff 0%, var(--bg) 42%, #eef2ff 100%);
      color: var(--text);
      padding: 24px;
    }

    .container {
      max-width: 720px;
      margin: 0 auto;
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 16px;
      padding: 24px;
      box-shadow: 0 18px 50px rgba(15, 23, 42, 0.08);
    }

    h1, h2 { margin: 0 0 16px; }
    h1 { font-size: 28px; }
    h2 { font-size: 18px; margin-top: 24px; }

    .info {
      background: #f8fafc;
      border: 1px solid var(--line);
      border-radius: 12px;
      padding: 12px 14px;
      color: var(--muted);
      margin-bottom: 20px;
    }

    .success {
      display: none;
      margin-bottom: 16px;
      padding: 12px 14px;
      border-radius: 12px;
      background: var(--success-bg);
      color: var(--success-text);
      border: 1px solid #abefc6;
    }

    .form-group { margin-bottom: 16px; }
    label {
      display: block;
      margin-bottom: 6px;
      font-weight: 700;
      color: var(--text);
    }

    input[type="text"], input[type="number"] {
      width: 100%;
      padding: 11px 12px;
      border: 1px solid var(--line);
      border-radius: 10px;
      font-size: 15px;
      outline: none;
    }

    input[readonly] {
      background: #f8fafc;
      color: var(--muted);
    }

    input:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.12);
    }

    button {
      width: 100%;
      padding: 12px 14px;
      margin-top: 10px;
      border: 0;
      border-radius: 10px;
      font-size: 15px;
      font-weight: 700;
      cursor: pointer;
    }

    .primary {
      background: var(--accent);
      color: white;
    }

    .primary:hover { background: var(--accent-hover); }

    .secondary {
      background: #e5eefc;
      color: #1d4ed8;
    }

    .secondary:hover { background: #dbe7fb; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Smart Blind Settings</h1>
    <div class="info">
      WiFi connected | IP: <span id="ip">Loading...</span>
    </div>

    <div class="success" id="success">Settings updated successfully.</div>

    <form id="configForm">
      <h2>Server Configuration</h2>
      <div class="form-group">
        <label for="serverUrl">Server URL</label>
        <input type="text" id="serverUrl" readonly>
      </div>

      <h2>Mode Settings</h2>
      <div class="form-group"><label for="mode0">Mode 1</label><input type="number" id="mode0"></div>
      <div class="form-group"><label for="mode1">Mode 2</label><input type="number" id="mode1"></div>
      <div class="form-group"><label for="mode2">Mode 3</label><input type="number" id="mode2"></div>
      <div class="form-group"><label for="mode3">Mode 4</label><input type="number" id="mode3"></div>
      <div class="form-group"><label for="mode4">Mode 5</label><input type="number" id="mode4"></div>

      <h2>WiFi Management</h2>
      <button type="button" class="secondary" onclick="resetWiFi()">Reset WiFi Settings</button>
      <button type="submit" class="primary">Save Settings</button>
    </form>
  </div>

  <script>
    document.getElementById('ip').textContent = window.location.hostname;

    window.onload = function() {
      fetch('/api/config')
        .then(function(response) { return response.json(); })
        .then(function(data) {
          document.getElementById('serverUrl').value = data.serverUrl || '';
          document.getElementById('mode0').value = data.modes[0] || 0;
          document.getElementById('mode1').value = data.modes[1] || 1;
          document.getElementById('mode2').value = data.modes[2] || 2;
          document.getElementById('mode3').value = data.modes[3] || 3;
          document.getElementById('mode4').value = data.modes[4] || 4;
        });
    };

    document.getElementById('configForm').onsubmit = function(event) {
      event.preventDefault();

      const data = {
        modes: [
          parseInt(document.getElementById('mode0').value, 10) || 0,
          parseInt(document.getElementById('mode1').value, 10) || 1,
          parseInt(document.getElementById('mode2').value, 10) || 2,
          parseInt(document.getElementById('mode3').value, 10) || 3,
          parseInt(document.getElementById('mode4').value, 10) || 4
        ]
      };

      fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
      })
      .then(function(response) { return response.json(); })
      .then(function() {
        document.getElementById('success').style.display = 'block';
        setTimeout(function() {
          document.getElementById('success').style.display = 'none';
        }, 3000);
      });
    };

    function resetWiFi() {
      if (confirm('Reset WiFi settings? Device will restart.')) {
        fetch('/api/reset-wifi', { method: 'POST' })
          .then(function() { alert('Resetting WiFi settings...'); });
      }
    }
  </script>
</body>
</html>
  )rawliteral";
}