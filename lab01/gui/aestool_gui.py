#!/usr/bin/env python3
"""Modern PyQt6 GUI for the aestool crypto core.

Every cryptographic operation and every randomness request goes through the
compiled shared library (aescore.dll / libaescore.so) via the C ABI in
src/aesgui.h. The only third-party Python package required is PyQt6:

    pip install PyQt6

Build the shared library first (from lab01/):

    Windows: gui\\build_gui.bat
    Linux:   ./gui/build_gui.sh

Then run:

    python gui/aestool_gui_qt.py
    python gui/aestool_gui_qt.py --self-test      # no window, ABI check only

Highlights
  * Light / dark theme (remembered between runs)
  * Mode picker with live key / IV validation and security badges
  * Encrypt / decrypt run on a worker thread (UI never freezes on big files)
  * Drag & drop any file onto the window
  * "Use as input" to chain encrypt -> decrypt quickly
  * Shortcuts: Ctrl+Enter encrypt, Ctrl+Shift+Enter decrypt, Ctrl+L clear
"""

from __future__ import annotations

import argparse
import base64
import codecs
import ctypes
import os
import sys
from ctypes import POINTER, byref, c_char_p, c_int, c_size_t, c_ubyte, c_void_p
from pathlib import Path
from string import Template

try:
    from PyQt6.QtCore import QSettings, Qt, QThread, pyqtSignal
    from PyQt6.QtGui import QFontDatabase, QGuiApplication, QKeySequence, QShortcut
    from PyQt6.QtWidgets import (
        QApplication,
        QButtonGroup,
        QFileDialog,
        QFrame,
        QGridLayout,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPlainTextEdit,
        QProgressBar,
        QPushButton,
        QRadioButton,
        QSizePolicy,
        QSplitter,
        QTabWidget,
        QVBoxLayout,
        QWidget,
    )
except ImportError as error:  # pragma: no cover - depends on the host Python
    print(f"PyQt6 is required for the GUI: {error}", file=sys.stderr)
    print("Install it with:  pip install PyQt6", file=sys.stderr)
    raise SystemExit(1)

ROOT = Path(__file__).resolve().parent.parent
MODES = ("ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm")
ECB_LIMIT = 16 * 1024
PREVIEW_LIMIT = 256 * 1024
HEX_DIGITS = set("0123456789abcdefABCDEF")

MODE_BLURB = {
    "ecb": "Electronic Codebook — leaks patterns, avoid for real data",
    "cbc": "Cipher Block Chaining — needs a random IV, no authentication",
    "ofb": "Output Feedback — stream-like, no authentication",
    "cfb": "Cipher Feedback — stream-like, no authentication",
    "ctr": "Counter — stream-like, never reuse a nonce, no authentication",
    "xts": "XEX tweakable — disk encryption, double-length key",
    "ccm": "Counter with CBC-MAC — authenticated (AEAD)",
    "gcm": "Galois/Counter — authenticated (AEAD), recommended",
}

U8P = POINTER(c_ubyte)


class LibraryError(RuntimeError):
    """Raised when the compiled library reports an error."""


# --------------------------------------------------------------------------
# C ABI binding (unchanged behaviour from the Tkinter version)
# --------------------------------------------------------------------------
def _library_candidates(explicit: str | None):
    if explicit:
        yield Path(explicit)
    env = os.environ.get("AESGUI_LIBRARY")
    if env:
        yield Path(env)

    if sys.platform == "win32":
        names = ("aescore.dll",)
        directories = (
            ROOT / "bin",
            ROOT / "build" / "windows-msvc-shared" / "Release",
            ROOT / "build" / "windows-msvc-shared",
            ROOT / "build" / "windows-msvc" / "Release",
            ROOT / "build" / "windows-msvc",
            ROOT / "build",
        )
    else:
        names = ("libaescore.so", "aescore.so")
        directories = (
            ROOT / "bin",
            ROOT / "lib",
            ROOT / "build" / "ubuntu-gcc-shared",
            ROOT / "build" / "ubuntu-gcc-shared" / "Release",
            ROOT / "build" / "ubuntu-clang-shared",
            ROOT / "build" / "ubuntu-clang-shared" / "Release",
            ROOT / "build",
            ROOT,
        )

    for directory in directories:
        for name in names:
            yield directory / name


def find_library(explicit: str | None = None) -> Path:
    for candidate in _library_candidates(explicit):
        if candidate.is_file():
            return candidate.resolve()
    raise LibraryError(
        "cannot find the aescore shared library.\n\n"
        "Build it first:\n"
        "  Windows: gui\\build_gui.bat\n"
        "  Linux:   ./gui/build_gui.sh\n\n"
        "Or pass --library PATH / set AESGUI_LIBRARY."
    )


class AesCore:
    """ctypes binding for the C ABI exported by aescore."""

    def __init__(self, path: str | Path):
        self.path = Path(path).resolve()
        if sys.platform == "win32":
            # Python 3.8+ no longer searches PATH for extension dependencies.
            # Keep the handle alive for the lifetime of this object.
            self._dll_directory = os.add_dll_directory(str(self.path.parent))
        self.lib = ctypes.CDLL(str(self.path))
        self._size_cache: dict[str, tuple[int, int, bool]] = {}
        self._bind()

    def _bind(self) -> None:
        lib = self.lib
        lib.aesgui_encrypt.argtypes = [
            c_char_p, c_char_p, c_char_p, c_char_p, c_char_p,
            c_size_t, c_int, POINTER(U8P), POINTER(c_size_t),
        ]
        lib.aesgui_encrypt.restype = c_int
        lib.aesgui_decrypt.argtypes = [
            c_char_p, c_char_p, c_char_p, c_char_p, c_char_p,
            c_size_t, POINTER(U8P), POINTER(c_size_t),
        ]
        lib.aesgui_decrypt.restype = c_int
        lib.aesgui_free.argtypes = [c_void_p]
        lib.aesgui_free.restype = None
        lib.aesgui_last_error.argtypes = []
        lib.aesgui_last_error.restype = c_char_p
        lib.aesgui_random_hex.argtypes = [c_int, c_char_p, c_size_t]
        lib.aesgui_random_hex.restype = c_int
        lib.aesgui_record_info.argtypes = [
            c_char_p, c_size_t, c_char_p, c_size_t, c_char_p, c_size_t,
            POINTER(c_int), POINTER(c_int),
        ]
        lib.aesgui_record_info.restype = c_int
        lib.aesgui_mode_sizes.argtypes = [
            c_char_p, POINTER(c_int), POINTER(c_int), POINTER(c_int)
        ]
        lib.aesgui_mode_sizes.restype = c_int

    def _check(self, code: int) -> None:
        if code == 0:
            return
        raw = self.lib.aesgui_last_error()
        message = raw.decode("utf-8", "replace") if raw else "unknown library error"
        raise LibraryError(message)

    def random_hex(self, byte_count: int) -> str:
        buffer = ctypes.create_string_buffer(byte_count * 2 + 1)
        self._check(self.lib.aesgui_random_hex(byte_count, buffer, len(buffer)))
        return buffer.value.decode("ascii")

    def mode_sizes(self, mode: str) -> tuple[int, int, bool]:
        cached = self._size_cache.get(mode)
        if cached is not None:
            return cached
        iv_len = c_int()
        tag_len = c_int()
        aead = c_int()
        self._check(self.lib.aesgui_mode_sizes(
            mode.encode("ascii"), byref(iv_len), byref(tag_len), byref(aead)))
        result = (iv_len.value, tag_len.value, bool(aead.value))
        self._size_cache[mode] = result
        return result

    def encrypt(self, mode: str, key_hex: str, iv_hex: str | None, aad_hex: str | None,
                data: bytes, pkcs7: bool = True) -> bytes:
        record = U8P()
        length = c_size_t()
        self._check(self.lib.aesgui_encrypt(
            mode.encode("ascii"),
            key_hex.encode("ascii"),
            iv_hex.encode("ascii") if iv_hex else None,
            aad_hex.encode("ascii") if aad_hex else None,
            data,
            len(data),
            1 if pkcs7 else 0,
            byref(record),
            byref(length),
        ))
        try:
            return ctypes.string_at(record, length.value)
        finally:
            self.lib.aesgui_free(record)

    def decrypt(self, mode: str, key_hex: str, iv_hex: str | None, aad_hex: str | None,
                record: bytes) -> bytes:
        plaintext = U8P()
        length = c_size_t()
        self._check(self.lib.aesgui_decrypt(
            mode.encode("ascii"),
            key_hex.encode("ascii"),
            iv_hex.encode("ascii") if iv_hex else None,
            aad_hex.encode("ascii") if aad_hex else None,
            record,
            len(record),
            byref(plaintext),
            byref(length),
        ))
        try:
            return ctypes.string_at(plaintext, length.value)
        finally:
            self.lib.aesgui_free(plaintext)

    def record_info(self, record: bytes) -> dict[str, object]:
        mode = ctypes.create_string_buffer(32)
        iv_hex = ctypes.create_string_buffer(512)
        tag_len = c_int()
        aad_len = c_int()
        self._check(self.lib.aesgui_record_info(
            record,
            len(record),
            mode,
            len(mode),
            iv_hex,
            len(iv_hex),
            byref(tag_len),
            byref(aad_len),
        ))
        return {
            "mode": mode.value.decode("ascii").lower(),
            "iv_hex": iv_hex.value.decode("ascii"),
            "tag_len": tag_len.value,
            "aad_len": aad_len.value,
        }


# --------------------------------------------------------------------------
# Container helpers
# --------------------------------------------------------------------------
def decode_container_text(text: str) -> bytes:
    compact = "".join(text.split())
    if not compact:
        raise ValueError("no ciphertext to decrypt")
    try:
        raw = base64.b64decode(compact, validate=True)
        if raw.startswith(b"AESL1"):
            return raw
    except Exception:
        pass
    try:
        raw = bytes.fromhex(compact)
        if raw.startswith(b"AESL1"):
            return raw
    except ValueError:
        pass
    raise ValueError("ciphertext must be an AESL1 container encoded as hex or base64")


def read_container_file(path: str) -> bytes:
    data = Path(path).read_bytes()
    if data.startswith(b"AESL1"):
        return data
    try:
        return decode_container_text(data.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as error:
        raise ValueError(f"{path} is not an AESL1 container: {error}") from error


def human_size(count: int) -> str:
    value = float(count)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024 or unit == "GiB":
            return f"{int(value)} {unit}" if unit == "B" else f"{value:.1f} {unit}"
        value /= 1024
    return f"{count} B"


# --------------------------------------------------------------------------
# Theming
# --------------------------------------------------------------------------
THEMES = {
    "dark": dict(
        bg="#0f1218", card="#171b24", border="#272e3d", text="#e7eaf1",
        muted="#8a93a8", input="#0f1218", btn="#212838", btnHover="#2a3246",
        accent="#5b8def", accentHover="#7aa2f7", accentText="#ffffff",
        ok="#3ecf8e", okBg="#12301f", bad="#ff6b6b", badBg="#3a1a1f",
        warn="#f2b84b", warnBg="#3a2e12", info="#7aa2f7", infoBg="#16233f",
    ),
    "light": dict(
        bg="#f3f5f9", card="#ffffff", border="#dde2ec", text="#1b2130",
        muted="#677089", input="#fbfcfe", btn="#eef1f7", btnHover="#e2e7f1",
        accent="#3b6fe0", accentHover="#2f5bc2", accentText="#ffffff",
        ok="#12805a", okBg="#e2f6ec", bad="#c53030", badBg="#fde8e8",
        warn="#92600a", warnBg="#fdf1d6", info="#2a55b8", infoBg="#e6eeff",
    ),
}

QSS = Template("""
* { font-family: "Segoe UI", "Inter", "Ubuntu", "Noto Sans", sans-serif; font-size: 13px; }
QMainWindow, QWidget#root { background: $bg; }
QWidget { color: $text; }
QLabel { background: transparent; }
QToolTip { background: $card; color: $text; border: 1px solid $border; padding: 4px 6px; }

QFrame#card { background: $card; border: 1px solid $border; border-radius: 12px; }
QLabel#title { font-size: 21px; font-weight: 700; }
QLabel#subtitle, QLabel#muted { color: $muted; }
QLabel#section { font-size: 11px; font-weight: 700; color: $muted; }
QLabel#metaLine { color: $muted; font-size: 12px; }

QLineEdit, QPlainTextEdit {
    background: $input; border: 1px solid $border; border-radius: 8px;
    padding: 7px 10px; selection-background-color: $accent; selection-color: $accentText;
}
QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid $accent; }
QLineEdit:disabled { color: $muted; background: $bg; }
QLineEdit[state="bad"] { border: 1px solid $bad; }

QPushButton {
    background: $btn; border: 1px solid $border; border-radius: 8px; padding: 7px 14px;
}
QPushButton:hover { background: $btnHover; }
QPushButton:pressed { background: $border; }
QPushButton:disabled { color: $muted; background: $bg; }
QPushButton#primary {
    background: $accent; border: 1px solid $accent; color: $accentText;
    font-weight: 600; padding: 10px 26px;
}
QPushButton#primary:hover { background: $accentHover; border-color: $accentHover; }
QPushButton#primary:disabled { background: $btn; color: $muted; border-color: $border; }
QPushButton#secondary {
    background: transparent; border: 1px solid $accent; color: $accent;
    font-weight: 600; padding: 10px 26px;
}
QPushButton#secondary:hover { background: $infoBg; }
QPushButton#secondary:disabled { border-color: $border; color: $muted; }
QPushButton#mode { padding: 8px 0; font-weight: 600; }
QPushButton#mode:checked { background: $accent; border-color: $accent; color: $accentText; }
QPushButton#ghost { background: transparent; border: 1px solid transparent; color: $muted; }
QPushButton#ghost:hover { background: $btn; color: $text; }
QPushButton#small { padding: 5px 10px; }

QTabWidget::pane { border: none; top: 6px; }
QTabBar::tab {
    background: transparent; padding: 7px 16px; color: $muted;
    border-bottom: 2px solid transparent; margin-right: 4px;
}
QTabBar::tab:hover { color: $text; }
QTabBar::tab:selected { color: $text; border-bottom: 2px solid $accent; font-weight: 600; }

QLabel#banner { border-radius: 8px; padding: 9px 12px; }
QLabel#banner[level="info"] { background: $infoBg; color: $info; }
QLabel#banner[level="ok"]   { background: $okBg;   color: $ok; }
QLabel#banner[level="warn"] { background: $warnBg; color: $warn; }
QLabel#banner[level="bad"]  { background: $badBg;  color: $bad; }

QLabel#warnBox { background: $warnBg; color: $warn; border-radius: 8px; padding: 8px 10px; }

QLabel#badge { border-radius: 9px; padding: 2px 9px; font-size: 11px; font-weight: 600; }
QLabel#badge[level="ok"]   { background: $okBg;   color: $ok; }
QLabel#badge[level="warn"] { background: $warnBg; color: $warn; }
QLabel#badge[level="bad"]  { background: $badBg;  color: $bad; }
QLabel#badge[level="info"] { background: $infoBg; color: $info; }
QLabel#badge[level="muted"] { background: $btn; color: $muted; }

QLabel#hint[level="ok"]  { color: $ok; }
QLabel#hint[level="bad"] { color: $bad; }
QLabel#hint[level="muted"] { color: $muted; }

QLabel#dropHint {
    border: 2px dashed $border; border-radius: 10px; color: $muted; padding: 22px;
}

QRadioButton { spacing: 6px; }
QRadioButton:disabled { color: $muted; }

QProgressBar { background: $btn; border: none; border-radius: 3px; max-height: 6px; }
QProgressBar::chunk { background: $accent; border-radius: 3px; }

QSplitter::handle { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: $border; border-radius: 4px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QMessageBox { background: $card; }
""")


def repolish(widget: QWidget) -> None:
    widget.style().unpolish(widget)
    widget.style().polish(widget)


def set_level(widget: QWidget, prop: str, value: str) -> None:
    widget.setProperty(prop, value)
    repolish(widget)


def make_card() -> tuple[QFrame, QVBoxLayout]:
    card = QFrame()
    card.setObjectName("card")
    layout = QVBoxLayout(card)
    layout.setContentsMargins(16, 14, 16, 16)
    layout.setSpacing(10)
    return card, layout


def section_label(text: str) -> QLabel:
    label = QLabel(text.upper())
    label.setObjectName("section")
    return label


# --------------------------------------------------------------------------
# Worker thread
# --------------------------------------------------------------------------
class Job(QThread):
    succeeded = pyqtSignal(object)
    failed = pyqtSignal(str)

    def __init__(self, fn, parent=None):
        super().__init__(parent)
        self._fn = fn

    def run(self) -> None:
        try:
            self.succeeded.emit(self._fn())
        except (LibraryError, ValueError, OSError) as error:
            self.failed.emit(str(error))
        except Exception as error:  # noqa: BLE001 - never kill the UI thread silently
            self.failed.emit(f"unexpected error: {error}")


# --------------------------------------------------------------------------
# Main window
# --------------------------------------------------------------------------
class AesWindow(QMainWindow):
    def __init__(self, core: AesCore):
        super().__init__()
        self.core = core
        self.settings = QSettings("aestool", "aestool-gui-qt")
        self.theme = str(self.settings.value("theme", "dark"))
        if self.theme not in THEMES:
            self.theme = "dark"

        self._mode = "gcm"
        self._busy = False
        self._job: Job | None = None
        self._output_bytes = b""
        self._output_is_cipher = True
        self._output_is_text = True
        self.mono = QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont)
        self.mono.setPointSize(10)

        self.setWindowTitle("aestool — AES encryption")
        self.setAcceptDrops(True)
        self.resize(1120, 800)
        self.setMinimumSize(900, 680)

        root = QWidget()
        root.setObjectName("root")
        self.setCentralWidget(root)
        outer = QVBoxLayout(root)
        outer.setContentsMargins(20, 16, 20, 12)
        outer.setSpacing(14)

        outer.addLayout(self._build_header())

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setChildrenCollapsible(False)
        splitter.addWidget(self._build_settings())
        splitter.addWidget(self._build_workspace())
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([380, 700])
        outer.addWidget(splitter, 1)

        self._build_status()
        self._build_shortcuts()

        self.apply_theme()
        self.set_mode("gcm")
        geometry = self.settings.value("geometry")
        if geometry is not None:
            self.restoreGeometry(geometry)

    # ------------------------------------------------------------- building
    def _build_header(self) -> QHBoxLayout:
        row = QHBoxLayout()
        titles = QVBoxLayout()
        titles.setSpacing(0)
        title = QLabel("AES Tool")
        title.setObjectName("title")
        subtitle = QLabel("Symmetric encryption · ECB CBC OFB CFB CTR XTS CCM GCM")
        subtitle.setObjectName("subtitle")
        titles.addWidget(title)
        titles.addWidget(subtitle)
        row.addLayout(titles)
        row.addStretch(1)

        self.lib_label = QLabel(f"Core: {self.core.path.name}")
        self.lib_label.setObjectName("muted")
        self.lib_label.setToolTip(str(self.core.path))
        row.addWidget(self.lib_label)

        self.theme_button = QPushButton()
        self.theme_button.setObjectName("small")
        self.theme_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.theme_button.clicked.connect(self.toggle_theme)
        row.addWidget(self.theme_button)
        return row

    def _build_settings(self) -> QWidget:
        card, layout = make_card()
        card.setMinimumWidth(340)
        card.setMaximumWidth(460)

        # Mode picker
        layout.addWidget(section_label("Mode"))
        grid = QGridLayout()
        grid.setSpacing(6)
        self.mode_group = QButtonGroup(self)
        self.mode_group.setExclusive(True)
        self.mode_buttons: dict[str, QPushButton] = {}
        for index, mode in enumerate(MODES):
            button = QPushButton(mode.upper())
            button.setObjectName("mode")
            button.setCheckable(True)
            button.setCursor(Qt.CursorShape.PointingHandCursor)
            button.setToolTip(MODE_BLURB[mode])
            self.mode_group.addButton(button)
            self.mode_buttons[mode] = button
            grid.addWidget(button, index // 4, index % 4)
            button.clicked.connect(lambda _checked=False, m=mode: self.set_mode(m))
        layout.addLayout(grid)

        self.mode_blurb = QLabel()
        self.mode_blurb.setObjectName("muted")
        self.mode_blurb.setWordWrap(True)
        layout.addWidget(self.mode_blurb)

        badges = QHBoxLayout()
        badges.setSpacing(6)
        self.badge_auth = self._badge()
        self.badge_iv = self._badge()
        self.badge_tag = self._badge()
        for badge in (self.badge_auth, self.badge_iv, self.badge_tag):
            badges.addWidget(badge)
        badges.addStretch(1)
        layout.addLayout(badges)

        self.warn_box = QLabel()
        self.warn_box.setObjectName("warnBox")
        self.warn_box.setWordWrap(True)
        layout.addWidget(self.warn_box)

        layout.addSpacing(4)

        # Key
        layout.addWidget(section_label("Key (hex)"))
        key_row = QHBoxLayout()
        key_row.setSpacing(6)
        self.key_edit = QLineEdit()
        self.key_edit.setFont(self.mono)
        self.key_edit.setEchoMode(QLineEdit.EchoMode.Password)
        self.key_edit.setPlaceholderText("hexadecimal key")
        self.key_edit.textChanged.connect(self._validate_key)
        key_row.addWidget(self.key_edit, 1)
        self.key_show = QPushButton("Show")
        self.key_show.setObjectName("small")
        self.key_show.setCheckable(True)
        self.key_show.toggled.connect(self._toggle_key_visibility)
        key_row.addWidget(self.key_show)
        layout.addLayout(key_row)

        key_actions = QHBoxLayout()
        key_actions.setSpacing(6)
        for text, handler in (
            ("Generate", self._generate_key),
            ("Load file…", self._load_key_file),
            ("Copy", self._copy_key),
        ):
            button = QPushButton(text)
            button.setObjectName("small")
            button.clicked.connect(handler)
            key_actions.addWidget(button)
        key_actions.addStretch(1)
        layout.addLayout(key_actions)

        self.key_hint = QLabel()
        self.key_hint.setObjectName("hint")
        layout.addWidget(self.key_hint)

        layout.addSpacing(4)

        # IV
        layout.addWidget(section_label("IV / nonce (used when encrypting)"))
        iv_row = QHBoxLayout()
        iv_row.setSpacing(6)
        self.iv_edit = QLineEdit()
        self.iv_edit.setFont(self.mono)
        self.iv_edit.setPlaceholderText("leave blank for a random IV")
        iv_row.addWidget(self.iv_edit, 1)
        self.iv_button = QPushButton("Generate")
        self.iv_button.setObjectName("small")
        self.iv_button.clicked.connect(self._generate_iv)
        iv_row.addWidget(self.iv_button)
        layout.addLayout(iv_row)

        # AAD
        layout.addWidget(section_label("AAD (hex, AEAD modes only)"))
        self.aad_edit = QLineEdit()
        self.aad_edit.setFont(self.mono)
        self.aad_edit.setPlaceholderText("optional additional authenticated data")
        layout.addWidget(self.aad_edit)

        layout.addStretch(1)
        return card

    def _badge(self) -> QLabel:
        label = QLabel()
        label.setObjectName("badge")
        label.setProperty("level", "muted")
        return label

    def _build_workspace(self) -> QWidget:
        holder = QWidget()
        layout = QVBoxLayout(holder)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)

        # Input card
        input_card, input_layout = make_card()
        input_layout.addWidget(section_label("Input"))
        self.input_tabs = QTabWidget()
        input_layout.addWidget(self.input_tabs)

        self.text_input = QPlainTextEdit()
        self.text_input.setFont(self.mono)
        self.text_input.setPlaceholderText(
            "Type or paste text to encrypt.\n"
            "To decrypt, paste an AESL1 container encoded as hex or base64.")
        self.input_tabs.addTab(self.text_input, "Text")

        file_tab = QWidget()
        file_layout = QVBoxLayout(file_tab)
        file_layout.setContentsMargins(0, 4, 0, 0)
        file_layout.setSpacing(8)
        self.drop_hint = QLabel(
            "Drop a file anywhere on this window\n"
            "Encryption reads raw bytes · decryption expects an AESL1 container")
        self.drop_hint.setObjectName("dropHint")
        self.drop_hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.drop_hint.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        file_layout.addWidget(self.drop_hint, 1)
        path_row = QHBoxLayout()
        path_row.setSpacing(6)
        self.path_edit = QLineEdit()
        self.path_edit.setFont(self.mono)
        self.path_edit.setPlaceholderText("path to input file")
        self.path_edit.textChanged.connect(self._path_changed)
        path_row.addWidget(self.path_edit, 1)
        browse = QPushButton("Browse…")
        browse.clicked.connect(self._browse_input)
        path_row.addWidget(browse)
        file_layout.addLayout(path_row)
        self.input_tabs.addTab(file_tab, "File")
        layout.addWidget(input_card, 5)

        # Actions
        actions = QHBoxLayout()
        actions.setSpacing(8)
        self.encrypt_button = QPushButton("Encrypt")
        self.encrypt_button.setObjectName("primary")
        self.encrypt_button.setToolTip("Ctrl+Enter")
        self.encrypt_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.encrypt_button.clicked.connect(self._encrypt)
        self.decrypt_button = QPushButton("Decrypt")
        self.decrypt_button.setObjectName("secondary")
        self.decrypt_button.setToolTip("Ctrl+Shift+Enter")
        self.decrypt_button.setCursor(Qt.CursorShape.PointingHandCursor)
        self.decrypt_button.clicked.connect(self._decrypt)
        self.clear_button = QPushButton("Clear")
        self.clear_button.setObjectName("ghost")
        self.clear_button.setToolTip("Ctrl+L")
        self.clear_button.clicked.connect(self._clear)
        actions.addWidget(self.encrypt_button)
        actions.addWidget(self.decrypt_button)
        actions.addWidget(self.clear_button)
        actions.addStretch(1)
        layout.addLayout(actions)

        self.banner = QLabel()
        self.banner.setObjectName("banner")
        self.banner.setWordWrap(True)
        self.banner.setProperty("level", "info")
        self.banner.hide()
        layout.addWidget(self.banner)

        # Output card
        output_card, output_layout = make_card()
        head = QHBoxLayout()
        head.addWidget(section_label("Output"))
        self.output_kind = QLabel("empty")
        self.output_kind.setObjectName("muted")
        head.addWidget(self.output_kind)
        head.addStretch(1)
        head.addWidget(QLabel("Format"))
        self.radio_hex = QRadioButton("Hex")
        self.radio_b64 = QRadioButton("Base64")
        self.radio_hex.setChecked(True)
        self.radio_hex.toggled.connect(self._refresh_output)
        head.addWidget(self.radio_hex)
        head.addWidget(self.radio_b64)
        output_layout.addLayout(head)

        self.output_text = QPlainTextEdit()
        self.output_text.setReadOnly(True)
        self.output_text.setFont(self.mono)
        self.output_text.setLineWrapMode(QPlainTextEdit.LineWrapMode.WidgetWidth)
        self.output_text.setPlaceholderText("The result appears here.")
        output_layout.addWidget(self.output_text, 1)

        self.meta_label = QLabel("")
        self.meta_label.setObjectName("metaLine")
        self.meta_label.setWordWrap(True)
        self.meta_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        output_layout.addWidget(self.meta_label)

        out_actions = QHBoxLayout()
        out_actions.setSpacing(6)
        self.copy_button = QPushButton("Copy")
        self.copy_button.clicked.connect(self._copy_output)
        self.save_button = QPushButton("Save…")
        self.save_button.clicked.connect(self._save_output)
        self.reuse_button = QPushButton("Use as input")
        self.reuse_button.setToolTip("Move the output into the input box (e.g. to test decryption)")
        self.reuse_button.clicked.connect(self._use_as_input)
        out_actions.addWidget(self.copy_button)
        out_actions.addWidget(self.save_button)
        out_actions.addWidget(self.reuse_button)
        out_actions.addStretch(1)
        output_layout.addLayout(out_actions)
        layout.addWidget(output_card, 5)

        self._update_output_buttons()
        return holder

    def _build_status(self) -> None:
        bar = self.statusBar()
        bar.setSizeGripEnabled(False)
        self.progress = QProgressBar()
        self.progress.setRange(0, 0)
        self.progress.setFixedWidth(140)
        self.progress.setTextVisible(False)
        self.progress.hide()
        bar.addPermanentWidget(self.progress)
        bar.showMessage("Ready.")

    def _build_shortcuts(self) -> None:
        for sequence, handler in (
            ("Ctrl+Return", self._encrypt),
            ("Ctrl+Enter", self._encrypt),
            ("Ctrl+Shift+Return", self._decrypt),
            ("Ctrl+Shift+Enter", self._decrypt),
            ("Ctrl+L", self._clear),
        ):
            shortcut = QShortcut(QKeySequence(sequence), self)
            shortcut.activated.connect(handler)

    # --------------------------------------------------------------- theming
    def apply_theme(self) -> None:
        app = QApplication.instance()
        if app is not None:
            app.setStyleSheet(QSS.substitute(THEMES[self.theme]))
        self.theme_button.setText("☀  Light" if self.theme == "dark" else "☾  Dark")

    def toggle_theme(self) -> None:
        self.theme = "light" if self.theme == "dark" else "dark"
        self.settings.setValue("theme", self.theme)
        self.apply_theme()

    # ------------------------------------------------------------ UI helpers
    def notify(self, text: str, level: str = "info") -> None:
        if not text:
            self.banner.hide()
            return
        self.banner.setText(text)
        set_level(self.banner, "level", level)
        self.banner.show()

    def status(self, text: str, timeout: int = 0) -> None:
        self.statusBar().showMessage(text, timeout)

    def _set_badge(self, badge: QLabel, text: str, level: str) -> None:
        badge.setText(text)
        set_level(badge, "level", level)

    def _set_busy(self, busy: bool, label: str = "") -> None:
        self._busy = busy
        for button in (self.encrypt_button, self.decrypt_button):
            button.setEnabled(not busy)
        self.progress.setVisible(busy)
        if busy:
            self.status(label)

    # ------------------------------------------------------------------ mode
    @property
    def mode(self) -> str:
        return self._mode

    def set_mode(self, mode: str) -> None:
        self._mode = mode
        self.mode_buttons[mode].setChecked(True)
        self._mode_changed()

    def _mode_changed(self) -> None:
        mode = self._mode
        try:
            iv_len, tag_len, aead = self.core.mode_sizes(mode)
        except LibraryError as error:
            self.notify(str(error), "bad")
            return

        self.mode_blurb.setText(MODE_BLURB[mode])
        if tag_len:
            self._set_badge(self.badge_auth, "Authenticated", "ok")
        else:
            self._set_badge(self.badge_auth, "No integrity check", "warn")
        self._set_badge(self.badge_iv, f"IV {iv_len} B" if iv_len else "No IV",
                        "info" if iv_len else "muted")
        self.badge_tag.setVisible(bool(tag_len))
        if tag_len:
            self._set_badge(self.badge_tag, f"Tag {tag_len} B", "info")

        self.iv_edit.setEnabled(bool(iv_len))
        self.iv_button.setEnabled(bool(iv_len))
        if not iv_len:
            self.iv_edit.clear()
            self.iv_edit.setPlaceholderText("this mode does not use an IV")
        else:
            self.iv_edit.setPlaceholderText("leave blank for a random IV")
            current = "".join(self.iv_edit.text().split())
            if current:
                size = len(current) // 2
                valid = 7 <= size <= 13 if mode == "ccm" else size == iv_len
                if len(current) % 2 or not valid:
                    self.iv_edit.clear()

        self.aad_edit.setEnabled(aead)
        if not aead:
            self.aad_edit.clear()
            self.aad_edit.setPlaceholderText("this mode has no AAD")
        else:
            self.aad_edit.setPlaceholderText("optional additional authenticated data")

        if mode == "ecb":
            self.warn_box.setText(
                "ECB leaks plaintext structure. The CLI blocks payloads over 16 KiB; "
                "this GUI asks for confirmation instead.")
            self.warn_box.show()
        elif mode == "xts":
            self.warn_box.setText("XTS provides confidentiality only — no authentication.")
            self.warn_box.show()
        else:
            self.warn_box.hide()

        if not self.key_edit.text().strip():
            self._generate_key()
        else:
            self._validate_key()

    # ------------------------------------------------------------------ keys
    def _validate_key(self) -> None:
        raw = "".join(self.key_edit.text().split())
        mode = self._mode
        allowed = (32, 64) if mode == "xts" else (16, 24, 32)
        expected = " / ".join(str(n) for n in allowed)
        state, level, text = "", "muted", f"expects {expected} bytes"
        if raw:
            if any(c not in HEX_DIGITS for c in raw):
                state, level, text = "bad", "bad", "✗ key must be hexadecimal"
            elif len(raw) % 2:
                state, level, text = "bad", "bad", "✗ odd number of hex digits"
            else:
                size = len(raw) // 2
                if size in allowed:
                    bits = size * 4 if mode == "xts" else size * 8
                    level, text = "ok", f"✓ {size} bytes · AES-{bits}"
                else:
                    state, level, text = "bad", "bad", f"✗ {size} bytes — expected {expected}"
        self.key_hint.setText(text)
        set_level(self.key_hint, "level", level)
        set_level(self.key_edit, "state", state)

    def _toggle_key_visibility(self, shown: bool) -> None:
        self.key_edit.setEchoMode(
            QLineEdit.EchoMode.Normal if shown else QLineEdit.EchoMode.Password)
        self.key_show.setText("Hide" if shown else "Show")

    def _generate_key(self) -> None:
        try:
            size = 64 if self._mode == "xts" else 32
            self.key_edit.setText(self.core.random_hex(size))
            self.status(f"Generated a {size * 8}-bit key. Store it safely.", 6000)
        except LibraryError as error:
            self.notify(str(error), "bad")

    def _copy_key(self) -> None:
        key = "".join(self.key_edit.text().split())
        if not key:
            self.status("There is no key to copy.", 4000)
            return
        QGuiApplication.clipboard().setText(key)
        self.status("Key copied to the clipboard.", 4000)

    def _generate_iv(self) -> None:
        try:
            iv_len, _, _ = self.core.mode_sizes(self._mode)
            if iv_len:
                self.iv_edit.setText(self.core.random_hex(iv_len))
                self.status(f"Generated a {iv_len}-byte IV/nonce.", 4000)
        except LibraryError as error:
            self.notify(str(error), "bad")

    def _load_key_file(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Select a key file")
        if not path:
            return
        try:
            data = Path(path).read_bytes()
            text = data.decode("ascii", "ignore").strip()
            if text.lower().startswith("hex:"):
                key_hex = "".join(text[4:].split())
            else:
                key_hex = data.hex()
            self.key_edit.setText(key_hex)
            self.status(f"Loaded key material from {path}.", 6000)
        except OSError as error:
            self.notify(f"Key file: {error}", "bad")

    # ----------------------------------------------------------------- input
    def _browse_input(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Select input file")
        if path:
            self.set_input_file(path)

    def set_input_file(self, path: str) -> None:
        self.path_edit.setText(path)
        self.input_tabs.setCurrentIndex(1)

    def _path_changed(self, text: str) -> None:
        path = text.strip()
        if path and Path(path).is_file():
            try:
                size = human_size(Path(path).stat().st_size)
            except OSError:
                size = "?"
            self.drop_hint.setText(f"{Path(path).name}\n{size}")
        else:
            self.drop_hint.setText(
                "Drop a file anywhere on this window\n"
                "Encryption reads raw bytes · decryption expects an AESL1 container")

    def dragEnterEvent(self, event) -> None:  # noqa: N802 - Qt API
        if event.mimeData().hasUrls():
            event.acceptProposedAction()

    def dropEvent(self, event) -> None:  # noqa: N802 - Qt API
        for url in event.mimeData().urls():
            if url.isLocalFile() and Path(url.toLocalFile()).is_file():
                self.set_input_file(url.toLocalFile())
                self.status(f"Loaded {url.toLocalFile()}", 5000)
                event.acceptProposedAction()
                return

    def _input_source(self) -> tuple[str, object]:
        if self.input_tabs.currentIndex() == 1:
            path = self.path_edit.text().strip()
            if not path:
                raise ValueError("choose an input file first")
            return "file", path
        return "text", self.text_input.toPlainText()

    def _key_hex(self) -> str:
        key = "".join(self.key_edit.text().split())
        if not key:
            raise ValueError("a key is required")
        if any(c not in HEX_DIGITS for c in key):
            raise ValueError("the key must be hexadecimal")
        if len(key) % 2:
            raise ValueError("the key has an odd number of hex digits")
        return key

    def _iv_hex(self) -> str | None:
        iv = "".join(self.iv_edit.text().split())
        return iv or None

    def _aad_hex(self) -> str | None:
        if not self.core.mode_sizes(self._mode)[2]:
            return None
        aad = "".join(self.aad_edit.text().split())
        return aad or None

    # --------------------------------------------------------------- actions
    def _run(self, label: str, fn, on_success) -> None:
        self._set_busy(True, label)
        self.notify("")
        job = Job(fn, self)
        job.succeeded.connect(on_success)
        job.failed.connect(self._on_failed)
        job.finished.connect(lambda: self._set_busy(False))
        self._job = job
        job.start()

    def _on_failed(self, message: str) -> None:
        self.notify(message, "bad")
        self.status(f"Failed: {message}", 8000)

    def _encrypt(self) -> None:
        if self._busy:
            return
        try:
            core = self.core
            mode = self._mode
            key = self._key_hex()
            iv = self._iv_hex()
            aad = self._aad_hex()
            kind, source = self._input_source()
            size = (Path(str(source)).stat().st_size if kind == "file"
                    else len(str(source).encode("utf-8")))
            if mode == "ecb" and size > ECB_LIMIT:
                answer = QMessageBox.warning(
                    self, "ECB warning",
                    "ECB mode leaks plaintext structure and the CLI blocks payloads "
                    f"over {ECB_LIMIT} bytes.\n\nEncrypt this {size}-byte payload anyway?",
                    QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                    QMessageBox.StandardButton.No)
                if answer != QMessageBox.StandardButton.Yes:
                    return
        except (LibraryError, ValueError, OSError) as error:
            self._on_failed(str(error))
            return

        def job():
            data = Path(str(source)).read_bytes() if kind == "file" else str(source).encode("utf-8")
            record = core.encrypt(mode, key, iv, aad, data)
            return {"mode": mode, "in": len(data), "record": record,
                    "info": core.record_info(record)}

        self._run("Encrypting…", job, self._encrypt_done)

    def _encrypt_done(self, result: dict) -> None:
        record: bytes = result["record"]
        info = result["info"]
        mode = result["mode"]
        self._set_output(record, cipher=True, text=True)
        self._show_meta(info, f"{human_size(result['in'])} → {human_size(len(record))} container")
        if info["tag_len"]:
            self.notify("Encrypted and authenticated successfully.", "ok")
        else:
            self.notify("Encrypted. This mode has no integrity protection — "
                        "tampering will not be detected.", "warn")
        self.status(f"Encrypted {result['in']} B → {len(record)} B ({mode.upper()}).", 8000)
        # Never leave a used IV in the field: the next encryption must get
        # a fresh random nonce even though the GUI bypasses the CLI registry.
        self.iv_edit.clear()

    def _decrypt(self) -> None:
        if self._busy:
            return
        try:
            core = self.core
            key = self._key_hex()
            kind, source = self._input_source()
        except (LibraryError, ValueError, OSError) as error:
            self._on_failed(str(error))
            return

        def job():
            blob = (read_container_file(str(source)) if kind == "file"
                    else decode_container_text(str(source)))
            info = core.record_info(blob)
            mode = str(info["mode"])
            # The AESL1 header is authoritative on decrypt: never let a stale IV
            # or AAD entry from an earlier encryption override the container.
            plaintext = core.decrypt(mode, key, None, None, blob)
            return {"mode": mode, "blob": len(blob), "plain": plaintext, "info": info}

        self._run("Decrypting…", job, self._decrypt_done)

    def _decrypt_done(self, result: dict) -> None:
        mode = result["mode"]
        info = result["info"]
        plaintext: bytes = result["plain"]
        if mode in self.mode_buttons and mode != self._mode:
            self.set_mode(mode)
        self._set_output(plaintext, cipher=False, text=True)
        self._show_meta(info, f"{human_size(result['blob'])} → {human_size(len(plaintext))} plaintext")
        if info["tag_len"]:
            self.notify("Decrypted and the authentication tag verified.", "ok")
        else:
            self.notify("Decrypted. This mode is unauthenticated, so a wrong key "
                        "or altered data can yield garbage without any error.", "warn")
        self.status(f"Decrypted {result['blob']} B → {len(plaintext)} B ({mode.upper()}).", 8000)

    def _clear(self) -> None:
        if self._busy:
            return
        self.text_input.clear()
        self.path_edit.clear()
        self.iv_edit.clear()
        self.aad_edit.clear()
        self._set_output(b"", cipher=True, text=True)
        self.meta_label.setText("")
        self.notify("")
        self.status("Cleared.", 4000)

    # ---------------------------------------------------------------- output
    def _show_meta(self, info: dict, summary: str) -> None:
        parts = [
            summary,
            f"mode {str(info['mode']).upper()}",
            f"IV {info['iv_hex'] or '(none)'}",
            f"tag {info['tag_len']} B",
            f"AAD {info['aad_len']} B",
        ]
        self.meta_label.setText("  ·  ".join(parts))

    def _set_output(self, data: bytes, cipher: bool, text: bool) -> None:
        self._output_bytes = data
        self._output_is_cipher = cipher
        self._output_is_text = text
        self._refresh_output()

    def _refresh_output(self, *_args) -> None:
        data = self._output_bytes
        truncated = len(data) > PREVIEW_LIMIT
        view = data[:PREVIEW_LIMIT] if truncated else data
        note = ""

        if not data:
            text = ""
            kind = "empty"
        elif self._output_is_cipher:
            kind = "ciphertext container"
            text = (base64.b64encode(view).decode("ascii") if self.radio_b64.isChecked()
                    else view.hex())
        else:
            try:
                decoder = codecs.getincrementaldecoder("utf-8")(errors="strict")
                text = decoder.decode(view, final=not truncated)
                kind = "plaintext (UTF-8)"
            except UnicodeDecodeError:
                text = view.hex()
                kind = "plaintext (binary, shown as hex)"
                note = "Binary data detected — Save to keep the exact bytes."
        if truncated:
            text += (f"\n… preview limited to {human_size(PREVIEW_LIMIT)}; "
                     f"all {human_size(len(data))} are kept for Save.")
        self.output_kind.setText(kind)
        self.output_text.setPlainText(text)
        self.radio_hex.setEnabled(self._output_is_cipher)
        self.radio_b64.setEnabled(self._output_is_cipher)
        self._update_output_buttons()
        if note:
            self.notify(note, "info")

    def _update_output_buttons(self) -> None:
        has = bool(self._output_bytes)
        for button in (self.copy_button, self.save_button, self.reuse_button):
            button.setEnabled(has)

    def _copy_output(self) -> None:
        QGuiApplication.clipboard().setText(self.output_text.toPlainText())
        if len(self._output_bytes) > PREVIEW_LIMIT:
            self.status(f"Copied the preview only; use Save for all "
                        f"{human_size(len(self._output_bytes))}.", 6000)
        else:
            self.status("Output copied to the clipboard.", 4000)

    def _use_as_input(self) -> None:
        data = self._output_bytes
        if not data:
            return
        if len(data) > PREVIEW_LIMIT:
            self.notify("Output is too large to move into the text box — Save it "
                        "and load it from the File tab.", "warn")
            return
        if self._output_is_cipher:
            text = (base64.b64encode(data).decode("ascii") if self.radio_b64.isChecked()
                    else data.hex())
        else:
            try:
                text = data.decode("utf-8")
            except UnicodeDecodeError:
                self.notify("Binary plaintext can't be placed in the text box — Save it instead.",
                            "warn")
                return
        self.text_input.setPlainText(text)
        self.input_tabs.setCurrentIndex(0)
        self.status("Output moved to the input box.", 4000)

    def _save_output(self) -> None:
        data = self._output_bytes
        if not data:
            return
        if self._output_is_cipher:
            name, filters = "ciphertext.bin", "AESL1 container (*.bin);;All files (*)"
        else:
            name, filters = "plaintext.bin", "All files (*)"
        path, _ = QFileDialog.getSaveFileName(self, "Save output", name, filters)
        if not path:
            return
        try:
            Path(path).write_bytes(data)
            self.status(f"Saved {human_size(len(data))} to {path}.", 8000)
            self.notify(f"Saved to {path}", "ok")
        except OSError as error:
            self.notify(f"Save failed: {error}", "bad")

    # ----------------------------------------------------------------- close
    def closeEvent(self, event) -> None:  # noqa: N802 - Qt API
        self.settings.setValue("geometry", self.saveGeometry())
        if self._job is not None and self._job.isRunning():
            self._job.wait(5000)
        super().closeEvent(event)


# --------------------------------------------------------------------------
# Self-test and entry point
# --------------------------------------------------------------------------
def run_self_test(core: AesCore) -> None:
    key = core.random_hex(32)
    gcm_record = b""

    for mode in MODES:
        data = b"hello, GUI \xe2\x9c\x93 " + bytes(range(32))
        aad = "aabbcc" if core.mode_sizes(mode)[2] else None
        record = core.encrypt(mode, key, None, aad, data)
        info = core.record_info(record)
        if info["mode"] != mode:
            raise AssertionError(f"record mode {info['mode']} != {mode}")
        if core.decrypt(mode, key, None, None, record) != data:
            raise AssertionError(f"round-trip failed for {mode}")
        if mode == "gcm":
            gcm_record = record

    try:
        core.decrypt("cbc", key, None, None, gcm_record)
    except LibraryError:
        pass
    else:
        raise AssertionError("mode mismatch was not rejected")

    try:
        core.decrypt("gcm", core.random_hex(32), None, None, gcm_record)
    except LibraryError:
        pass
    else:
        raise AssertionError("wrong key was not rejected")

    print(f"self-test OK: {core.path}")
    print("  modes tested: " + " ".join(m.upper() for m in MODES))
    print("  round-trip, header mode check and wrong-key rejection passed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="aestool PyQt6 GUI")
    parser.add_argument("--library", help="path to aescore.dll / libaescore.so")
    parser.add_argument("--self-test", action="store_true",
                        help="exercise the C ABI without opening a window")
    args = parser.parse_args(argv)

    try:
        core = AesCore(find_library(args.library))
    except (LibraryError, OSError) as error:
        print(error, file=sys.stderr)
        return 1

    if args.self_test:
        try:
            run_self_test(core)
        except (LibraryError, AssertionError) as error:
            print(f"self-test FAILED: {error}", file=sys.stderr)
            return 1
        return 0

    app = QApplication(sys.argv[:1])
    app.setStyle("Fusion")
    window = AesWindow(core)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())