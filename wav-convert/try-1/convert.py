#!/usr/bin/env python3

#http://soundfile.sapp.org/doc/WaveFormat/

from sys import exit

#Expected values
SAMPLE_RATE=8000
NUM_CHANNELS=1
BITS_PER_SAMPLE=16
#Does not match! Expected 16k but found 32k
BYTE_RATE=SAMPLE_RATE*NUM_CHANNELS*BITS_PER_SAMPLE/8
BYTE_RATE=32000
BLOCK_ALIGN=NUM_CHANNELS*BITS_PER_SAMPLE/8

#Info fields at beginning of WAV
NAME=0
SIZE=1
ENDIAN=2
EXPECTED=3
header= [   ["ChunkID",         4,  "big",      "RIFF"],
            ["ChunkSize",       4,  "little",   ""],
            ["Format",          4,  "big",      "WAVE"]]

fmt_sub=[   ["Subchunk1ID",     4,  "big",      "fmt "],
            ["Subchunk1Size",   4,  "little",   16],
            ["AudioFormat",     2,  "little",   1],
            ["NumChannels",     2,  "little",   NUM_CHANNELS],
            ["SampleRate",      4,  "little",   SAMPLE_RATE],
            ["ByteRate",        4,  "little",   BYTE_RATE],
            ["BlockAlign",      2,  "little",   BLOCK_ALIGN],
            ["BitsPerSample",   2,  "little",   BITS_PER_SAMPLE]]

data_sub=[  ["Subchunk2ID",     4,  "big",      "data"],
            ["Subchunk2Size",   4,  "little",   ""]]
        
fields=[header,fmt_sub,data_sub]

#Holds info fields at beginning of WAV
wav_info={}

with open("test.wav","rb") as f:
    #Read in info fields at beggining of WAV
    for field in fields:
        for info in field:
            
            #Add info from list above to dictionary of info fields
            info_name=info[NAME]
            wav_info[info_name]={}
            wav_info[info_name]["size"]=info[SIZE]
            wav_info[info_name]["endian"]=info[ENDIAN]
            wav_info[info_name]["expected"]=info[EXPECTED]

            #Add data from file to dictionary of info fields
            temp_data=f.read(info[SIZE])
            wav_info[info_name]["raw"]=temp_data
            if info[ENDIAN]=="little":
                wav_info[info_name]["int"]=int.from_bytes(temp_data,"little")
                wav_info[info_name]["value"]=wav_info[info_name]["int"]
            elif info[ENDIAN]=="big":
                wav_info[info_name]["text"]=temp_data.decode("utf-8")
                wav_info[info_name]["value"]=wav_info[info_name]["text"]
            else:
                print("Error: Unknown endianness -",info[ENDIAN])
                exit()

            #Check that value read in matches expected value
            if info[EXPECTED]!="":
                if info[EXPECTED]!=wav_info[info_name]["value"]:
                    print(info_name+": mismatch!")
                    print("\tExpected: "+str(info[EXPECTED]))
                    print("\tFound: "+str(wav_info[info_name]["value"]))
                    exit()

            #Display data read in
            print(info_name+": "+str(wav_info[info_name]["value"]))

    #Read in data bytes
    wav_data=[]
    block_size=wav_info["BlockAlign"]["value"]
    byte_count=wav_info["Subchunk2Size"]["value"]
    for i in range(0,byte_count,block_size):
        #Convert signed to unsigned
        SIGN_BIAS=32768
        wav_data+=[SIGN_BIAS+int.from_bytes(f.read(block_size),"little",signed=True)]

#Convert PCM data from WAV to PDM
#https://dlbeer.co.nz/articles/pdm.html
#https://en.wikipedia.org/wiki/Pulse-density_modulation
pdm_error=0
pdm_data=[]
bit_count=16
pdm_sample=0
SAMPLE_MAX=0xFFFF
for sample in wav_data:
    #Double check signed to unsigned above succeeded
    if sample>0xFFFF or sample<0:
        print("Sample out of range: "+hex(sample))
        exit()
    pdm_error+=sample
    print("Sample:",sample,"New error:",pdm_error)
    pdm_sample>>=1
    if pdm_error>SAMPLE_MAX:
        print("Error exceeded! Output 1") 
        pdm_error-=SAMPLE_MAX
        pdm_sample|=0x8000
    else:
        print("Error below limit. Output 0")
    bit_count-=1
    if bit_count==0:
        print("Word formed:",pdm_sample)
        print()
        pdm_data+=[pdm_sample]
        pdm_sample=0
        bit_count=16
if bit_count!=16:
    pdm_sample>>=bit_count
    pdm_data+=[pdm_sample]
    
#Output converted PDM data to C file
SAMPLES_PER_LINE=8
with open("../wav-data.c","wt") as f:
    #Count of samples
    f.write("const unsigned int wav_data_size="+str(len(pdm_data))+";\n")
    #PDM data
    f.write("const unsigned int wav_data[]={\n")
    line_temp=""
    line_size=0
    for sample in pdm_data:
        line_temp+=hex(sample)+", "
        line_size+=1
        if line_size==SAMPLES_PER_LINE:
            f.write(line_temp+"\n")
            line_temp=""
            line_size=0
    if line_size!=0:
        f.write(line_temp+"\n")
    #Add 0 at end after last comma
    f.write("0")
    #End of array
    f.write("};\n")
