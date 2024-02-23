import json
import os

def load_json_data(filename):
    if os.path.exists(filename):
        with open(filename,'r', encoding="utf-8") as f:
            return json.load(f)

def write_json_data(filename, data):
    parent_dir = os.path.abspath(os.path.dirname(filename))
    if not os.path.exists(parent_dir):
        os.makedirs(parent_dir)
    with open(filename, 'w', encoding="utf8") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)