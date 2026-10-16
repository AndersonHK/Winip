"""Build Winip or its model/GPU tests using the installed Visual Studio toolchain."""
import argparse
import re
import subprocess
from build_support import REPOSITORY_ROOT, child_environment, find_msbuild
from versioning import current_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('configuration', nargs='?', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--tests', action='store_true', help='Build and run the model/GPU suite.')
    names = parser.add_mutually_exclusive_group()
    names.add_argument('--name', default='Winip', help='Application executable basename.')
    names.add_argument('--next', action='store_true', help='Use the Winip-next executable name.')
    arguments = parser.parse_args()
    target_name = 'Winip-next' if arguments.next else arguments.name
    if not re.fullmatch(r'[A-Za-z0-9_-]+', target_name):
        parser.error('--name must contain only letters, digits, hyphens and underscores.')
    resource = (REPOSITORY_ROOT / 'Src/ProductVersion.rc').read_text(encoding='utf-8')
    if '"' + current_version() + r'\0"' not in resource:
        raise RuntimeError('VERSION and Src/ProductVersion.rc disagree')
    environment = child_environment()
    project = REPOSITORY_ROOT / ('Tests/WinipTests.vcxproj' if arguments.tests else 'Winip.sln')
    command = [str(find_msbuild(environment)), str(project), '/nologo', '/verbosity:minimal',
               '/p:Platform=x64', '/p:Configuration=' + arguments.configuration]
    if not arguments.tests:
        command.append('/p:TargetName=' + target_name)
    subprocess.run(command, cwd=REPOSITORY_ROOT, env=environment, check=True)
    if arguments.tests:
        executable = REPOSITORY_ROOT / 'Out/Tests/x64' / arguments.configuration / 'WinipTests.exe'
        subprocess.run([str(executable), str(REPOSITORY_ROOT)],
                       cwd=REPOSITORY_ROOT, env=environment, check=True)


if __name__ == '__main__':
    main()
