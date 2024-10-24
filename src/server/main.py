from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import List, Optional
import logging
import json
import os

app = FastAPI()

# Configure logging
logging.basicConfig(level=logging.INFO)

# In-memory storage
current_config = {
    "knockThreshold": 100,
    "knockFadeTime": 150,
    "defaultKnockTimeout": 1200,
    "isProgrammingMode": False
}




# Stores the latest knock data from the ESP32
latest_knock_data = None
# file path where to store the knock patterns
PATTERNS_FILE = 'knock_patterns.json'

class KnockData(BaseModel):
    timeout: int
    knock_sequence: List[int]

class ConfigData(BaseModel):
    knockThreshold: Optional[int] = Field(None, ge=0)
    knockFadeTime: Optional[int] = Field(None, ge=0)
    defaultKnockTimeout: Optional[int] = Field(None, ge=0)
    isProgrammingMode: Optional[bool] = None

class CompareKnocksInput(BaseModel):
    knock1: List[int]
    knock2: List[int]
    deviation: float

class KnockPattern(BaseModel):
    id: int
    pattern: List[int]
    
def load_patterns():
    if os.path.exists(PATTERNS_FILE):
        with open(PATTERNS_FILE, 'r') as f:
            return json.load(f)
    else:
        # Just for testing: Store predefined knock patterns
        initial_patterns = [
            {"id": 1, "pattern": [50, 30, 450]},
            {"id": 2, "pattern": [60, 40, 500]}
        ]
        with open(PATTERNS_FILE, 'w') as f:
            json.dump(initial_patterns, f)
        return initial_patterns

def save_patterns():
    with open(PATTERNS_FILE, 'w') as f:
        json.dump(knock_patterns, f)

knock_patterns = load_patterns()

@app.post("/knock")
def handle_knock(data: KnockData):
    global latest_knock_data
    latest_knock_data = data.knock_sequence
    logging.info("Received knock data from ESP32: %s", latest_knock_data)

    # You can perform validation or processing here
    # For now, we'll just acknowledge the receipt

    # Return the current configuration to the ESP32
    response = {
        "message": "Knock data received",
        "config": current_config
    }
    return response

@app.get("/config")
def get_config():
    return current_config

@app.post("/config")
def update_config(config: ConfigData):
    # Update the current configuration with provided values
    for key, value in config.dict(exclude_unset=True).items():
        current_config[key] = value
    return {"message": "Configuration updated", "config": current_config}

@app.get("/latest_knock")
def get_latest_knock():
    if latest_knock_data is not None:
        return {"knock_sequence": latest_knock_data}
    else:
        return {"message": "No knock data available"}

@app.post("/compare_knocks")
def compare_knocks(data: CompareKnocksInput):
    knock1 = data.knock1
    knock2 = data.knock2
    deviation = data.deviation

    if len(knock1) != len(knock2):
        return {"match": False, "reason": "Knock sequences have different lengths"}

    match = True
    for k1, k2 in zip(knock1, knock2):
        diff = abs(k1 - k2)
        percent_diff = (diff / k2) * 100 if k2 != 0 else 100
        if percent_diff > deviation:
            match = False
            break

    return {"match": match}

@app.get("/patterns")
def get_patterns():
    return {"patterns": knock_patterns}

@app.post("/patterns")
def add_pattern(pattern: KnockPattern):
    # Check if pattern with the same ID already exists
    for p in knock_patterns:
        if p["id"] == pattern.id:
            return {"message": "Pattern ID already exists"}
    knock_patterns.append(pattern.dict())
    save_patterns()
    return {"message": "Pattern added", "pattern": pattern.dict()}
