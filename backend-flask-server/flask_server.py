from flask import Flask, jsonify
import os
import datetime
import google.oauth2.credentials
import google_auth_oauthlib.flow
import googleapiclient.discovery
import json

app = Flask(__name__)

CREDENTIALS_FILE = "credentials.json" #The credentials file provided by GCloud, IMPORTANT
TOKEN_FILE = "token.json"  # File to store access tokens

# Function to get Google Calendar API service
def get_calendar_service():
    creds = None

    # Load saved credentials if available
    if os.path.exists(TOKEN_FILE):
        with open(TOKEN_FILE, "r") as token:
            creds_data = json.load(token)
            creds = google.oauth2.credentials.Credentials.from_authorized_user_info(creds_data)

    # If no credentials or expired, re-authenticate
    if not creds or not creds.valid:
        flow = google_auth_oauthlib.flow.InstalledAppFlow.from_client_secrets_file(
            CREDENTIALS_FILE, ["https://www.googleapis.com/auth/calendar.readonly"]
        )
        creds = flow.run_local_server(port=0)

        # Save credentials for next time
        with open(TOKEN_FILE, "w") as token:
            token.write(creds.to_json())

    return googleapiclient.discovery.build("calendar", "v3", credentials=creds)

@app.route("/events", methods=["GET"])
def get_events():
    service = get_calendar_service()
    now = datetime.datetime.utcnow()
    start_of_day = now.replace(hour=0, minute=0, second=0, microsecond=0).isoformat() + "Z"
    end_of_day = now.replace(hour=23, minute=59, second=59, microsecond=0).isoformat() + "Z"

    events_result = service.events().list(
        calendarId="primary",
        timeMin=start_of_day,
        timeMax=end_of_day,
        maxResults=10,
        singleEvents=True,
        orderBy="startTime"
    ).execute()

    events = events_result.get("items", [])
    return jsonify(events)

@app.route("/", methods=["GET"])
def home():
    return "✅ Flask Server is Running! Go to /events to see today's calendar events."


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
