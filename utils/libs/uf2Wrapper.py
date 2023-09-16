# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

import logging
import os
import subprocess
import sys
from enum import Enum

logger = logging.getLogger()
logging.basicConfig(level=logging.INFO)


class uf2WrapperException(Exception):
    pass

class uf2Wrapper:
    def __init__(self, script_dir):
        self.main_path = os.path.abspath(script_dir)

    def generate_uf2(self, hexpath, devicename):

        hexfile =os.path.abspath(os.path.join(hexpath, "Nordic_MFG.hex"))
        atuf2name = "AssetTracker_" + devicename + ".uf2"
        uf2file = os.path.abspath(os.path.join(os.getcwd(), atuf2name))

        args = [sys.executable, 'uf2conv.py', hexfile, '-c', '-f', '0xADA52840', '-o', uf2file]
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
