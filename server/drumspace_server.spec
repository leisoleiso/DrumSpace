# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec for the DrumSpace musicvae server.
#
# Build with:
#   pyinstaller --clean --noconfirm drumspace_server.spec
#
# The model checkpoint is NOT bundled: it lives next to the exe in
# checkpoints/ (see server.py's _base_dir()).

import os

block_cipher = None

# Magenta / TF hidden imports that PyInstaller can't auto-detect.
hiddenimports = [
    'magenta.models.music_vae',
    'magenta.models.music_vae.configs',
    'magenta.models.music_vae.lstm_models',
    'magenta.models.music_vae.data',
    'magenta.models.music_vae.base_model',
    'magenta.music.drums_encoder_decoder',
    'magenta.music.drums_lib',
    'magenta.music.events_lib',
    'magenta.music.midi_io',
    'magenta.music.protobuf.music_pb2',
    'magenta.pipelines.drum_pipelines',
    'magenta.pipelines.statistics',
    'magenta.common.sequence_example_lib',
    'tensorflow.python.ops.numpy_ops',
    'tensorflow.python.client',
    'tensorflow.python.framework',
    'astor',
    'gast',
]

a = Analysis(
    ['server.py'],
    pathex=[os.path.abspath('.')],
    binaries=[],
    datas=[],
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='drumspace_server',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
