import json
import tempfile
import unittest
from pathlib import Path

from scripts.check_release_version import VersionError, validate_release_version


class ReleaseVersionTests(unittest.TestCase):
    def write_versions(self, root: Path, json_version: str, properties_version: str) -> None:
        (root / "library.json").write_text(
            json.dumps({"name": "Flow", "version": json_version}),
            encoding="utf-8",
        )
        (root / "library.properties").write_text(
            f"name=Flow\nversion={properties_version}\n",
            encoding="utf-8",
        )

    def test_matching_versions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_versions(root, "0.1.0", "0.1.0")
            self.assertEqual(validate_release_version(root, "v0.1.0"), "0.1.0")

    def test_matching_prerelease_versions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_versions(root, "0.2.0-rc.1", "0.2.0-rc.1")
            self.assertEqual(
                validate_release_version(root, "v0.2.0-rc.1"),
                "0.2.0-rc.1",
            )

    def test_tag_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_versions(root, "0.1.0", "0.1.0")
            with self.assertRaisesRegex(VersionError, "requires version 0.2.0"):
                validate_release_version(root, "v0.2.0")

    def test_manifest_versions_must_match_each_other(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_versions(root, "0.1.0", "0.2.0")
            with self.assertRaisesRegex(VersionError, "library.properties=0.2.0"):
                validate_release_version(root, "v0.1.0")

    def test_invalid_tag_format(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write_versions(root, "0.1.0", "0.1.0")
            with self.assertRaisesRegex(VersionError, "invalid release tag"):
                validate_release_version(root, "0.1.0")

    def test_missing_version_field(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "library.json").write_text(
                json.dumps({"name": "Flow"}),
                encoding="utf-8",
            )
            (root / "library.properties").write_text("name=Flow\n", encoding="utf-8")
            with self.assertRaisesRegex(VersionError, "missing string version"):
                validate_release_version(root, "v0.1.0")


if __name__ == "__main__":
    unittest.main()
