import os

search_word = "IThreadPool"

for root, dirs, files in os.walk("."):
    for filename in files:
        filepath = os.path.join(root, filename)
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                for line_num, line in enumerate(f, start=1):
                    if search_word in line:
                        print(f"{filepath} (строка {line_num}): {line.strip()}")
        except (UnicodeDecodeError, PermissionError):
            pass
