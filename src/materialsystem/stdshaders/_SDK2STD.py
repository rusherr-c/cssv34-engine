import os

for filename in os.listdir('.'):
    if filename.startswith('sdk_'):
        new_name = filename[len('sdk_'):]
        os.rename(filename, new_name)
        print(f'Renamed: {filename} -> {new_name}')