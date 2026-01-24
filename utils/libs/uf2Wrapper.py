# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import logging
import os
import subprocess
import sys
from enum import Enum

logger = logging.getLogger()
logging.basicConfig(level=logging.INFO)

# Flash region constants for MFG + sidewalk_storage erasure
# MFG page: 0xd0000-0xd0FFF (4KB)
# sidewalk_storage: 0xd1000-0xd1FFF (4KB)
MFG_START_ADDR = 0xd0000
MFG_END_ADDR = 0xd1FFF  # Extended to include sidewalk_storage area (8KB total)


class uf2WrapperException(Exception):
    pass


def extend_hex_with_padding(input_hex_path, output_hex_path, start_addr, end_addr):
    """
    Read an Intel HEX file and extend it with padding to cover the full
    address range from start_addr to end_addr (inclusive).
    Uses 0x00 for padding to force the UF2 bootloader to write (and thus erase) those pages.
    """
    # Parse existing hex file into a dict of address -> byte
    data = {}
    with open(input_hex_path, 'r') as f:
        upper_addr = 0
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            # Parse Intel HEX record
            byte_count = int(line[1:3], 16)
            address = int(line[3:7], 16)
            record_type = int(line[7:9], 16)
            
            if record_type == 0x04:  # Extended Linear Address
                upper_addr = int(line[9:13], 16) << 16
            elif record_type == 0x02:  # Extended Segment Address
                upper_addr = int(line[9:13], 16) << 4
            elif record_type == 0x00:  # Data record
                full_addr = upper_addr + address
                for i in range(byte_count):
                    byte_val = int(line[9 + i*2:11 + i*2], 16)
                    data[full_addr + i] = byte_val
            elif record_type == 0x01:  # EOF
                break
    
    # Fill in padding for any missing addresses in the range
    # Use 0x00 to ensure the bootloader writes to these locations (forcing erase)
    # Note: Flash erased state is 0xFF, so writing 0x00 forces actual writes
    for addr in range(start_addr, end_addr + 1):
        if addr not in data:
            data[addr] = 0x00
    
    # Write extended hex file
    with open(output_hex_path, 'w') as f:
        # Write extended linear address record for upper 16 bits
        upper = (start_addr >> 16) & 0xFFFF
        checksum = (0x02 + 0x00 + 0x00 + 0x04 + (upper >> 8) + (upper & 0xFF)) & 0xFF
        checksum = (~checksum + 1) & 0xFF
        f.write(f':02000004{upper:04X}{checksum:02X}\n')
        
        # Write data records (16 bytes per line)
        current_addr = start_addr
        while current_addr <= end_addr:
            # Calculate how many bytes to write (max 16)
            bytes_remaining = end_addr - current_addr + 1
            byte_count = min(16, bytes_remaining)
            
            # Build data bytes
            data_bytes = []
            for i in range(byte_count):
                data_bytes.append(data.get(current_addr + i, 0x00))
            
            # Calculate checksum
            addr_low = current_addr & 0xFFFF
            checksum = byte_count + (addr_low >> 8) + (addr_low & 0xFF) + 0x00
            checksum += sum(data_bytes)
            checksum = (~checksum + 1) & 0xFF
            
            # Format record
            data_str = ''.join(f'{b:02X}' for b in data_bytes)
            f.write(f':{byte_count:02X}{addr_low:04X}00{data_str}{checksum:02X}\n')
            
            current_addr += byte_count
        
        # Write EOF record
        f.write(':00000001FF\n')
    
    logger.info(f"Extended hex file to cover 0x{start_addr:X}-0x{end_addr:X} ({end_addr - start_addr + 1} bytes)")


class uf2Wrapper:
    def __init__(self, script_dir):
        self.main_path = os.path.abspath(script_dir)

    def generate_uf2(self, hexpath, devicename):

        hexfile = os.path.abspath(os.path.join(hexpath, "Nordic_MFG.hex"))
        extended_hexfile = os.path.abspath(os.path.join(hexpath, "Nordic_MFG_extended.hex"))
        atuf2name = "AssetTracker_" + devicename + ".uf2"
        uf2file = os.path.abspath(os.path.join(os.getcwd(), atuf2name))

        # Extend hex file to cover full MFG + sidewalk_storage region
        extend_hex_with_padding(hexfile, extended_hexfile, MFG_START_ADDR, MFG_END_ADDR)

        args = [sys.executable, 'uf2conv.py', extended_hexfile, '-c', '-f', '0xADA52840', '-o', uf2file]
        result = subprocess.run(args=args,
                                cwd=self.main_path, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        print_subprocess_results(result, subprocess_name="uf2conv.py")

def print_subprocess_results(result, subprocess_name="", withAssert=True):
    if result.returncode != 0:
        message = " ".join(result.args)
        message += " \n"
        message += " " + result.stdout.decode()
        message += " " + result.stderr.decode()
        raise uf2WrapperException(message)

    for line in result.stdout.decode().splitlines():
        logger.debug(line)
        if withAssert:
            assert 'error' not in line, f"Something went wrong after calling subprocess {subprocess_name}"

    for line in result.stderr.decode().splitlines():
        logger.error(line)
        if withAssert:
            assert 'error' not in line, f"Something went wrong after calling subprocess {subprocess_name}"
