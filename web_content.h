#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Bluetooth Device Selection</title>
    <style>
        body { 
            font-family: Arial; 
            margin: 0; 
            padding: 20px;
            background: #f5f5f5;
        }
        .device-list { 
            max-width: 600px; 
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            padding: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .device-item {
            padding: 15px;
            margin: 10px 0;
            background: #f0f0f0;
            border-radius: 5px;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        .device-item:hover { 
            background: #e0e0e0;
            transform: translateY(-2px);
        }
        h2 {
            color: #333;
            text-align: center;
            margin-bottom: 20px;
        }
        .loading {
            text-align: center;
            color: #666;
            padding: 20px;
        }
        .message {
            text-align: center;
            color: #666;
            padding: 20px;
            line-height: 1.5;
            background: #fff3cd;
            border-radius: 5px;
            margin: 10px 0;
        }
        .button {
            display: block;
            width: 100%;
            padding: 15px;
            margin: 20px 0;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            transition: all 0.3s ease;
        }
        .button:hover {
            background: #45a049;
            transform: translateY(-2px);
        }
        .button:disabled {
            background: #cccccc;
            cursor: not-allowed;
        }
    </style>
</head>
<body>
    <div class="device-list">
        <h2>Available Bluetooth Devices</h2>
        <button id="refreshBtn" class="button">Rescan Devices</button>
        <div id="deviceList">
            <div class="loading">Loading device list...</div>
        </div>
    </div>
    <a href="/update">Firmware Update</a>

    <script>
        let isScanning = false;

        async function loadDevices() {
            try {
                const response = await fetch('/api/devices');
                const data = await response.json();
                const deviceList = document.getElementById('deviceList');
                deviceList.innerHTML = '';
                
                if (data.status === 'empty') {
                    const message = document.createElement('div');
                    message.className = 'message';
                    message.textContent = data.message;
                    deviceList.appendChild(message);
                } else if (data.status === 'ok' && data.devices) {
                    data.devices.forEach(device => {
                        const div = document.createElement('div');
                        div.className = 'device-item';
                        div.textContent = device;
                        div.onclick = () => selectDevice(device);
                        deviceList.appendChild(div);
                    });
                }
            } catch (error) {
                console.error('Error loading devices:', error);
                const deviceList = document.getElementById('deviceList');
                deviceList.innerHTML = '<div class="message">Failed to load device list, please refresh the page and try again</div>';
            }
        }

        async function rescan() {
            if (isScanning) return;
            
            const refreshBtn = document.getElementById('refreshBtn');
            refreshBtn.disabled = true;
            refreshBtn.textContent = 'Scanning...';
            
            try {
                const response = await fetch('/api/rescan', {
                    method: 'POST'
                });
                const result = await response.json();
                if (result.status === 'ok') {
                    setTimeout(loadDevices, 10000); // wait for scanning to complete and refresh list
                }
            } catch (error) {
                console.error('Error starting rescan:', error);
            } finally {
                setTimeout(() => {
                    refreshBtn.disabled = false;
                    refreshBtn.textContent = 'Rescan Devices';
                }, 6000);
            }
        }

        async function selectDevice(name) {
            try {
                const response = await fetch('/api/select', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ device: name })
                });
                const result = await response.json();
                if (result.status === 'ok') {
                    alert('Device selected: ' + name);
                    const deviceList = document.getElementById('deviceList');
                    deviceList.innerHTML = '<div class="message">Device selection successful, please wait for reconnection...</div>';
                    document.getElementById('refreshBtn').style.display = 'none';
                } else {
                    throw new Error(result.message || 'Device selection failed');
                }
            } catch (error) {
                console.error('Error selecting device:', error);
                alert('Device selection failed: ' + error.message);
            }
        }

        document.getElementById('refreshBtn').onclick = rescan;
        loadDevices();
    </script>
</body>
</html>
)rawliteral";

#endif 