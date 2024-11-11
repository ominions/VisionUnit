import os
import json
import requests
import time
import threading
from flask import Flask, render_template, send_from_directory
from urllib.parse import unquote
from mimetypes import guess_extension

app = Flask(__name__)

# Directory to save uploaded images
image_dir = os.path.join(app.root_path, "static/images")
os.makedirs(image_dir, exist_ok=True)  # Ensure the folder exists

# Path for the persisted fetched images file
fetched_images_file = 'fetched_images.json'

# Load the previously fetched images from a file (if any)
def load_fetched_images():
    if os.path.exists(fetched_images_file):
        with open(fetched_images_file, 'r') as f:
            return set(json.load(f))
    return set()

# Save the list of fetched images to a file
def save_fetched_images():
    with open(fetched_images_file, 'w') as f:
        json.dump(list(fetched_images), f)

# Load initially
fetched_images = load_fetched_images()

# Function to sanitize and decode filenames
def sanitize_filename(url):
    # Decode the URL
    filename = unquote(url.split("/")[-1])  # Get the last part of the URL and decode
    filename = filename.replace("?", "_").replace("&", "_").replace("=", "_").replace("%", "_")
    filename = filename.replace(":", "_").replace("/", "_")
    
    # Ensure the file has a valid extension (check content type)
    if not any(filename.endswith(ext) for ext in ['.jpg', '.jpeg', '.png', '.gif']):
        filename = filename + '.jpg'  # Default to '.jpg' if no valid extension is present

    return filename

# Function to fetch images from the API
def fetch_images():
    while True:
        try:
            response = requests.get('https://dev.giriamrit.com.np/api/images/')
            
            if response.status_code == 200:
                images = response.json()

                print(f"API returned {len(images)} images.")  # Log the number of images returned

                for image in images:
                    image_url = image.get('image_url')
                    
                    if image_url:
                        filename = sanitize_filename(image_url)
                        filepath = os.path.join(image_dir, filename)
                        
                        # Check if image was already fetched or exists locally
                        if filename not in fetched_images and not os.path.exists(filepath):
                            print(f"Fetching new image: {image_url} -> Saving as {filename}")
                            fetched_images.add(filename)
                            
                            try:
                                img_data = requests.get(image_url).content
                                
                                # Save the image to the file system
                                with open(filepath, "wb") as f:
                                    f.write(img_data)
                                print(f"Fetched and saved image: {filepath}")
                            except Exception as e:
                                print(f"Error downloading image {image_url}: {e}")
                        else:
                            print(f"Skipping image {filename}, already fetched or exists locally.")

                # Save the updated list of fetched images
                save_fetched_images()

            else:
                print(f"Error fetching image data from the server. Status code: {response.status_code}")
        
        except Exception as e:
            print(f"Error occurred during fetch: {e}")
        
        time.sleep(60)  # Check every minute for new images

# Start the image-fetching function in a separate thread
threading.Thread(target=fetch_images, daemon=True).start()

@app.route("/show-images")
def show_images():
    image_filenames = os.listdir("static/images")
    image_filenames.sort(reverse=True)  # Show newest images first
    return render_template("image_gallery.html", images=image_filenames)

@app.route("/images/<filename>")
def images(filename):
    return send_from_directory("static/images", filename)

if __name__ == "__main__":
    app.run(debug=True)
