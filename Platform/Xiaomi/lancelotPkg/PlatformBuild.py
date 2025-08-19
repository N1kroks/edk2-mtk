#!/usr/bin/env python3
import datetime
import logging
import os
import uuid
import subprocess
import gzip
import shutil
import logging

from io import StringIO
from pathlib import Path

from edk2toolext import edk2_logging
from edk2toolext.environment import shell_environment
from edk2toolext.environment.uefi_build import UefiBuilder
from edk2toolext.invocables.edk2_platform_build import BuildSettingsManager
from edk2toolext.invocables.edk2_pr_eval import PrEvalSettingsManager
from edk2toolext.invocables.edk2_setup import (RequiredSubmodule, SetupSettingsManager)
from edk2toolext.invocables.edk2_update import UpdateSettingsManager
from edk2toolext.invocables.edk2_parse import ParseSettingsManager
from edk2toollib.utility_functions import RunCmd

# ####################################################################################### #
#                                Common Configuration                                     #
# ####################################################################################### #
class CommonPlatform ():
    PackagesSupported = ("lancelotPkg")
    ArchSupported = ("AARCH64")
    TargetsSupported = ("DEBUG", "RELEASE")
    Scopes = ('lancelot', 'gcc_aarch64_linux', 'edk2-build')
    WorkspaceRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    PackagesPath = (
        "Platform/Xiaomi",
        "Common/edk2",
        "Common/edk2-platforms",
        "Silicon/MediaTek"
    )

# ####################################################################################### #
#                         Configuration for Update & Setup                                #
# ####################################################################################### #
class SettingsManager (UpdateSettingsManager, SetupSettingsManager, PrEvalSettingsManager, ParseSettingsManager):

    def GetPackagesSupported (self):
        return CommonPlatform.PackagesSupported

    def GetArchitecturesSupported (self):
        return CommonPlatform.ArchSupported

    def GetTargetsSupported (self):
        return CommonPlatform.TargetsSupported

    def GetRequiredSubmodules (self):
        return [
            RequiredSubmodule ("Common/edk2", True),
        ]

    def SetArchitectures (self, list_of_requested_architectures):
        unsupported = set(list_of_requested_architectures) - set(self.GetArchitecturesSupported())

        if (len(unsupported) > 0):
            errorString = ("Unsupported Architecture Requested: " + " ".join(unsupported))
            logging.critical (errorString)
            raise Exception (errorString)

        self.ActualArchitectures = list_of_requested_architectures

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def FilterPackagesToTest (self, changedFilesList: list, potentialPackagesList: list) -> list:
        build_these_packages = []
        possible_packages = potentialPackagesList.copy ()

        for f in changedFilesList:
            if "BaseTools" in f:
                if os.path.splitext(f) not in [".txt", ".md"]:
                    build_these_packages = possible_packages
                    break

            if "platform-build-run-steps.yml" in f:
                build_these_packages = possible_packages
                break

        return build_these_packages

    def GetPlatformDscAndConfig (self) -> tuple:
        return ("lancelotPkg/lancelot.dsc", {})

    def GetName (self):
        return "lancelot"

    def GetPackagesPath (self):
        return CommonPlatform.PackagesPath

# ####################################################################################### #
#                         Actual Configuration for Platform Build                         #
# ####################################################################################### #
class PlatformBuilder (UefiBuilder, BuildSettingsManager):
    def __init__ (self):
        UefiBuilder.__init__ (self)

    def AddCommandLineOptions (self, parserObj):
        parserObj.add_argument('-a', "--arch", dest="build_arch", type=str, default="AARCH64", help="Optional - CSV of architecture to build. AARCH64 is used for PEI and DXE and is the only valid option for this platform.")

    def RetrieveCommandLineOptions (self, args):
        if args.build_arch.upper() != "AARCH64":
            raise Exception("Invalid Arch Specified.  Please see comments in PlatformBuild.py::PlatformBuilder::AddCommandLineOptions")

    def GetWorkspaceRoot (self):
        return CommonPlatform.WorkspaceRoot

    def GetPackagesPath (self):
        result = [ shell_environment.GetBuildVars().GetValue("FEATURE_CONFIG_PATH", "") ]

        for a in CommonPlatform.PackagesPath:
            result.append(a)

        return result

    def GetActiveScopes (self):
        return CommonPlatform.Scopes

    def GetName (self):
        return "lancelotPkg"

    def GetLoggingLevel (self, loggerType):
        return logging.INFO
        return super().GetLoggingLevel(loggerType)

    def GetLatestCommit (self):
        try:
            result = subprocess.run (
                ['git', 'describe', '--tags', '--always'],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            return result.stdout.strip()
        except:
            return "Unknown"

    def SetPlatformEnv (self):
        logging.debug ("PlatformBuilder SetPlatformEnv")

        git_commit = self.GetLatestCommit ()

        self.env.SetValue ("PRODUCT_NAME", "lancelot", "Platform Hardcoded")
        self.env.SetValue ("ACTIVE_PLATFORM", "lancelotPkg/lancelot.dsc", "Platform Hardcoded")
        self.env.SetValue ("TARGET_ARCH", "AARCH64", "Platform Hardcoded")
        self.env.SetValue ("TOOL_CHAIN_TAG", "CLANG38", "set default to clang38")
        self.env.SetValue ("EMPTY_DRIVE", "FALSE", "Default to false")
        self.env.SetValue ("RUN_TESTS", "FALSE", "Default to false")
        self.env.SetValue ("SHUTDOWN_AFTER_RUN", "FALSE", "Default to false")
        self.env.SetValue ("BLD_*_BUILDID_STRING", "Unknown", "Default")
        self.env.SetValue ("BUILDREPORTING", "TRUE", "Enabling build report")
        self.env.SetValue ("BUILDREPORT_TYPES", "PCD DEPEX FLASH BUILD_FLAGS LIBRARY FIXED_ADDRESS HASH", "Setting build report types")
        self.env.SetValue ("BLD_*_FD_BASE", "0x40080000", "Default")
        self.env.SetValue ("BLD_*_FD_SIZE", "0x00200000", "Default")
        self.env.SetValue ("BLD_*_FIRMWARE_VER", git_commit, "Latest commit")

        return 0

    def PlatformPreBuild (self):
        return 0

    def PlatformGetOutDir (self):
        target = self.env.GetValue("TARGET")
        toolchain = self.env.GetValue("TOOL_CHAIN_TAG")
        output_dir = self.env.GetValue("OUTPUT_DIRECTORY")
        return os.path.join(output_dir, f"{target}_{toolchain}")

    def PlatformCompressFd (self):
        edk2_logging.log_progress("Compressing FD")

        platform = self.env.GetValue("PRODUCT_NAME")
        output_dir = self.PlatformGetOutDir()
        fd_path = os.path.join(output_dir, "FV", f"{platform.upper()}_UEFI.fd")
        fd_gz_path = fd_path + ".gz"

        try:
            with open(fd_path, 'rb') as f_in:
                with gzip.open(fd_gz_path, 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)
        except Exception as e:
            logging.critical(f"Gzip compression failed: {e}")
            return 1

        return 0

    def PlatformBuildBootImage (self):
        edk2_logging.log_progress("Building boot.img")

        platform = self.env.GetValue("PRODUCT_NAME")
        bootimg_out = os.path.join("Build", f"boot-{platform}.img")

        fd_path = os.path.join(self.PlatformGetOutDir(), "FV", f"{platform.upper()}_UEFI.fd.gz")

        platform_dir = os.path.dirname(os.path.abspath(__file__))
        blob_dir = os.path.join(platform_dir, "Blob")
        dtb = os.path.join(blob_dir, "mt6769-xiaomi-lancelot.dtb")
        ramdisk = os.path.join(blob_dir, "ramdisk")

        try:
            subprocess.run(
                ["mkbootimg",
                "--cmdline", "bootopt=64S3,32N2,64N2",
                "--header_version", "2",
                "--pagesize", "2048",
                "--base", "0x40078000",
                "--kernel_offset", "0x8000",
                "--second_offset", "0xbff88000",
                "--dtb_offset", "0xbc08000",
                "--tags_offset", "0xbc08000",
                "--ramdisk_offset", "0x7c08000",
                "--os_version", "15.0.0",
                "--os_patch_level", "2025-05",
                "--kernel", fd_path,
                "--dtb", dtb,
                "--ramdisk", ramdisk,
                "--output", bootimg_out],
                check=True
            )
        except subprocess.CalledProcessError:
            logging.critical(f"boot.img build failed!")
            return 1

        logging.info("=========== SUCCESS! ===========")
        logging.info("Output boot.img written to: " + bootimg_out)
        logging.info("================================")

        return 0

    def PlatformPostBuild (self):
        steps = [
            self.PlatformCompressFd,
            self.PlatformBuildBootImage
        ]

        for step in steps:
            status = step()
            if status:
                return status

        return 0

    def FlashRomImage (self):
        return 0

if __name__ == "__main__":
    import argparse
    import sys

    from edk2toolext.invocables.edk2_platform_build import Edk2PlatformBuild
    from edk2toolext.invocables.edk2_setup import Edk2PlatformSetup
    from edk2toolext.invocables.edk2_update import Edk2Update

    SCRIPT_PATH = os.path.relpath (__file__)

    parser = argparse.ArgumentParser (add_help=False)

    parse_group = parser.add_mutually_exclusive_group()

    parse_group.add_argument ("--update", "--UPDATE", action='store_true', help="Invokes stuart_update")
    parse_group.add_argument ("--setup",  "--SETUP",  action='store_true', help="Invokes stuart_setup")

    args, remaining = parser.parse_known_args()

    new_args = ["stuart", "-c", SCRIPT_PATH]
    new_args = new_args + remaining

    sys.argv = new_args

    if args.setup:
        Edk2PlatformSetup().Invoke()
    elif args.update:
        Edk2Update().Invoke()
    else:
        Edk2PlatformBuild().Invoke()
