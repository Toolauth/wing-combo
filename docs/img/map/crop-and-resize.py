import os
from PIL import Image

# --- CONFIGURATION ---
# 1. Folder containing the original PNG images (use '.' for the current folder)
IMAGE_FOLDER = '.'

# 2. Output folder (will be created if it doesn't exist)
OUTPUT_FOLDER = 'cropped_images'

# 3. Target dimensions
TARGET_SIZE = 1160
# ---------------------

def crop_and_resize_images():
    """
    Performs a center-crop (3376x1937 -> 1937x1937) and then scales down 
    to 968x968 for all PNG images in the current folder.
    """

    # 1. Ensure the output folder exists
    if not os.path.exists(OUTPUT_FOLDER):
        os.makedirs(OUTPUT_FOLDER)
        print(f"Created output folder: {OUTPUT_FOLDER}")

    # 2. Process all files in the input folder
    for filename in os.listdir(IMAGE_FOLDER):
        if filename.lower().endswith('.png'):
            input_path = os.path.join(IMAGE_FOLDER, filename)

            # Skip the output folder itself
            if os.path.isdir(input_path):
                continue
            
            output_path = os.path.join(OUTPUT_FOLDER, filename)

            try:
                img = Image.open(input_path)
                original_width, original_height = img.size

                # Verify image dimensions match expected original size
                if original_width != 3376 or original_height != 1937:
                    print(f"⚠️ Warning: Skipping {filename}. Dimensions are {original_width}x{original_height}, not the expected 3376x1937.")
                    continue

                # --- STEP 1: Calculate Center-Crop Box (3376x1937 -> 1937x1937) ---
                # We need to find the horizontal center to crop the 1937-pixel height square.
                crop_height = original_height  # 1937
                
                # Calculate the left and right coordinates to center the 1937 width
                left = (original_width - crop_height) // 2
                right = left + crop_height
                
                # The crop box is (left, top, right, bottom)
                # Since we keep the full height, top=0 and bottom=original_height
                crop_box = (left, 0, right, original_height)
                
                # Perform the center crop
                cropped_img = img.crop(crop_box)
                
                # --- STEP 2: Downscale to TARGET_SIZE x TARGET_SIZE (968x968) ---
                resized_img = cropped_img.resize((TARGET_SIZE, TARGET_SIZE), Image.Resampling.LANCZOS)
                
                # Save the final image
                resized_img.save(output_path)

                print(f"✅ Success: Processed {filename} -> Saved to {output_path} at {TARGET_SIZE}x{TARGET_SIZE}")

            except Exception as e:
                print(f"❌ Error processing {filename}: {e}")

    print("\n✨ Cropping and scaling process complete.")

if __name__ == "__main__":
    # Ensure Pillow is installed before running: pip install Pillow
    crop_and_resize_images()