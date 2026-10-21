#!/usr/bin/env python3
"""Minimal cross-platform GUI for the aestool crypto core.

The GUI is deliberately thin: every cryptographic operation and every randomness
request goes through the compiled shared library (aescore.dll / libaescore.so)
via the C ABI in src/aesgui.h. Tkinter ships with CPython, so no extra Python
packages are required.

Build the shared library first (from lab01/):

    Windows: gui\\build_gui.bat
    Linux:   ./gui/build_gui.sh

Then run:

    python gui/aestool_gui.py
    python3 gui/aestool_gui.py
"""

from __future__ import annotations

import argparse
import base64
import ctypes
import os
import sys
from ctypes import POINTER, byref, c_char_p, c_int, c_size_t, c_ubyte, c_void_p
from pathlib import Path

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from tkinter.scrolledtext import ScrolledText
except ImportError as error:  # pragma: no cover - depends on the host Python
    print(f"Tkinter is required for the GUI: {error}", file=sys.stderr)
    print("Linux: sudo apt install python3-tk", file=sys.stderr)
    raise SystemExit(1)

ROOT = Path(__file__).resolve().parent.parent
MODES = ("ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm")
ECB_LIMIT = 16 * 1024
PREVIEW_LIMIT = 256 * 1024

U8P = POINTER(c_ubyte)


class LibraryError(RuntimeError):
    """Raised when the compiled library reports an error."""


def _library_candidates(explicit: str | None):
    if explicit:
        yield Path(explicit)
    env = os.environ.get("AESGUI_LIBRARY")
    if env:
        yield Path(env)

    if sys.platform == "win32":
        names = ("aescore.dll",)
        directories = (
            ROOT / "build_gui" / "Release",
            ROOT / "build_gui",
            ROOT / "build_windows_msvc" / "Release",
            ROOT / "build_windows_msvc",
            ROOT / "build" / "Release",
            ROOT / "build",
        )
    else:
        names = ("libaescore.so", "aescore.so")
        directories = (
            ROOT / "build_gui",
            ROOT / "build_ubuntu_gcc_shared",
            ROOT / "build_ubuntu_gcc_shared" / "Release",
            ROOT / "build_ubuntu_gcc",
            ROOT / "build_ubuntu_clang",
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
        iv_len = c_int()
        tag_len = c_int()
        aead = c_int()
        self._check(self.lib.aesgui_mode_sizes(
            mode.encode("ascii"), byref(iv_len), byref(tag_len), byref(aead)))
        return iv_len.value, tag_len.value, bool(aead.value)

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


class AesGui(ttk.Frame):
    def __init__(self, master: tk.Tk, core: AesCore):
        super().__init__(master, padding=8)
        self.core = core
        self.last_output: bytes | None = None
        self.last_output_kind = "ciphertext"
        self.last_record: bytes | None = None
        self.pack(fill="both", expand=True)

        master.title("aestool — AES symmetric encryption GUI")
        master.geometry("1000x780")
        master.minsize(820, 640)

        self._build_settings()
        self._build_input()
        self._build_actions()
        self._build_output()
        self._build_status()

        self.mode_var.trace_add("write", lambda *_: self._mode_changed())
        self.format_var.trace_add("write", lambda *_: self._refresh_output_view())
        self._mode_changed()

    # ------------------------------------------------------------------ UI
    def _build_settings(self) -> None:
        frame = ttk.LabelFrame(self, text="Key and parameters", padding=8)
        frame.pack(fill="x", pady=(0, 6))
        frame.columnconfigure(1, weight=1)

        ttk.Label(frame, text="Mode").grid(row=0, column=0, sticky="w")
        self.mode_var = tk.StringVar(value="gcm")
        self.mode_box = ttk.Combobox(
            frame, textvariable=self.mode_var, values=[m.upper() for m in MODES],
            state="readonly", width=6,
        )
        self.mode_box.grid(row=0, column=1, sticky="w", padx=(6, 6))
        self.mode_hint = ttk.Label(frame, text="")
        self.mode_hint.grid(row=0, column=2, columnspan=3, sticky="w")

        ttk.Label(frame, text="Key (hex)").grid(row=1, column=0, sticky="w", pady=3)
        self.key_var = tk.StringVar()
        self.key_entry = ttk.Entry(frame, textvariable=self.key_var, font=("Consolas", 10))
        self.key_entry.grid(row=1, column=1, columnspan=3, sticky="ew", padx=(6, 6))
        ttk.Button(frame, text="Generate", command=self._generate_key).grid(row=1, column=4)
        ttk.Button(frame, text="Load file…", command=self._load_key_file).grid(
            row=1, column=5, padx=(6, 0))

        ttk.Label(frame, text="IV / nonce (encrypt)").grid(row=2, column=0, sticky="w", pady=3)
        self.iv_var = tk.StringVar()
        self.iv_entry = ttk.Entry(frame, textvariable=self.iv_var, font=("Consolas", 10))
        self.iv_entry.grid(row=2, column=1, columnspan=3, sticky="ew", padx=(6, 6))
        self.iv_button = ttk.Button(frame, text="Generate", command=self._generate_iv)
        self.iv_button.grid(row=2, column=4)
        ttk.Label(frame, text="blank = random", foreground="#555").grid(
            row=2, column=5, padx=(6, 0))

        ttk.Label(frame, text="AAD (encrypt, AEAD)").grid(row=3, column=0, sticky="w", pady=3)
        self.aad_var = tk.StringVar()
        self.aad_entry = ttk.Entry(frame, textvariable=self.aad_var, font=("Consolas", 10))
        self.aad_entry.grid(row=3, column=1, columnspan=5, sticky="ew", padx=(6, 0))

        self.warning_var = tk.StringVar(value="")
        self.warning_label = ttk.Label(
            frame, textvariable=self.warning_var, foreground="#a33")
        self.warning_label.grid(row=4, column=0, columnspan=6, sticky="w", pady=(4, 0))

    def _build_input(self) -> None:
        frame = ttk.LabelFrame(self, text="Input", padding=6)
        frame.pack(fill="both", expand=True, pady=(0, 6))
        self.input_tabs = ttk.Notebook(frame)
        self.input_tabs.pack(fill="both", expand=True)

        text_tab = ttk.Frame(self.input_tabs)
        self.input_tabs.add(text_tab, text="Text (UTF-8)")
        self.text_input = ScrolledText(text_tab, height=8, wrap="word",
                                       font=("Consolas", 10), undo=True)
        self.text_input.pack(fill="both", expand=True)
        ttk.Label(text_tab, foreground="#555",
                  text="For decryption you can also paste the container as hex or base64.").pack(
            anchor="w", pady=(4, 0))

        file_tab = ttk.Frame(self.input_tabs)
        self.input_tabs.add(file_tab, text="File")
        row = ttk.Frame(file_tab)
        row.pack(fill="x", pady=(2, 0))
        ttk.Label(row, text="Path").pack(side="left")
        self.input_path_var = tk.StringVar()
        ttk.Entry(row, textvariable=self.input_path_var, font=("Consolas", 10)).pack(
            side="left", fill="x", expand=True, padx=6)
        ttk.Button(row, text="Browse…", command=self._browse_input).pack(side="left")
        ttk.Label(file_tab, foreground="#555",
                  text="Encryption reads raw bytes; decryption expects an AESL1 container.").pack(
            anchor="w", pady=(6, 0))

    def _build_actions(self) -> None:
        bar = ttk.Frame(self)
        bar.pack(fill="x", pady=(0, 6))
        ttk.Button(bar, text="Encrypt", command=self._encrypt).pack(side="left")
        ttk.Button(bar, text="Decrypt", command=self._decrypt).pack(side="left", padx=6)
        ttk.Button(bar, text="Clear", command=self._clear).pack(side="left")
        ttk.Button(bar, text="Copy output", command=self._copy_output).pack(side="right")
        ttk.Button(bar, text="Save output…", command=self._save_output).pack(
            side="right", padx=6)

    def _build_output(self) -> None:
        frame = ttk.LabelFrame(self, text="Output", padding=6)
        frame.pack(fill="both", expand=True, pady=(0, 6))

        row = ttk.Frame(frame)
        row.pack(fill="x")
        ttk.Label(row, text="Ciphertext format").pack(side="left")
        self.format_var = tk.StringVar(value="hex")
        ttk.Radiobutton(row, text="Hex", value="hex", variable=self.format_var).pack(
            side="left", padx=(6, 0))
        ttk.Radiobutton(row, text="Base64", value="base64", variable=self.format_var).pack(
            side="left", padx=(6, 0))
        self.output_kind = ttk.Label(row, text="ciphertext", foreground="#555")
        self.output_kind.pack(side="right")

        self.output_text = ScrolledText(frame, height=10, wrap="char",
                                        font=("Consolas", 10), state="disabled")
        self.output_text.pack(fill="both", expand=True, pady=(6, 0))

    def _build_status(self) -> None:
        self.status_var = tk.StringVar(value="Ready.")
        ttk.Label(self, textvariable=self.status_var, relief="sunken", anchor="w").pack(
            fill="x")

    # ------------------------------------------------------------- helpers
    def _mode(self) -> str:
        return self.mode_var.get().lower()

    def _mode_changed(self) -> None:
        mode = self._mode()
        iv_len, tag_len, aead = self.core.mode_sizes(mode)
        key_hint = "32/64" if mode == "xts" else "16/24/32"
        details = f"key {key_hint} B"
        details += f" · IV {iv_len} B" if iv_len else " · no IV"
        if tag_len:
            details += f" · tag {tag_len} B · AAD enabled"
        self.mode_hint.configure(text=details)

        state = "normal" if iv_len else "disabled"
        self.iv_entry.configure(state=state)
        self.iv_button.configure(state=state)
        if not iv_len:
            self.iv_var.set("")
        else:
            current_iv = "".join(self.iv_var.get().split())
            if current_iv:
                size = len(current_iv) // 2
                valid = 7 <= size <= 13 if mode == "ccm" else size == iv_len
                if len(current_iv) % 2 or not valid:
                    self.iv_var.set("")
        self.aad_entry.configure(state="normal" if aead else "disabled")
        if not aead:
            self.aad_var.set("")

        if mode == "ecb":
            self.warning_var.set(
                "ECB leaks plaintext structure. The CLI blocks payloads over 16 KiB; "
                "this GUI asks for confirmation instead.")
        elif mode == "xts":
            self.warning_var.set("XTS provides confidentiality only — no authentication.")
        else:
            self.warning_var.set("")

        current = self.key_var.get().strip()
        if not current:
            self._generate_key()

    def _generate_key(self) -> None:
        bytes_needed = 64 if self._mode() == "xts" else 32
        self.key_var.set(self.core.random_hex(bytes_needed))
        self.status_var.set(
            f"Generated a {bytes_needed * 8}-bit key (hex). Store it safely.")

    def _generate_iv(self) -> None:
        mode = self._mode()
        iv_len, _, _ = self.core.mode_sizes(mode)
        if iv_len:
            self.iv_var.set(self.core.random_hex(iv_len))
            self.status_var.set(f"Generated a {iv_len}-byte IV/nonce.")
        else:
            self.status_var.set(f"{mode.upper()} does not use an IV/nonce.")

    def _load_key_file(self) -> None:
        path = filedialog.askopenfilename(title="Select a key file")
        if not path:
            return
        try:
            data = Path(path).read_bytes()
            text = data.decode("ascii", "ignore").strip()
            if text.lower().startswith("hex:"):
                key_hex = "".join(text[4:].split())
            else:
                key_hex = data.hex()
            self.key_var.set(key_hex)
            self.status_var.set(f"Loaded key material from {path}.")
        except OSError as error:
            messagebox.showerror("Key file", str(error), parent=self)

    def _browse_input(self) -> None:
        path = filedialog.askopenfilename(title="Select input file")
        if path:
            self.input_path_var.set(path)
            self.input_tabs.select(1)

    def _read_input(self, decrypting: bool) -> bytes:
        if self.input_tabs.index("current") == 1:
            path = self.input_path_var.get().strip()
            if not path:
                raise ValueError("choose an input file first")
            if decrypting:
                return read_container_file(path)
            return Path(path).read_bytes()

        text = self.text_input.get("1.0", "end-1c")
        if decrypting:
            return decode_container_text(text)
        return text.encode("utf-8")

    def _key_hex(self) -> str:
        key = "".join(self.key_var.get().split())
        if not key:
            raise ValueError("a key is required")
        if any(c not in "0123456789abcdefABCDEF" for c in key):
            raise ValueError("the key must be hexadecimal")
        if len(key) % 2:
            raise ValueError("the key has an odd number of hex digits")
        return key

    def _iv_hex(self) -> str | None:
        iv = "".join(self.iv_var.get().split())
        return iv or None

    def _aad_hex(self) -> str | None:
        if not self.core.mode_sizes(self._mode())[2]:
            return None
        aad = "".join(self.aad_var.get().split())
        return aad or None

    # ------------------------------------------------------------- actions
    def _encrypt(self) -> None:
        try:
            mode = self._mode()
            key = self._key_hex()
            data = self._read_input(decrypting=False)
            if mode == "ecb" and len(data) > ECB_LIMIT:
                proceed = messagebox.askyesno(
                    "ECB warning",
                    "ECB mode leaks plaintext structure and the CLI blocks payloads "
                    f"over {ECB_LIMIT} bytes.\n\nEncrypt this {len(data)}-byte payload anyway?",
                    parent=self,
                )
                if not proceed:
                    return
            record = self.core.encrypt(mode, key, self._iv_hex(), self._aad_hex(), data)
            info = self.core.record_info(record)
            self.last_record = record
            self.last_output = record
            self.last_output_kind = "ciphertext"
            self._set_output(record, f"ciphertext ({mode.upper()})")
            self.status_var.set(
                f"Encrypted {len(data)} B → {len(record)} B container · "
                f"mode {str(info['mode']).upper()} · IV {info['iv_hex'] or '(none)'} · "
                f"tag {info['tag_len']} B · AAD {info['aad_len']} B"
            )
            if mode != "ecb":
                # Never leave a used IV in the field: the next encryption must get
                # a fresh random nonce even though the GUI bypasses the CLI registry.
                self.iv_var.set("")
            if mode in ("ecb", "cbc", "ofb", "cfb", "ctr", "xts"):
                self.status_var.set(self.status_var.get() + " · no integrity protection")
        except (LibraryError, ValueError, OSError) as error:
            messagebox.showerror("Encryption failed", str(error), parent=self)
            self.status_var.set(f"Encryption failed: {error}")

    def _decrypt(self) -> None:
        try:
            blob = self._read_input(decrypting=True)
            info = self.core.record_info(blob)
            mode = str(info["mode"])
            self.mode_var.set(mode.upper())
            key = self._key_hex()
            # The AESL1 header is authoritative on decrypt: never let a stale IV
            # or AAD entry from an earlier encryption override the container.
            plaintext = self.core.decrypt(mode, key, None, None, blob)
            self.last_output = plaintext
            self.last_output_kind = "plaintext"
            self._set_output(plaintext, "plaintext (UTF-8)")
            preview = plaintext.decode("utf-8", "replace")
            self.status_var.set(
                f"Decrypted {len(blob)} B → {len(plaintext)} B · mode {mode.upper()} · "
                f"tag {info['tag_len']} B · AAD {info['aad_len']} B"
            )
            if not info["tag_len"]:
                self.status_var.set(self.status_var.get() + " · unauthenticated mode")
            if preview and "\ufffd" not in preview:
                # Keep a short preview in the status bar for quick feedback.
                snippet = preview.replace("\r\n", " ").replace("\n", " ").replace("\r", " ")
                if len(snippet) > 70:
                    snippet = snippet[:67] + "…"
                self.status_var.set(self.status_var.get() + f" · “{snippet}”")
        except (LibraryError, ValueError, OSError) as error:
            messagebox.showerror("Decryption failed", str(error), parent=self)
            self.status_var.set(f"Decryption failed: {error}")

    def _clear(self) -> None:
        self.text_input.delete("1.0", "end")
        self.input_path_var.set("")
        self.iv_var.set("")
        self.aad_var.set("")
        self.last_output = None
        self.last_record = None
        self._set_output(b"", "ciphertext")
        self.status_var.set("Cleared.")

    def _set_output(self, data: bytes, kind: str) -> None:
        self._output_bytes = data
        self._output_kind_text = kind
        self.output_kind.configure(text=kind)
        self._refresh_output_view()

    def _refresh_output_view(self) -> None:
        data = getattr(self, "_output_bytes", b"")
        limit = PREVIEW_LIMIT
        truncated = len(data) > limit
        view = data[:limit] if truncated else data
        if getattr(self, "_output_kind_text", "") == "plaintext (UTF-8)":
            text = view.decode("utf-8", "replace")
        elif self.format_var.get() == "base64":
            text = base64.b64encode(view).decode("ascii")
        else:
            text = view.hex()
        if truncated:
            text += (
                f"\n… preview limited to {limit} bytes; "
                f"the full {len(data)} bytes are kept for Save output."
            )
        self.output_text.configure(state="normal")
        self.output_text.delete("1.0", "end")
        self.output_text.insert("1.0", text)
        self.output_text.configure(state="disabled")

    def _copy_output(self) -> None:
        text = self.output_text.get("1.0", "end-1c")
        self.clipboard_clear()
        self.clipboard_append(text)
        data = getattr(self, "_output_bytes", b"")
        if len(data) > PREVIEW_LIMIT:
            self.status_var.set(
                f"Copied the preview only ({PREVIEW_LIMIT} B); use Save output for "
                f"all {len(data)} bytes."
            )
        else:
            self.status_var.set("Output copied to the clipboard.")

    def _save_output(self) -> None:
        data = getattr(self, "_output_bytes", b"")
        if not data:
            messagebox.showinfo("Save output", "There is no output to save.", parent=self)
            return
        ciphertext = getattr(self, "_output_kind_text", "").startswith("ciphertext")
        if ciphertext:
            default_name = "ciphertext.bin"
            filetypes = [("AESL1 container", "*.bin"), ("All files", "*.*")]
        else:
            default_name = "plaintext.bin"
            filetypes = [("All files", "*.*")]
        path = filedialog.asksaveasfilename(
            title="Save output", initialfile=default_name, filetypes=filetypes,
            defaultextension=".bin",
        )
        if not path:
            return
        try:
            Path(path).write_bytes(data)
            self.status_var.set(f"Saved {len(data)} bytes to {path}.")
        except OSError as error:
            messagebox.showerror("Save output", str(error), parent=self)


def run_self_test(core: AesCore) -> None:
    key = core.random_hex(32)
    gcm_record = b""

    for mode in MODES:
        data = b"hello, GUI \xe2\x9c\x93 " + bytes(range(32))
        aad = None
        if core.mode_sizes(mode)[2]:
            aad = "aabbcc"
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

    bad_key = core.random_hex(32)
    try:
        core.decrypt("gcm", bad_key, None, None, gcm_record)
    except LibraryError:
        pass
    else:
        raise AssertionError("wrong key was not rejected")

    print(f"self-test OK: {core.path}")
    print("  modes tested: " + " ".join(m.upper() for m in MODES))
    print("  round-trip, header mode check and wrong-key rejection passed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="aestool Tkinter GUI")
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

    root = tk.Tk()
    AesGui(root, core)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
