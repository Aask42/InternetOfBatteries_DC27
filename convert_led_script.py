import sys, os
import struct

def GetLED(LEDName):
    LEDNames = ["led_20", "led_40", "led_60", "led_80", "led_100", "node", "batt", "stat", "pout"]
    if LEDName in LEDNames:
        return LEDNames.index(LEDName)

    print("Error: %s is not a valid LED name" % (LEDName))
    sys.exit(-1)

def GetLevel(Level):
    #if the level is * then return 0xffff
    try:
        if Level[0] == "*":
            return struct.pack("<H", 0xffff)
        elif Level[0][0:5] == "lvar_":
            VarID = int(Level[0][5:])
            if (VarID < 0) or (VarID > 15):
                return -1;
            return struct.pack("<H", 0xfff0 + VarID)
        elif Level[0][0:5] == "gvar_":
            VarID = int(Level[0][5:])
            if (VarID < 0) or (VarID > 16):
                return -1;
            return struct.pack("<H", 0xffe0 + VarID)
        elif Level[0] == "rand":
            if len(Level) < 3:
                return -1

            #get the two values to return
            Val1 = float(Level[1])
            Val2 = float(Level[2])
            if Val1 > Val2:
                return -1

            if (Val1 < 0.0) or (Val1 > 300.0):
                return -1

            if (Val2 < 0.0) or (Val2 > 300.0):
                return -1

            return struct.pack("<HH", int(Val1 * 100), int(Val2 * 100))
        else:
            Val = float(Level[0])
            if (Val < 0.0) or (Val > 300.0):
                return -1

            return struct.pack("<H", int(Val * 100))
    except:
        pass

    return -1

def GetTime(Time):
    #if the level is * then return 0xffff
    try:
        if Time[0][0:5] == "lvar_":
            VarID = int(Time[0][5:])
            if (VarID < 0) or (VarID > 15):
                return -1;
            return struct.pack("<H", 0xfff0 + VarID)
        elif Time[0][0:5] == "gvar_":
            VarID = int(Time[0][5:])
            if (VarID < 0) or (VarID > 16):
                return -1;
            return struct.pack("<H", 0xffe0 + VarID)        
        elif Time[0] == "rand":
            if len(Time) < 3:
                return -1

            #get the two values to return
            Val1 = int(Time[1])
            Val2 = int(Time[2])
            if Val1 > Val2:
                return -1

            if (Val1 < 0) or (Val1 > 64000):
                return -1

            if (Val2 < 0) or (Val2 > 64000):
                return -1

            return struct.pack("<HH", Val1, Val2)
        else:
            Val = int(Time[0])
            if (Val < 0) or (Val > 64000):
                return -1

            return struct.pack("<H", Val)
    except:
        pass

    return -1

def ParseLED(directory, filename, entrycount, initfile):
    print("Parsing %s" % (filename))

    #get the led data
    leddata = open(directory + "/" + filename, "rb").read().decode("utf-8").split("\n")

    labels = dict()
    unknownlabels = []

    #parse
    outdata = bytearray()
    outdata += struct.pack("<H", 0) #led bit flag

    LEDBits = 0
    linenum = 0
    for curline in leddata:
        #strip any whitespace away
        curline = curline.strip().lower()
        linenum += 1

        if len(curline) == 0:
            continue

        #if a comment then skip
        if curline[0:2] == "//":
            continue

        splitline = curline.split(" ")

        #if a label then store it's location
        if curline[-1] == ":":
            if len(splitline) != 1:
                print("%s: Error parsing label on line %d: %s" % (filename, linenum, curline))
                sys.exit(-1)

            labelname = curline[0:-1]
            if labelname in labels:
                print("%s: Error label %s on line %d already exists on line %d" % (filename, labelname, linenum, labels[labelname]["line"]))
                sys.exit(-1)

            #all good, add it
            labels[labelname] = {"line": linenum, "pos": len(outdata)}
            continue

        #handle command
        if splitline[0] in ["set", "add", "sub"]:
            LEDVal = GetLED(splitline[1])
            LEDBits |= (1 << LEDVal)

            Level = GetLevel(splitline[2:])
            if (LEDVal == -1) or (Level == -1):
                print("%s: Error parsing line %d" % (filename, linenum))
                sys.exit(0)

            Command = ["set", "add","sub"].index(splitline[0])
            if len(Level) > 2:
                Command |= 0x80

            outdata.append(Command)
            outdata.append(LEDVal)
            outdata += Level

        elif splitline[0] == "move":
            LEDVal = GetLED(splitline[1])
            LEDBits |= (1 << LEDVal)

            Level1 = GetLevel(splitline[2:])
            Level2 = -1
            levelpos = 3
            if (Level1 != -1):
                if (len(Level1) != 2):
                    levelpos += 2
                Level2 = GetLevel(splitline[levelpos:])

            levelpos = 4
            if (Level2 != -1) and (len(Level2) != 2):
                levelpos += 2

            Time = GetTime(splitline[levelpos:])

            if (LEDVal == -1) or (Level1 == -1) or (Level2 == -1) or (Time == -1):
                print("%s: Error parsing line %d" % (filename, linenum))
                sys.exit(-1)

            Command = 3
            if len(Level1) > 2:
                Command |= 0x80
            if len(Level2) > 2:
                Command |= 0x40
            if len(Time) > 2:
                Command |= 0x20

            outdata.append(Command)
            outdata.append(LEDVal)
            outdata += Level1 + Level2 + Time

        elif splitline[0] == "delay":
            Time = GetTime(splitline[1:])
            if(Time == -1):
                print("%s: Error parsing line %d" % (filename, linenum))
                sys.exit(-1)

            Command = 0x10
            if len(Time) > 2:
                Command |= 0x80

            outdata.append(Command)
            outdata += Time

        elif splitline[0] == "stop":
            if len(splitline) >= 2:
                Time = GetTime(splitline[1:])
                if(Time == -1):
                    print("%s: Error parsing line %d" % (filename, linenum))
                    sys.exit(-1)
            else:
                Time = struct.pack("<H", 0xffff)

            Command = 0x11
            if len(Time) > 2:
                Command |= 0x80

            outdata.append(Command)
            outdata += Time

        elif splitline[0] in ["if", "wait"]:
            LEDVal = GetLED(splitline[1])
            LEDBits |= (1 << LEDVal)

            Level = GetLevel(splitline[3:])
            MathType = ["=", "<", ">", "<=", ">="].index(splitline[2])

            if splitline[0] == "if":
                Command = 0x12
            else:
                Command = 0x14

            if len(Level) > 2:
                Command |= 0x80

            outdata.append(Command)
            outdata.append(LEDVal)
            outdata.append(MathType)
            outdata += Level

            if splitline[0] == "if":
                #setup a label entry that is empty so it is filled in later
                labelpos = 4
                if(len(Level) != 2):
                    labelpos += 2
                unknownlabels.append({"name": splitline[labelpos], "line": linenum, "pos": len(outdata)})
                outdata += struct.pack("<H", 0xffff)     #label position

        elif splitline[0] == "goto":
            #get the label name
            if splitline[1] in labels:
                labelpos = labels[splitline[1]]["pos"]
            else:
                #label is unknown, add it to the unknown and return 0xffff
                labelpos = 0xffff
                unknownlabels.append({"name": splitline[1], "line": linenum, "pos": len(outdata) + 1})

            Command = 0x13
            outdata.append(Command)
            outdata += struct.pack("<H", labelpos)
        else:
            #unknown line
            print("Unknown command: %s" % (splitline[0]))
            sys.exit(-1)

    #go through our unknown labels and fill them in
    for entry in unknownlabels:
        #if it doesn't exist then error
        if entry["name"] not in labels:
            print("%s Error: unable to locate label %s used on line %d" % (entry["name"], entry["line"]))
            sys.exit(-1)

        #modify our byte array for the proper location
        outdata[entry["pos"]:entry["pos"]+2] = struct.pack("<H", labels[entry["name"]]["pos"])

    #modify the first entry for the led bits
    outdata[0:2] = struct.pack("<H", LEDBits)

    #generate the hex data and output it
    outhexdata = ""
    for i in range(0, len(outdata)):
        if i and ((i % 16) == 0):
            outhexdata += "\n"
        outhexdata += "0x%02x, " % (outdata[i])

    filename = filename.replace(" ", "_")
    outfiledata = "const uint8_t led_%s[] = {\n" % (filename[0:-4].lower())
    outfiledata += outhexdata[0:-2]
    outfiledata += "};\n"
    outfiledata += "#define led_%s_len %d\n" % (filename[0:-4].lower(), len(outdata))
    outfiledata += "#define LED_%s %d\n" % (filename[0:-4].upper(), entrycount)
    initfile.write(("LEDHandler->AddScript(LED_%s, led_%s, led_%s_len);\n" % (filename[0:-4].upper(), filename[0:-4].lower(), filename[0:-4].lower())).encode("utf-8"))

    return outfiledata.encode("utf-8")

def main():
    #convert led scripts over to a binary format included into the C code for being parsed
    f = open("src/leds-data.h", "wb")
    f2 = open("src/leds-setup.h", "wb")

    f.write("#ifndef __leds_data_h\n#define __leds_data_h\n\n#define LED_ALL 0xffff\n\n".encode("utf-8"))

    count = 0
    for file in os.listdir("leds/."):
        if file[-4:] == ".led":
            #have a file to parse
            f.write(ParseLED("leds", file, count, f2))
            count += 1

    f.write(("#define LED_SCRIPT_COUNT %d\n" % (count)).encode("utf-8"))
    f.write(("#define LED_SYSTEM_SCRIPT_START %d\n" % (count)).encode("utf-8"))

    systemcount = 0
    for file in os.listdir("leds - system/."):
        if file[-4:] == ".led":
            #have a file to parse
            f.write(ParseLED("leds - system", file, count, f2))
            count += 1
            systemcount += 1

    f.write(("#define LED_SYSTEM_SCRIPT_COUNT %d\n" % (systemcount)).encode("utf-8"))
    f.write(("#define LED_TOTAL_SCRIPT_COUNT %d\n" % (count)).encode("utf-8"))
    f.write("\n#endif\n".encode("utf-8"))
    f.close()
    f2.close()
main()