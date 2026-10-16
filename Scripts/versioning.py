"""Stable release identity shared by package construction and validation."""
import uuid
from build_support import REPOSITORY_ROOT

UPGRADE_CODE = uuid.UUID('c99b13c5-f87d-45da-9e97-80900ba0b3da')


def current_version():
    return (REPOSITORY_ROOT / 'VERSION').read_text(encoding='utf-8').strip()


def product_code(version):
    # Preserve this namespace/name scheme: installed MSI maintenance depends on it.
    identity = uuid.uuid5(UPGRADE_CODE, 'Winip/x64/' + version)
    return '{' + str(identity).upper() + '}'
