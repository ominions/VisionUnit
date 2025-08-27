import os
import json
import requests
import time
import threading
from flask import Flask, render_template, send_from_directory
from urllib.parse import unquote

app = Flask(__name__)
images_root = os.path.join(app.root_path, "static/images")
os.makedirs(images_root, exist_ok=True)

# ---- Utility functions ----

def sanitize_filename(url):
    filename = unquote(url.split("/")[-1])
    filename = filename.replace("?", "_").replace("&", "_").replace("=", "_").replace("%", "_").replace(":", "_").replace("/", "_")
    if not any(filename.endswith(ext) for ext in ['.jpg', '.jpeg', '.png', '.gif']):
        filename += '.jpg'
    return filename

def get_latest_batch():
    batches = [
        (int(name.replace("batch", "")), name)
        for name in os.listdir(images_root)
        if name.startswith("batch") and os.path.isdir(os.path.join(images_root, name))
    ]
    return max(batches, default=(0, "batch0"))[1]  # returns name like 'batch3'

def load_all_saved_images():
    saved = set()
    for batch_folder in os.listdir(images_root):
        path = os.path.join(images_root, batch_folder)
        if os.path.isdir(path) and batch_folder.startswith("batch"):
            for file in os.listdir(path):
                saved.add(file)
    return saved

# Path for JSON file tracking saved images
json_path = os.path.join(images_root, "fetch_images.json")

def load_saved_images_json():
    if os.path.exists(json_path):
        try:
            with open(json_path, "r") as f:
                return json.load(f)
        except Exception as e:
            print(f"Failed to load JSON: {e}")
    return []

# ---- Load previous state ----

last_batch_name = get_latest_batch()
last_batch_path = os.path.join(images_root, last_batch_name)
saved_images = load_all_saved_images()

saved_images_info = load_saved_images_json()

current_batch_name = None
current_batch_path = None

# ---- Fetch function ----

def fetch_images():
    global current_batch_name, current_batch_path, saved_images_info

    try:
        response = requests.get('<backend_url>')
        if response.status_code != 200:
            print(f"[{last_batch_name}] Failed to fetch: {response.status_code}")
            return

        images = response.json()
        new_images = []

        for img in images:
            url = img.get("image_url")
            if not url:
                continue

            filename = sanitize_filename(url)
            if filename not in saved_images:
                new_images.append((filename, url))

        if not new_images:
            print(f"[{last_batch_name}] No new images found.")
            return

        # Create next batch only if new images exist
        next_batch_num = int(last_batch_name.replace("batch", "")) + 1
        current_batch_name = f"batch{next_batch_num}"
        current_batch_path = os.path.join(images_root, current_batch_name)
        os.makedirs(current_batch_path, exist_ok=True)

        print(f"[{current_batch_name}] Saving {len(new_images)} new images...")

        for filename, url in new_images:
            try:
                img_data = requests.get(url).content
                with open(os.path.join(current_batch_path, filename), "wb") as f:
                    f.write(img_data)

                saved_images.add(filename)

                # Append to JSON list and save JSON file
                saved_images_info.append({"filename": filename, "url": url, "batch": current_batch_name})
                with open(json_path, "w") as fjson:
                    json.dump(saved_images_info, fjson, indent=2)

                print(f"[{current_batch_name}] Saved: {filename}")
            except Exception as e:
                print(f"[{current_batch_name}] Failed to save {filename}: {e}")

    except Exception as e:
        print(f"[{last_batch_name}] Exception: {e}")

# ---- Background thread loop ----

def background_fetch():
    while True:
        fetch_images()
        time.sleep(60)

threading.Thread(target=background_fetch, daemon=True).start()

# ---- Routes ----

@app.route("/show-images")
def show_images():
    all_images = []
    for batch in sorted(os.listdir(images_root), reverse=True):
        path = os.path.join(images_root, batch)
        if os.path.isdir(path):
            for file in sorted(os.listdir(path), reverse=True):
                all_images.append(f"{batch}/{file}")
    return render_template("image_gallery.html", images=all_images)

@app.route("/images/<path:filename>")
def images(filename):
    return send_from_directory(images_root, filename)

if __name__ == "__main__":
    print(f"Starting server — last batch: {last_batch_name}")
    app.run(debug=True)
