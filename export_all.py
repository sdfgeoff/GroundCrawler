import os

ROOT_FOLDER = os.path.dirname(os.path.abspath(__file__))

EXPORT_SINGLE = """
import FreeCAD
import sys
import os

args = sys.argv[sys.argv.index("--")+1:]

input_filename = args[0]
output_folder = args[1]

print("Opening {}".format(input_filename))
doc = FreeCAD.open(input_filename)

clean_filename = os.path.basename(input_filename).replace(".FCStd", "")

for obj in doc.Objects:
    if obj.Label.endswith(".stl"):
        shape = obj.Shape
        
        output_filename = "{}-{}".format(clean_filename, obj.Label)
        output_filename = os.path.join(output_folder, output_filename)
        print("Exporting to {}".format(output_filename))
        shape.exportStl(output_filename)
        
exit(0)
"""

EXPORT_SINGLE_FILENAME = "/tmp/export_single_stl.py"
open(EXPORT_SINGLE_FILENAME, "w").write(EXPORT_SINGLE)

MECHANICS_FOLDER = os.path.join(ROOT_FOLDER, "Mechanics")
OUTPUT_FOLDER = os.path.join(ROOT_FOLDER, "stls")

if not os.path.exists(OUTPUT_FOLDER):
    os.mkdir(OUTPUT_FOLDER)


FREECAD_BINARY = "freecad"


for file_name in os.listdir(MECHANICS_FOLDER):
    filepath = os.path.join(MECHANICS_FOLDER, file_name)
    if file_name.endswith(".FCStd"):
        command = "{} -c {} -- {} {}".format(FREECAD_BINARY, EXPORT_SINGLE_FILENAME, filepath, OUTPUT_FOLDER)
        print(command)
        os.system(command)


