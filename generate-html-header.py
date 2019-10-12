import sys, os
import binascii

def ParseHTML(ID):
    try:
        htmldata = open("html/%s.html" % (ID.lower()), "rb").read().decode("utf-8")
    except:
        print("Error opening html for %s" % (ID))
        sys.exit(-1)

    #strip all comments out
    htmldata = htmldata.split("\n")
    finalhtmldata = ""
    foundscript = 0
    for entry in htmldata:
        entry = entry.strip()
        if not len(entry):
            continue

        if "<script" in entry:
            foundscript = 1
        elif "</script>" in entry:
            foundscript = 0
        
        if not foundscript:
            finalhtmldata += entry + "\n"
        else:
            if entry[0:2] == "//":
                 continue

            if (entry.find("//") != -1) and entry.find("WebSocket") == -1:
                finalhtmldata += entry[0:entry.find("//")] + "\n"
            else:
                finalhtmldata += entry + "\n"
 
    #open("html/%s-test.html" % (ID.lower()), "wb").write(finalhtmldata.encode("utf-8"))

    bindata = binascii.b2a_hex(finalhtmldata.encode("utf-8")).decode("utf-8")

    c_array  = "#ifndef __html_%s_header\n" % (ID)
    c_array += "#define __html_%s_header\n\n" % (ID)
    c_array += "const char HTML%sData[] = {\n" % (ID)

    hexdata = ""
    for i in range(0, len(bindata), 2):
        if i and ((i % 32) == 0):
            hexdata += "\n"

        hexdata += "0x" + bindata[i:i+2] + ", "

    hexdata = hexdata[0:-2]
    if(len(hexdata)):
        hexdata += ", "
    hexdata += "0"
    c_array += hexdata + "};\n"
    c_array += "#define HTML%sDataLen " % (ID) + str(len(finalhtmldata) + 1) + "\n\n"
    c_array += "#endif"

    open("src/html-%s-header.h" % (ID.lower()), "wb").write(c_array.encode("utf-8"))
    return

ParseHTML("Website")
ParseHTML("BusinessCard")
ParseHTML("Party")
ParseHTML("Rick")
ParseHTML("Resume")