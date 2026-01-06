#!/usr/bin/env python3
"""
test.py - STK500v1 Protocol Trace Parser and Analyzer

This script parses raw STK500v1 protocol traces captured from USB CDC
communication between avrdude (or similar host) and the AVR ISP programmer.

*** WORK IN PROGRESS ***
This parser is currently under development. Features being implemented:
- Full command decoding with parameter interpretation
- Response validation and error detection  
- Flash data hex dump and analysis
- Protocol timing analysis

Usage:
    Place captured input bytes in 'in.txt' and responses in 'out.txt'
    Run: python3 test.py

Input Format:
    Raw binary files containing the byte stream from host (in.txt) 
    and programmer responses (out.txt)

Output:
    Paired command/response trace with hex dump

STK500v1 Frame Format:
    - Commands: <cmd> [payload...] <EOP=0x20>
    - Responses: <INSYNC=0x14> [data...] <OK=0x10>

Author: MUdroThe1
Date: 2026
"""

from pathlib import Path

# STK500v1 Protocol Constants
INSYNC = 0x14  # Response sync byte - indicates programmer is synchronized
OK     = 0x10  # Response OK byte - command executed successfully  
EOP    = 0x20  # End Of Packet marker - terminates all commands

def read_bytes(p: str) -> bytes:
    return Path(p).read_bytes()

def split_commands(in_bytes: bytes):
    # STK500v1 commands from host end with CRC_EOP (0x20).
    cmds = []
    cur = bytearray()
    for b in in_bytes:
        cur.append(b)
        if b == EOP:
            cmds.append(bytes(cur))
            cur.clear()
    if cur:
        cmds.append(bytes(cur))
    return cmds

def split_replies(out_bytes: bytes):
    # Typical reply: 0x14 [payload...] 0x10
    replies = []
    i = 0
    n = len(out_bytes)
    while i < n:
        if out_bytes[i] != INSYNC:
            # skip noise bytes (shouldn't happen in clean captures)
            i += 1
            continue
        j = i + 1
        while j < n and out_bytes[j] != OK:
            j += 1
        if j < n and out_bytes[j] == OK:
            replies.append(out_bytes[i:j+1])
            i = j + 1
        else:
            break
    return replies

def hx(bs: bytes) -> str:
    return " ".join(f"{b:02X}" for b in bs)


# =============================================================================
# Main Execution - Parse and display protocol trace
# =============================================================================

# Read captured protocol data from files
# in.txt: Commands from host (avrdude) to programmer
# out.txt: Responses from programmer to host
in_b  = read_bytes("in.txt")
out_b = read_bytes("out.txt")

# Split byte streams into individual frames
cmds = split_commands(in_b)
reps = split_replies(out_b)

# Display summary and paired command/response trace
m = min(len(cmds), len(reps))  # Number of complete pairs
print(f"commands={len(cmds)} replies={len(reps)} paired={m}\n")

# Print each command with its corresponding response
for k in range(m):
    print(f"{k+1:04d} CMD: {hx(cmds[k])}")
    print(f"     RSP: {hx(reps[k])}\n")

# TODO: Add command type decoding
# TODO: Add response validation
# TODO: Add flash data extraction and hex dump
# TODO: Add error detection and reporting