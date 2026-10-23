"""PEP 517 release-wheel delivery with the project's original source fallback."""
import hashlib
import importlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import sysconfig

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / 'binary_wheels.json'


def _manifest():
    return json.loads(MANIFEST.read_text(encoding='utf8'))


def _normalized_project(path):
    import tomllib
    data = tomllib.loads(path.read_text(encoding='utf8'))
    build = data.get('build-system', {})
    build.pop('build-backend', None)
    build.pop('backend-path', None)
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()


def _asset(config_settings=None):
    if (config_settings or sys.version_info[:2] != (3, 15)
            or not sysconfig.get_config_var('Py_GIL_DISABLED')
            or sys.platform != 'linux' or platform.machine() != 'x86_64'):
        return None
    if any(os.environ.get(name) for name in
           ('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'CC', 'CXX')):
        return None
    data = _manifest()
    libc, version = platform.libc_ver()
    if (libc != 'glibc'
            or tuple(map(int, version.split('.'))) < tuple(data['minimum_glibc'])):
        return None
    for name, digest in data['delivery_source_hashes'].items():
        path = ROOT / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            return None
    if _normalized_project(ROOT / 'pyproject.toml') != data['source_project']:
        return None
    paths = ['.', *[':(exclude)' + p for p in data['delivery_paths']]]
    try:
        result = subprocess.run(
            ['git', '-C', str(ROOT), 'diff', '--quiet', data['source_commit'], '--', *paths],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        if result.returncode:
            return None
        untracked = subprocess.check_output(
            ['git', '-C', str(ROOT), 'ls-files', '--others', '--exclude-standard'],
            universal_newlines=True, timeout=30).splitlines()
        if any(p not in data['delivery_paths'] and not (
                p.startswith('__pycache__/_binary_wheel_backend.') and p.endswith('.pyc'))
               for p in untracked):
            return None
    except (OSError, subprocess.SubprocessError):
        return None
    return data['artifact']


def _source():
    data = _manifest()
    if data.get('initialize_source_submodules'):
        subprocess.check_call(
            ['git', 'submodule', 'update', '--init', '--recursive', '--checkout'], cwd=str(ROOT))
    for path in reversed(data['source_backend_path']):
        sys.path.insert(0, str(ROOT / path))
    module, separator, attribute = data['source_backend'].partition(':')
    backend = importlib.import_module(module)
    return getattr(backend, attribute) if separator else backend


def get_requires_for_build_wheel(config_settings=None):
    if _asset(config_settings):
        return ['pip']
    return _source().get_requires_for_build_wheel(config_settings)


def build_wheel(wheel_directory, config_settings=None, metadata_directory=None):
    asset = _asset(config_settings)
    if asset is None:
        return _source().build_wheel(wheel_directory, config_settings, metadata_directory)
    target = Path(wheel_directory).resolve()
    target.mkdir(parents=True, exist_ok=True)
    subprocess.check_call(
        [sys.executable, '-m', 'pip', 'download', '--no-deps', '--only-binary=:all:',
         '--dest', str(target), asset['url'] + '#sha256=' + asset['sha256']])
    return asset['filename']


def __getattr__(name):
    # Let pip obtain metadata from its retained wheel, avoiding a second download.
    if name == 'prepare_metadata_for_build_wheel' and _asset() is not None:
        raise AttributeError(name)
    return getattr(_source(), name)