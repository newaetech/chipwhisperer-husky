#
# This script packs firmware files into Python modules to allow easy distribution
#
import os
import base64
import datetime
import binascii

hwdir = r"../../../firmware"


phywhisperer_v = [1, 1]
phywhisperer_files = [("phywhisperer-firmware.zip", os.path.join(hwdir, r"fpga/phywhisperer.zip")),
                ("phywhisperer-SAM3U1C.bin", os.path.join(hwdir, r"phywhisperer/phywhisperer-SAM3U1C.bin"))]


file_list = [
    ("phywhisperer.py", phywhisperer_v, phywhisperer_files),
]


for fdata in file_list:
    f = open(fdata[0], "w")

    f.write("# This file was auto-generated. Do not manually edit or save. What are you doing looking at it? Close it now!\n")
    f.write("# Generated on %s\n"%datetime.datetime.now())
    f.write("#\n")
    f.write("import binascii\n")
    f.write("import io\n\n")
    f.write("fwver = [%d, %d]\n" % (fdata[1][0], fdata[1][1]))
    f.write("def getsome(item, filelike=True):\n")
    f.write("    data = _contents[item].encode('latin-1')\n")
    f.write("    data = binascii.a2b_base64(data)\n")
    f.write("    if filelike:\n")
    f.write("        data = io.BytesIO(data)\n")
    f.write("    return data\n\n")
    f.write("_contents = {\n")

    f.write("")

    for embdata in fdata[2]:
        with open(embdata[1], "rb") as e_file:
            # json_str = base64.b64encode(e_file.read())# json.dumps(e_file.read(), ensure_ascii=False)
            json_str = binascii.b2a_base64(e_file.read())

            f.write("\n#Contents from %s\n"%embdata[1])
            f.write("'%s':'"%embdata[0])
            f.write(json_str.decode().replace("\n",""))
            f.write("',\n\n")
            f.flush()
    f.write("}\n")

f.close()


