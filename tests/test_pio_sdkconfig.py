"""Check the migration of existing PlatformIO-generated ESP-IDF settings."""

import pathlib
import tempfile
import unittest

import scripts.pio_config


class SoftwareAesConfigTests(unittest.TestCase):
    """Keep software AES enabled without discarding other SDK settings."""

    def test_existing_hardware_setting_is_disabled(self) -> None:
        """Replace the AES option and preserve unrelated configuration."""
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "sdkconfig.crowpanel_app"
            config.write_text(
                "CONFIG_OTHER=y\nCONFIG_MBEDTLS_HARDWARE_AES=y\n",
                encoding="utf-8",
            )
            self.assertTrue(scripts.pio_config.enable_software_aes(config))
            expected = "CONFIG_OTHER=y\n# CONFIG_MBEDTLS_HARDWARE_AES is not set\n"
            self.assertEqual(config.read_text(encoding="utf-8"), expected)
            self.assertFalse(scripts.pio_config.enable_software_aes(config))

    def test_missing_option_is_added_without_losing_settings(self) -> None:
        """Add the required option to an older generated configuration."""
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "sdkconfig.crowpanel_app"
            config.write_text("CONFIG_OTHER=y\n", encoding="utf-8")
            self.assertTrue(scripts.pio_config.enable_software_aes(config))
            expected = "CONFIG_OTHER=y\n# CONFIG_MBEDTLS_HARDWARE_AES is not set\n"
            self.assertEqual(config.read_text(encoding="utf-8"), expected)

    def test_missing_config_is_left_for_idf_to_create(self) -> None:
        """Do not create an incomplete sdkconfig before a fresh build."""
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "sdkconfig.crowpanel_app"
            self.assertFalse(scripts.pio_config.enable_software_aes(config))
            self.assertFalse(config.exists())


class XmlConfigTests(unittest.TestCase):
    """Remove the unused XML runtime without losing unrelated options."""

    def test_existing_xml_options_are_disabled(self) -> None:
        """Disable XML, keep the observer enabled, and make migration idempotent."""
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "sdkconfig.crowpanel_app"
            config.write_text(
                "CONFIG_OTHER=y\nCONFIG_LV_USE_XML=y\n# CONFIG_LV_USE_OBSERVER is not set\n",
                encoding="utf-8",
            )
            self.assertTrue(scripts.pio_config.disable_xml_runtime(config))
            expected = (
                "CONFIG_OTHER=y\n# CONFIG_LV_USE_XML is not set\n"
                "CONFIG_LV_USE_OBSERVER=y\n"
            )
            self.assertEqual(config.read_text(encoding="utf-8"), expected)
            self.assertFalse(scripts.pio_config.disable_xml_runtime(config))

    def test_missing_config_is_not_created(self) -> None:
        """Leave fresh configuration to ESP-IDF and its defaults."""
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "sdkconfig.crowpanel_app"
            self.assertFalse(scripts.pio_config.disable_xml_runtime(config))
            self.assertFalse(config.exists())


if __name__ == "__main__":
    unittest.main()
