# ESP32 Data Proxy API

This Flask API acts as a proxy between the ESP32 device and the remote Quix API, handling CORS and providing a local endpoint for testing.

## Features

- **CORS Support**: Accepts requests from any origin
- **Data Relay**: Forwards POST requests to the configured remote API
- **Configurable**: Remote API URL can be set via environment variables
- **Logging**: Provides detailed logging of requests and responses

## Setup

1. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

2. **Configure environment**:
   - Copy `.env.template` to `.env`
   - Update `REMOTE_API_URL` if needed
   ```bash
   cp .env.template .env
   ```

3. **Run the server**:
   ```bash
   python main.py
   ```

## API Endpoints

### POST /data/<test_id>
Receives data from ESP32 and forwards it to the remote API.

**Example**:
```bash
curl -X POST http://localhost:5000/data/test123 \
  -H "Content-Type: application/json" \
  -d '{"data": [{"timestamp": 1234567890, "voltage": 12.5}]}'
```

### GET /
Returns API status and available endpoints.

### GET /health
Health check endpoint.

## Configuration

Environment variables (set in `.env`):
- `REMOTE_API_URL`: The remote API URL to forward requests to (default: Quix API)
- `PORT`: Port to run the proxy server on (default: 5000)

## ESP32 Configuration

Update your ESP32's `DATA_URL` in the `.env` file to point to this proxy:
```
DATA_URL=http://YOUR_COMPUTER_IP:5000/data
```

Replace `YOUR_COMPUTER_IP` with your computer's IP address on the same network as the ESP32.
