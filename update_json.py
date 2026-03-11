import json

file_path = "assets/Patterns/default_library.json"
with open(file_path, "r") as f:
    data = json.load(f)

for template in data["templates"]:
    if template["id"] == "acoustic_12_8_arpeggiated":
        for event in template["patterns"]["Guitar"]["events"]:
            # If it's a pinch targeting 17 (String 0 + 4), 
            # change it to 80 (Bit 6 (Dynamic Bass) + String 4)
            if event["type"] == "GuitarPinch" and event.get("actionParameter") == 17:
                event["actionParameter"] = 80
            # If it's a pluck targeting string 0, change to 64
            elif event["type"] == "GuitarPluck" and event.get("actionParameter") == 0:
                event["actionParameter"] = 64

with open(file_path, "w") as f:
    json.dump(data, f, indent=2)
