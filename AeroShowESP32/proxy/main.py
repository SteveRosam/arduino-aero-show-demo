"""
Create a Flask API with CORS support that proxies POST requests to a configurable remote API endpoint
"""

import os
from flask import Flask, request, jsonify
from flask_cors import CORS
import requests
from dotenv import load_dotenv
import logging

# Load environment variables
load_dotenv()

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Create Flask app
app = Flask(__name__)

# Enable CORS for all origins
CORS(app, resources={r"/*": {"origins": "*"}})

# Get remote API URL from environment variable
REMOTE_API_URL = os.getenv('REMOTE_API_URL', 'https://gateway-quixers-aerospacedemoingestionpipeline-dev.az-france-0.app.quix.io/data/')

# Ensure the remote URL ends with a slash
if REMOTE_API_URL and not REMOTE_API_URL.endswith('/'):
    REMOTE_API_URL += '/'

@app.route('/')
def home():
    return jsonify({
        "status": "running",
        "message": "ESP32 Data Proxy API",
        "endpoints": {
            "POST /data/<test_id>": "Relay data to remote API"
        }
    })

@app.route('/data/<test_id>', methods=['POST'])
def proxy_data(test_id):
    """
    Proxy endpoint that receives POST data and forwards it to the remote API
    """
    try:
        # Get the incoming data
        data = request.get_json()
        
        # Log the incoming request
        logger.info(f"Received POST request for test_id: {test_id}")
        logger.info(f"Data size: {len(str(data)) if data else 0} bytes")
        
        # Construct the remote URL
        remote_url = f"{REMOTE_API_URL}{test_id}"
        
        # Forward the request to the remote API
        headers = {
            'Content-Type': 'application/json'
        }
        
        # Make the request to the remote API
        response = requests.post(remote_url, json=data, headers=headers)
        
        # Log the response
        logger.info(f"Remote API response: {response.status_code}")
        
        # Return the remote API's response
        return jsonify({
            "status": "success",
            "remote_status_code": response.status_code,
            "remote_response": response.text,
            "test_id": test_id
        }), response.status_code
        
    except requests.exceptions.RequestException as e:
        logger.error(f"Error forwarding request: {str(e)}")
        return jsonify({
            "status": "error",
            "message": f"Failed to forward request: {str(e)}",
            "test_id": test_id
        }), 502  # Bad Gateway
        
    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}")
        return jsonify({
            "status": "error",
            "message": f"Internal server error: {str(e)}",
            "test_id": test_id
        }), 500

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        "status": "healthy",
        "remote_api_configured": bool(REMOTE_API_URL)
    })

if __name__ == '__main__':
    # Get port from environment or default to 5000
    port = int(os.getenv('PORT', 5000))
    
    # Log startup information
    logger.info(f"Starting proxy server on port {port}")
    logger.info(f"Remote API URL: {REMOTE_API_URL}")
    
    # Run the Flask app
    app.run(host='0.0.0.0', port=port, debug=True)