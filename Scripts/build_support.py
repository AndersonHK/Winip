"""Shared repository and Visual Studio discovery for build and packaging tools."""
import os
from pathlib import Path
import subprocess

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


def child_environment():
    """Avoid duplicate Path/PATH keys without changing the parent environment."""
    return {key.upper(): value for key, value in os.environ.items()}


def find_msbuild(environment):
    program_files = environment.get('PROGRAMFILES(X86)')
    if not program_files:
        raise RuntimeError('Building Winip requires Windows and Visual Studio C++ tools.')
    vswhere = Path(program_files) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        raise RuntimeError('Visual Studio Installer was not found. Install the C++ workload first.')
    installation = subprocess.check_output(
        [str(vswhere), '-latest', '-products', '*', '-requires',
         'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
         '-property', 'installationPath'], env=environment, text=True).strip()
    if not installation:
        raise RuntimeError('No Visual Studio installation with C++ build tools was found.')
    msbuild = Path(installation) / 'MSBuild/Current/Bin/MSBuild.exe'
    if not msbuild.is_file():
        raise RuntimeError('MSBuild is missing from the selected Visual Studio installation.')
    return msbuild
