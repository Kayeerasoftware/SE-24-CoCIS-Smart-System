#!/usr/bin/env python3
"""
CoCIS Smart System - Real Crystal-Clear Audio Companion & Wokwi Bridge
======================================================================
This companion listens to the ESP32 Wokwi simulation (over RFC2217 port 4000)
and plays the REAL crystal-clear studio MP3/WAV audio files directly through
your PC speakers whenever access events trigger:
  - Access Granted: Plays 'Acess_granted.mp3' (crystal clear voice, no alarm)
  - Access Denied:  Plays 'Access_denaid.mp3' (crystal clear voice + alarm)
"""

import os
import sys
import time
import socket
import threading

# Windows native sound player
try:
    import winsound
    HAS_WINSOUND = True
except ImportError:
    HAS_WINSOUND = False

# Soundfile fallback for conversion if needed
try:
    import soundfile as sf
    HAS_SOUNDFILE = True
except ImportError:
    HAS_SOUNDFILE = False

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
FILE_GRANTED_WAV = os.path.join(SCRIPT_DIR, "access_granted.wav")
FILE_DENIED_WAV  = os.path.join(SCRIPT_DIR, "access_denied.wav")
FILE_GRANTED_MP3 = os.path.join(SCRIPT_DIR, "Acess_granted.mp3")
FILE_DENIED_MP3  = os.path.join(SCRIPT_DIR, "Access_denaid.mp3")

WOKWI_HOST = "127.0.0.1"
WOKWI_PORT = 4000

def prepare_audio_files():
    """Ensure high-fidelity uncompressed WAV files exist for zero-latency Windows playback."""
    if (not os.path.exists(FILE_GRANTED_WAV) or not os.path.exists(FILE_DENIED_WAV)) and HAS_SOUNDFILE:
        print("[AUDIO] Generating studio WAV files from original audio clips...")
        if os.path.exists(FILE_GRANTED_MP3) and not os.path.exists(FILE_GRANTED_WAV):
            data, samplerate = sf.read(FILE_GRANTED_MP3)
            sf.write(FILE_GRANTED_WAV, data, samplerate)
            print(f"[AUDIO] Created {os.path.basename(FILE_GRANTED_WAV)} ({samplerate} Hz)")
        if os.path.exists(FILE_DENIED_MP3) and not os.path.exists(FILE_DENIED_WAV):
            data, samplerate = sf.read(FILE_DENIED_MP3)
            sf.write(FILE_DENIED_WAV, data, samplerate)
            print(f"[AUDIO] Created {os.path.basename(FILE_DENIED_WAV)} ({samplerate} Hz)")

s_last_played_time = 0

def play_sound(filepath):
    """Play crystal-clear audio through PC speakers asynchronously without stutter or overlap."""
    global s_last_played_time
    now = time.time()
    if now - s_last_played_time < 0.8:
        return
    s_last_played_time = now

    if not os.path.exists(filepath):
        print(f"[AUDIO ERROR] File not found: {filepath}")
        return
    if HAS_WINSOUND:
        winsound.PlaySound(filepath, winsound.SND_FILENAME | winsound.SND_ASYNC)
    else:
        # Cross-platform fallback
        if sys.platform == "darwin":
            os.system(f"afplay '{filepath}' &")
        elif sys.platform.startswith("linux"):
            os.system(f"aplay '{filepath}' &")

def on_access_granted():
    print("\n\033[1;32m" + "="*60)
    print(" >>> [AUDIO EVENT]: ACCESS GRANTED")
    print(" >>> Playing: 'Access granted' (Crystal-Clear Studio Voice - NO ALARM)")
    print("="*60 + "\033[0m\n")
    play_sound(FILE_GRANTED_WAV)

def on_access_denied():
    print("\n\033[1;31m" + "="*60)
    print(" !!! [AUDIO EVENT]: ACCESS DENIED")
    print(" !!! Playing: 'Access denied' (Crystal-Clear Studio Voice + ALARM)")
    print("="*60 + "\033[0m\n")
    play_sound(FILE_DENIED_WAV)

class WokwiBridge(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.sock = None
        self.running = True

    def run(self):
        while self.running:
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.settimeout(2.0)
                self.sock.connect((WOKWI_HOST, WOKWI_PORT))
                print(f"\033[1;32m[CONNECTED] Linked to Wokwi simulation at {WOKWI_HOST}:{WOKWI_PORT}\033[0m")
                print("Listening for real-time audio triggers...\n")
                
                buffer = ""
                while self.running:
                    try:
                        data = self.sock.recv(1024)
                        if not data:
                            break
                        # Filter out telnet IAC control sequences
                        text = data.decode("utf-8", errors="ignore")
                        buffer += text
                        
                        while "\n" in buffer:
                            line, buffer = buffer.split("\n", 1)
                            line = line.strip()
                            if "[AUDIO_TRIGGER:ACCESS_GRANTED]" in line:
                                on_access_granted()
                            elif "[AUDIO_TRIGGER:ACCESS_DENIED]" in line:
                                on_access_denied()
                    except socket.timeout:
                        continue
            except (ConnectionRefusedError, OSError):
                # Wokwi is not running yet; wait and retry
                time.sleep(2.0)
            finally:
                if self.sock:
                    try:
                        self.sock.close()
                    except:
                        pass
                    self.sock = None

    def send_command(self, cmd_str):
        if self.sock:
            try:
                self.sock.sendall((cmd_str + "\r\n").encode("utf-8"))
                return True
            except:
                return False
        return False

def interactive_menu(bridge):
    print("\033[1;36m" + "="*68)
    print("      CoCIS SMART SYSTEM - REAL CRYSTAL-CLEAR AUDIO CONTROLLER      ")
    print("="*68 + "\033[0m")
    print("  Status: Listening on port 4000 (Auto-connect to Wokwi)")
    print("\n  Quick Controls:")
    print("    [1] or [G] -> Trigger ACCESS GRANTED  (Real Voice 'Access granted')")
    print("    [2] or [D] -> Trigger ACCESS DENIED   (Real Voice 'Access denied')")
    print("    [T]        -> Test PC Sound Output    (Plays both audio files now)")
    print("    [Q]        -> Exit Companion")
    print("\033[1;36m" + "="*68 + "\033[0m\n")

    while True:
        try:
            choice = input("Select Action [1/2/T/Q] > ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break

        if choice in ["1", "g", "grant"]:
            bridge.send_command("tap in TAG_1001")
            on_access_granted()
        elif choice in ["2", "d", "deny"]:
            bridge.send_command("tap in UNKNOWN_TAG")
            on_access_denied()
        elif choice in ["t", "test"]:
            print("[TEST] Playing 'Access granted'...")
            on_access_granted()
            time.sleep(3.0)
            print("[TEST] Playing 'Access denied'...")
            on_access_denied()
        elif choice in ["q", "exit", "quit"]:
            print("Exiting Audio Companion.")
            break
        else:
            if bridge.send_command(choice):
                print(f"[SENT TO WOKWI] {choice}")

def main():
    prepare_audio_files()
    bridge = WokwiBridge()
    bridge.start()

    # If --daemon flag passed, run in headless background mode
    if len(sys.argv) > 1 and sys.argv[1] == "--daemon":
        print("[AUDIO DAEMON] Running in background. Press Ctrl+C to terminate.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        interactive_menu(bridge)

if __name__ == "__main__":
    main()

