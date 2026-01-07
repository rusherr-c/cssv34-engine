import os


TARGET_EXTENSIONS = {
    ".vpc_crc",
    ".sln",
    ".vcproj",
    ".vcxproj",
    ".filters",
    ".user"
}

#cGreen = "[92m" # green color
#cRed = "[91m" # red color
#cBlue = "[94m" # blue color
#cYellow = "[33m" # yellow color
#cDefault = "[0m" # default color

cGreen = ""
cRed = ""
cBlue = ""
cYellow = ""
cDefault = ""

def delete_target_files(base_dir="."):
    deleted_count = 0
    for root, _, files in os.walk(base_dir):
        for file in files:
            _, ext = os.path.splitext(file)
            if ext.lower() in TARGET_EXTENSIONS:
                full_path = os.path.join(root, file)
                try:
                    os.remove(full_path)
                    print(f"{cGreen}Deleted: {full_path}")
                    deleted_count += 1
                except Exception as e:
                    print(f"{cRed}Deleting error {full_path}:{cYellow} {e}")
    print(f"\n{cBlue}Deleted (count): {cDefault}{deleted_count}")

if __name__ == "__main__":
    delete_target_files(".")
