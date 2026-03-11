import json

file_path = "Assets/Patterns/default_library.json"
with open(file_path, "r") as f:
    data = json.load(f)

for template in data["templates"]:
    if template["id"] == "acoustic_12_8_arpeggiated":
        template["patterns"]["Guitar"]["events"] = [
            { "offsetBeats": 0.0, "lengthBeats": 0.5, "velocityBase": 110, "type": "GuitarPinch", "actionParameter": 33 }, 
            { "offsetBeats": 0.3333333, "lengthBeats": 0.33, "velocityBase": 75, "type": "GuitarPluck", "actionParameter": 3 },
            { "offsetBeats": 0.6666667, "lengthBeats": 0.33, "velocityBase": 80, "type": "GuitarPluck", "actionParameter": 4 }, 

            { "offsetBeats": 1.0, "lengthBeats": 0.5, "velocityBase": 105, "type": "GuitarPinch", "actionParameter": 33 },
            { "offsetBeats": 1.3333333, "lengthBeats": 0.33, "velocityBase": 75, "type": "GuitarPluck", "actionParameter": 3 },
            { "offsetBeats": 1.6666667, "lengthBeats": 0.33, "velocityBase": 80, "type": "GuitarPluck", "actionParameter": 4 },

            { "offsetBeats": 2.0, "lengthBeats": 0.5, "velocityBase": 115, "type": "GuitarPinch", "actionParameter": 33 },
            { "offsetBeats": 2.3333333, "lengthBeats": 0.33, "velocityBase": 75, "type": "GuitarPluck", "actionParameter": 3 },
            { "offsetBeats": 2.6666667, "lengthBeats": 0.33, "velocityBase": 80, "type": "GuitarPluck", "actionParameter": 4 },

            { "offsetBeats": 3.0, "lengthBeats": 0.5, "velocityBase": 100, "type": "GuitarPinch", "actionParameter": 33 },
            { "offsetBeats": 3.3333333, "lengthBeats": 0.33, "velocityBase": 75, "type": "GuitarPluck", "actionParameter": 3 },
            { "offsetBeats": 3.6666667, "lengthBeats": 0.33, "velocityBase": 80, "type": "GuitarPluck", "actionParameter": 4 }
        ]
        break

with open(file_path, "w") as f:
    json.dump(data, f, indent=2)

print("Updated acoustic_12_8_arpeggiated events.")
