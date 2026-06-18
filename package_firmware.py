import hashlib
import os
import shutil
from pathlib import Path

def remove_vowels(string):
    tmp = string
    for vowel in ["a", "e", "i", "o", "u"]:
        tmp = tmp.replace(vowel, "")
    return tmp

for device in ["Sylvania_Smart_LED_Plus"]:
    for floor in [floor for floor in os.scandir(os.path.join(device, "devices")) if floor.is_dir()]:
        for area in [area for area in os.scandir(floor.path) if area.is_dir()]:
            for node_name in [node_name for node_name in os.scandir(area.path) if node_name.is_file() and node_name.name.endswith(".yaml") and not node_name.name == "secrets.yaml" and not node_name.name == "base.yaml"]:
                name = remove_vowels(area.name) + "_" + node_name.name.split(".")[0]
                output_dir = os.path.join("firmware", name)
                os.makedirs(output_dir)

                firmware_bin_file = Path(os.path.join(area.path, ".esphome/build", name, ".pioenvs", name, "firmware.bin"))
                firmware_uf2_file = Path(os.path.join(area.path, ".esphome/build", name, ".pioenvs", name, "firmware.uf2"))
                Path(os.path.join(output_dir, "firmware.ota.bin.md5")).write_text(hashlib.md5(firmware_bin_file.read_bytes()).hexdigest())
                shutil.copyfile(firmware_bin_file, os.path.join(output_dir, "firmware.ota.bin"))
                shutil.copyfile(firmware_uf2_file, os.path.join(output_dir, "firmware.ota.uf2"))
