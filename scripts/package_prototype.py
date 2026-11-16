#!/usr/bin/env python3
"""Bundle the screen-only prototype for Figma without copying shared source.

Example:
    python3 scripts/package_prototype.py --output /tmp/esp32-prototype.archive

The output is a gzip tar archive, regardless of its filename extension.
Use --directory /tmp/esp32-prototype to also write a fresh static preview
directory. Both destinations must be outside this repository.
"""

import argparse
import gzip
import io
from pathlib import Path
import stat
import tarfile


ROOT = Path(__file__).resolve().parents[1]
PROTOTYPE_FILES = ("index.html", "prototype.css", "prototype.js")
SHARED_FILES = (
    "poetry-data.js", "engine.js", "library-store.js", "display.js",
    "atmosphere.js", "atmosphere.css", "background.bundle.js",
)


def read_source(relative):
    """Read only regular, unlinked source files from the known manifest."""
    source = ROOT / relative
    for part in (source, *source.parents):
        if part == ROOT:
            break
        if part.is_symlink():
            raise ValueError(f"Source must not be a symbolic link: {relative}")
    metadata = source.stat()
    if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
        raise ValueError(f"Source must be a regular file without hard links: {relative}")
    return source.read_bytes()


def collect_files():
    files = {name: read_source(Path("prototype") / name) for name in PROTOTYPE_FILES}
    html = files["index.html"].decode("utf-8")
    for quote in ('"', "'"):
        for attribute in ("src", "href"):
            html = html.replace(f"{attribute}={quote}../sandbox/", f"{attribute}={quote}sandbox/")
    files["index.html"] = html.encode("utf-8")
    for name in SHARED_FILES:
        files[f"sandbox/{name}"] = read_source(Path("sandbox") / name)
    readme = ROOT / "prototype" / "README.md"
    if readme.exists() or readme.is_symlink():
        files["README.md"] = read_source(Path("prototype") / "README.md")
    return files


def destination(value, label):
    path = Path(value).expanduser()
    if path.is_symlink():
        raise ValueError(f"{label} must not be a symbolic link")
    path = path.resolve()
    if path == ROOT or ROOT in path.parents:
        raise ValueError(f"{label} must be outside the repository")
    if not path.parent.is_dir():
        raise ValueError(f"{label} parent directory does not exist: {path.parent}")
    return path


def package(output, directory=None):
    files = collect_files()
    output = destination(output, "Archive")
    if output.exists():
        metadata = output.stat()
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_nlink != 1:
            raise ValueError("Existing archive must be a regular file without hard links")
    if directory is not None:
        directory = destination(directory, "Preview directory")
        if directory.exists():
            raise ValueError(f"Preview directory already exists; choose a fresh path: {directory}")
        if output == directory or directory in output.parents:
            raise ValueError("Archive must be outside the preview directory")

    # Construct and inspect the complete archive in memory before writing it.
    archive_bytes = io.BytesIO()
    with gzip.GzipFile(fileobj=archive_bytes, mode="wb", mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w") as archive:
            for name, data in sorted(files.items()):
                entry = tarfile.TarInfo(name)
                entry.size = len(data)
                entry.mode = 0o644
                archive.addfile(entry, io.BytesIO(data))
    archive_bytes.seek(0)
    with tarfile.open(fileobj=archive_bytes, mode="r:gz") as archive:
        entries = archive.getmembers()
        if {entry.name for entry in entries} != set(files) or not all(entry.isfile() for entry in entries):
            raise ValueError("Archive validation failed")
    output.write_bytes(archive_bytes.getvalue())
    if directory is not None:
        directory.mkdir()
        for name, data in files.items():
            target = directory / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    return output, directory, files


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", required=True, help="Archive path outside the repository")
    parser.add_argument("--directory", help="Also create this new static preview directory outside the repository")
    args = parser.parse_args()
    try:
        output, directory, files = package(args.output, args.directory)
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(1, f"Prototype packaging failed: {error}\n")
    print(f"Created {output} ({len(files)} regular files)")
    if directory is not None:
        print(f"Static preview directory: {directory}")


if __name__ == "__main__":
    main()
